/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { createHash, randomUUID } from "node:crypto";
import { createReadStream } from "node:fs";
import { constants as fsConstants } from "node:fs";
import { copyFile, lstat, mkdir, open, readFile, readdir, rename, rm } from "node:fs/promises";
import { basename, isAbsolute, join, relative, resolve } from "node:path";
import lockfile from "proper-lockfile";
import yauzl from "yauzl";
import { canonicalJsonBytes } from "./wire/canonicalJson.mjs";
import { KB_ID, SUPPORTED_PACKAGE_SCHEMA_VERSIONS, validateSharedMembers } from "./wire/packageContracts.mjs";
import {
    PACKAGE_V5_FIXED_MEMBERS,
    PACKAGE_V5_METADATA_MEMBERS,
    PACKAGE_V5_PAYLOAD_PREFIX,
    PACKAGE_V5_SUFFIX_MEMBERS,
    validateBundleChecksums,
    validateBundleClosure,
    validateBundleRecords,
    validateManifestBundle,
} from "./wire/packageBundleContracts.mjs";
import { detectOverriddenSkills, installBundleToWorkspace, stageBundleForWorkspace } from "./bundleWorkspace.mjs";
import { parseCanonicalJson, parseCanonicalJsonl } from "./wire/strictJsonParser.mjs";
import { loadEmbeddingModelContract } from "./embeddingRuntime.mjs";
import { loadKnowledgePack, readActiveKnowledgePointer, readInstallRecord } from "./knowledgePackLoader.mjs";
import { loadRuntimeContract } from "./runtimeContract.mjs";

const MAX_ARCHIVE_BYTES = 512 * 1024 * 1024;
const MAX_MEMBER_BYTES = 512 * 1024 * 1024;
const MAX_UNCOMPRESSED_BYTES = 1024 * 1024 * 1024;
const ARCHIVE_NAME = "knowledge-pack-v5.zip";
const SIDECAR_NAME = "knowledge-pack-v5.zip.sha256";
const SHA256_RE = /^[0-9a-f]{64}$/;
const ZIP_LOCAL_SIGNATURE = 0x04034b50;
const ZIP_EOCD_SIGNATURE = 0x06054b50;
const ZIP_UNIX_REGULAR_0644 = 0o100644;
const ZIP_UNIX_REGULAR_0755 = 0o100755;
const ZIP_VERSION_MADE_BY = 0x0314;
const ZIP_VERSION_NEEDED = 20;
const ZIP_DOS_DATE_1980_01_01 = 33;

export class KnowledgePackageError extends Error {
    constructor(code, message, options) {
        super(message, options);
        this.name = "KnowledgePackageError";
        this.code = code;
    }
}

export const createKnowledgePackageService = ({
    ragDataDir,
    modelDir,
    runtimeDir,
    loadModelContract = loadEmbeddingModelContract,
    loadRuntime = loadRuntimeContract,
    validateArchive = validatePackageArchive,
    loadInstalledPack = loadKnowledgePack,
    tempId = randomUUID,
    logger = console,
} = {}) => {
    const root = resolveRequiredPath(ragDataDir, "RAG data directory");
    const modelDirectory = resolveRequiredPath(modelDir, "RAG model directory");
    const runtimeDirectory = resolveRequiredPath(runtimeDir, "RAG runtime directory");

    const dependencies = async () => {
        const [{ manifest: modelContract }, loadedRuntime] = await Promise.all([
            loadModelContract({ modelDir: modelDirectory }),
            loadRuntime(runtimeDirectory),
        ]);
        return { modelContract, runtimeContract: loadedRuntime.contract ?? loadedRuntime };
    };

    const getStatus = async () => {
        try {
            if (!await regularPathExists(join(root, "active.json"))) return { active: null, previous: null };
            const { pointer, kbDir } = await readActiveKnowledgePointer(root);
            const install = await readInstallRecord(kbDir);
            return {
                active: pointer.active,
                previous: pointer.previous,
                installMode: install.installMode,
            };
        } catch (error) {
            return { active: null, previous: null, error: normalizeError(error).code };
        }
    };

    const importPackageUnlocked = async (packPath, {
        mode,
        sidecarPath = `${packPath}.sha256`,
    } = {}) => {
        const installMode = installModeForImportMode(mode);
        if (!installMode) {
            throw new KnowledgePackageError(
                "configuration_error",
                "RAG import mode is invalid",
            );
        }
        const sourceArchive = await requireSourceFile(packPath, ARCHIVE_NAME, "pack_missing", "pack_unreadable");
        const sourceSidecar = await requireSourceFile(sidecarPath, SIDECAR_NAME, "sidecar_missing", "sidecar_invalid");
        const names = archiveNamesForHandoff(packPath, sidecarPath);
        const { modelContract, runtimeContract } = await dependencies();
        const transaction = safeChildDirectory(join(root, ".staging"), sanitizeId(tempId()));
        const packageStage = join(transaction, "package");
        const stagedArchive = join(transaction, names.archive);
        const stagedSidecar = join(transaction, names.sidecar);
        await ensurePrivateDirectory(join(root, ".staging"));
        await mkdir(transaction, { recursive: false, mode: 0o700 });
        try {
            await copyFile(sourceArchive, stagedArchive, fsConstants.COPYFILE_EXCL);
            await copyFile(sourceSidecar, stagedSidecar, fsConstants.COPYFILE_EXCL);
            await fsyncFile(stagedArchive);
            await fsyncFile(stagedSidecar);
            const validated = await validateArchive({
                archivePath: stagedArchive,
                sidecarPath: stagedSidecar,
                modelContract,
                runtimeContract,
            });
            const version = validated.pack.manifest.kbVersion;
            const target = safeChildDirectory(root, version);
            const existing = await inspectExistingInstall(target);
            if (existing) {
                if (existing.package.sha256 !== validated.sha256) {
                    throw new KnowledgePackageError("version_conflict", `Knowledge package version ${version} is already installed with a different SHA-256`);
                }
                await loadInstalledAndVerify(target, existing, { modelContract, runtimeContract, validateArchive, loadInstalledPack });
                return importResult("already_imported", validated, existing.installMode);
            }
            await mkdir(packageStage, { recursive: false, mode: 0o700 });
            const installedNames = [...PACKAGE_V5_FIXED_MEMBERS];
            for (const name of installedNames) {
                await writeDurableFile(join(packageStage, name), validated.members.get(name));
            }
            await rename(stagedArchive, join(packageStage, names.archive));
            await rename(stagedSidecar, join(packageStage, names.sidecar));
            const fileNames = [names.archive, names.sidecar, ...installedNames];
            const files = await Promise.all(
                fileNames.map((name) => fingerprintInstalledFile(packageStage, name)),
            );
            const install = {
                schemaVersion: "1.1",
                kbId: KB_ID,
                kbVersion: version,
                installMode,
                package: { sha256: validated.sha256, sizeBytes: validated.sizeBytes },
                runtimeContractSha256: validated.pack.contract.contractSha256,
                memberSha256: Object.fromEntries(
                    validated.memberNames.map((name) => [name, sha256(validated.members.get(name))]),
                ),
                files,
                bundle: validated.bundleInstallRecord,
            };
            await writeDurableFile(join(packageStage, "install.json"), canonicalJsonBytes(install));
            try {
                await rename(packageStage, target);
            } catch (error) {
                const concurrent = await inspectExistingInstall(target);
                if (!concurrent || concurrent.package.sha256 !== validated.sha256) throw error;
                await loadInstalledAndVerify(target, concurrent, { modelContract, runtimeContract, validateArchive, loadInstalledPack });
                return importResult("already_imported", validated, concurrent.installMode);
            }
            return importResult("imported", validated, installMode);
        } catch (error) {
            throw normalizeError(error);
        } finally {
            await rm(transaction, { recursive: true, force: true }).catch(() => {});
        }
    };
    const importPackage = (packPath, options) => withLifecycleLock(root, () => importPackageUnlocked(packPath, options));

    const activateUnlocked = async (kbVersion, { sha256: expectedSha256, agentWorkspacePath } = {}) => {
        const version = validateVersion(kbVersion);
        const target = safeChildDirectory(root, version);
        const install = await readInstallRecord(target).catch((error) => {
            throw new KnowledgePackageError("pack_unreadable", `Unable to activate knowledge package ${version}: ${error.message}`, { cause: error });
        });
        if (expectedSha256 !== undefined && expectedSha256 !== install.package.sha256) {
            throw new KnowledgePackageError("package_sha256_mismatch", "Activation SHA-256 does not match the installed package");
        }
        const { modelContract, runtimeContract } = await dependencies();
        const pack = await loadInstalledAndVerify(target, install, { modelContract, runtimeContract, validateArchive, loadInstalledPack });
        const bundle = await installPackageBundle({
            install,
            pack,
            agentWorkspacePath,
        });
        const current = await readOptionalPointer(root);
        if (current?.active.kbVersion === version && current.active.sha256 === install.package.sha256) {
            return { status: "already_active", active: current.active, previous: current.previous, bundle };
        }
        const next = {
            schemaVersion: "4.0",
            active: pointerEntry(version, install.package.sha256),
            previous: current?.active ?? null,
        };
        await writeAtomic(root, "active.json", canonicalJsonBytes(next), sanitizeId(tempId()));
        return {
            status: "activated",
            active: next.active,
            previous: next.previous,
            chunks: pack.chunks.length,
            installMode: install.installMode,
            bundle,
        };
    };
    const activate = (kbVersion, options) => withLifecycleLock(root, () => activateUnlocked(kbVersion, options));

    const verify = () => withLifecycleLock(root, async () => {
        const { pointer, kbDir } = await readActiveKnowledgePointer(root);
        const install = await readInstallRecord(kbDir);
        assertPointerMatchesInstall(pointer.active, install);
        const { modelContract, runtimeContract } = await dependencies();
        const pack = await loadInstalledAndVerify(kbDir, install, { modelContract, runtimeContract, validateArchive, loadInstalledPack });
        return {
            status: "verified",
            kbId: install.kbId,
            kbVersion: pack.manifest.kbVersion,
            sha256: install.package.sha256,
            chunks: pack.chunks.length,
            installMode: install.installMode,
        };
    });

    const rollback = () => withLifecycleLock(root, async () => {
        const current = await readOptionalPointer(root);
        if (!current?.previous) throw new KnowledgePackageError("rollback_unavailable", "No previous RAG package is available for rollback");
        const target = safeChildDirectory(root, current.previous.directory);
        const install = await readInstallRecord(target);
        assertPointerMatchesInstall(current.previous, install);
        const { modelContract, runtimeContract } = await dependencies();
        const pack = await loadInstalledAndVerify(target, install, { modelContract, runtimeContract, validateArchive, loadInstalledPack });
        const next = {
            schemaVersion: "4.0",
            active: current.previous,
            previous: current.active,
        };
        await writeAtomic(root, "active.json", canonicalJsonBytes(next), sanitizeId(tempId()));
        return {
            status: "rolled_back",
            active: next.active,
            previous: next.previous,
            chunks: pack.chunks.length,
            installMode: install.installMode,
        };
    });

    return { activate, getStatus, importPackage, rollback, verify };
};

export const validatePackageArchive = async ({ archivePath, sidecarPath, modelContract, runtimeContract }) => {
    const archive = await requireRegularFile(archivePath, "pack_unreadable", "Knowledge package ZIP");
    const sidecar = await requireRegularFile(sidecarPath, "sidecar_invalid", "Knowledge package SHA-256 sidecar");
    const archiveBase = basename(archive);
    const sidecarBase = basename(sidecar);
    if (archiveBase !== ARCHIVE_NAME || sidecarBase !== SIDECAR_NAME) {
        throw new KnowledgePackageError("sidecar_invalid", `Knowledge package handoff files must be named ${ARCHIVE_NAME} and ${SIDECAR_NAME}`);
    }
    const sizeBytes = (await lstat(archive)).size;
    if (sizeBytes <= 0 || sizeBytes > MAX_ARCHIVE_BYTES) throw new KnowledgePackageError("archive_too_large", "Knowledge package ZIP is outside the permitted size range");
    const expectedSha256 = parseSidecar(await readFile(sidecar), ARCHIVE_NAME);
    const actualSha256 = await sha256File(archive);
    if (actualSha256 !== expectedSha256) throw new KnowledgePackageError("package_sha256_mismatch", "Knowledge package ZIP does not match its SHA-256 sidecar");
    return validatePackageArchiveV5({ archive, sidecar, sizeBytes, modelContract, runtimeContract });
};

const validatePackageArchiveV5 = async ({ archive, sidecar, sizeBytes, modelContract, runtimeContract }) => {
    const { entries, members } = await readArchiveV5(archive, sizeBytes);
    await validateLocalHeadersV5(archive, sizeBytes, entries);
    let pack;
    try {
        pack = validatePackageV5MemberBytes({ members, runtimeContract, modelContract });
    } catch (error) {
        throw normalizeError(error);
    }
    const memberNames = entries.map(({ fileName }) => fileName);
    return {
        archivePath: archive,
        sidecarPath: sidecar,
        sha256: await sha256File(archive),
        sizeBytes,
        members,
        memberNames,
        pack,
        bundleInstallRecord: pack.bundleInstallRecord,
    };
};

const readArchiveV5 = (archive, sizeBytes) => new Promise((resolvePromise, rejectPromise) => {
    yauzl.open(archive, {
        autoClose: true,
        decodeStrings: true,
        lazyEntries: true,
        strictFileNames: true,
        validateEntrySizes: true,
    }, (openError, zipFile) => {
        if (openError) return rejectPromise(new KnowledgePackageError("invalid_archive", `Unable to open knowledge package ZIP: ${openError.message}`, { cause: openError }));
        if (zipFile.comment) return rejectAndClose(zipFile, rejectPromise, new KnowledgePackageError("invalid_archive", "Knowledge package ZIP comment must be empty"));
        const entries = [];
        const members = new Map();
        const names = new Set();
        const folded = new Set();
        let total = 0;
        let settled = false;
        const fail = (error) => {
            if (settled) return;
            settled = true;
            zipFile.close();
            rejectPromise(normalizeError(error));
        };
        zipFile.on("error", fail);
        zipFile.on("entry", (entry) => {
            void (async () => {
                try {
                    validateCentralEntryV5(entry, entries.length, names, folded);
                    names.add(entry.fileName);
                    folded.add(entry.fileName.toLowerCase());
                    total += entry.uncompressedSize;
                    if (total > MAX_UNCOMPRESSED_BYTES) throw new KnowledgePackageError("archive_too_large", "Knowledge package ZIP exceeds the uncompressed size limit");
                    entries.push(entry);
                    members.set(entry.fileName, await readEntry(zipFile, entry));
                    zipFile.readEntry();
                } catch (error) {
                    fail(error);
                }
            })();
        });
        zipFile.on("end", () => {
            if (settled) return;
            settled = true;
            try {
                assertV5MemberPlan([...members.keys()]);
            } catch (error) {
                rejectPromise(normalizeError(error));
                return;
            }
            if (entries.length !== members.size || sizeBytes !== zipFile.fileSize) {
                rejectPromise(new KnowledgePackageError("invalid_archive", "Knowledge package ZIP directory is incomplete"));
                return;
            }
            resolvePromise({ entries, members });
        });
        zipFile.readEntry();
    });
});

const validateCentralEntryV5 = (entry, index, names, folded) => {
    const name = entry.fileName;
    if (names.has(name)) throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP contains duplicate members");
    if (folded.has(name.toLowerCase())) throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP contains case-fold-colliding members");
    if (!name || name.startsWith("/") || /[\\:\0]/.test(name) || name.split("/").some((part) => !part || part === "." || part === "..")) {
        throw new KnowledgePackageError("invalid_archive_entries", `Knowledge package ZIP member name is unsafe: ${name}`);
    }
    const fixed = PACKAGE_V5_FIXED_MEMBERS.includes(name);
    if (!fixed && (!name.startsWith(PACKAGE_V5_PAYLOAD_PREFIX) || name.endsWith("/"))) {
        throw new KnowledgePackageError("invalid_archive_entries", `Knowledge package ZIP member is not a fixed member or Bundle payload file: ${name}`);
    }
    const metadataCount = PACKAGE_V5_METADATA_MEMBERS.length;
    if (index < metadataCount) {
        if (name !== PACKAGE_V5_METADATA_MEMBERS[index]) {
            throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP members do not use the fixed Package v5 order");
        }
    } else if (!fixed) {
        const previous = [...names].filter((existing) => !PACKAGE_V5_FIXED_MEMBERS.includes(existing)).pop();
        if (previous !== undefined && compareCodePoints(name, previous) <= 0) {
            throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP payload members are not sorted");
        }
    }
    if (entry.versionMadeBy !== ZIP_VERSION_MADE_BY
        || entry.versionNeededToExtract !== ZIP_VERSION_NEEDED
        || entry.generalPurposeBitFlag !== 0
        || entry.compressionMethod !== 8
        || entry.lastModFileTime !== 0
        || entry.lastModFileDate !== ZIP_DOS_DATE_1980_01_01
        || entry.internalFileAttributes !== 0
        || entry.extraFieldLength !== 0
        || entry.fileComment !== "") {
        throw new KnowledgePackageError("invalid_archive", `Knowledge package ZIP metadata is noncanonical: ${name}`);
    }
    const mode = entry.externalFileAttributes >>> 16;
    const expected = fixed ? [ZIP_UNIX_REGULAR_0644] : [ZIP_UNIX_REGULAR_0644, ZIP_UNIX_REGULAR_0755];
    if (!expected.includes(mode)) {
        throw new KnowledgePackageError("invalid_archive", `Knowledge package ZIP member mode is invalid: ${name}`);
    }
    if (entry.uncompressedSize < 0 || entry.uncompressedSize > MAX_MEMBER_BYTES) {
        throw new KnowledgePackageError("archive_too_large", `Knowledge package ZIP member exceeds the size limit: ${name}`);
    }
};

const assertV5MemberPlan = (names) => {
    // Metadata order is already enforced per entry during streaming; only the
    // total length and the trailing suffix order need a final check here.
    const suffixCount = PACKAGE_V5_SUFFIX_MEMBERS.length;
    if (names.length < PACKAGE_V5_METADATA_MEMBERS.length + suffixCount) {
        throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP members do not use the fixed Package v5 order");
    }
    const tail = names.slice(names.length - suffixCount);
    for (let index = 0; index < suffixCount; index += 1) {
        if (tail[index] !== PACKAGE_V5_SUFFIX_MEMBERS[index]) {
            throw new KnowledgePackageError("invalid_archive_entries", "Knowledge package ZIP members do not use the fixed Package v5 order");
        }
    }
};

const validateLocalHeadersV5 = async (archive, sizeBytes, entries) => {
    const handle = await open(archive, "r");
    try {
        const eocd = Buffer.alloc(22);
        await readExactly(handle, eocd, sizeBytes - eocd.length);
        if (eocd.readUInt32LE(0) !== ZIP_EOCD_SIGNATURE
            || eocd.readUInt16LE(4) !== 0
            || eocd.readUInt16LE(6) !== 0
            || eocd.readUInt16LE(8) !== entries.length
            || eocd.readUInt16LE(10) !== entries.length
            || eocd.readUInt16LE(20) !== 0) {
            throw new KnowledgePackageError("invalid_archive", "Knowledge package ZIP EOCD is noncanonical");
        }
        const centralSize = eocd.readUInt32LE(12);
        const centralOffset = eocd.readUInt32LE(16);
        if (centralOffset + centralSize + 22 !== sizeBytes || entries[0]?.relativeOffsetOfLocalHeader !== 0) {
            throw new KnowledgePackageError("invalid_archive", "Knowledge package ZIP has a prefix, trailer, or invalid central directory");
        }
        for (const [index, entry] of entries.entries()) {
            const offset = entry.relativeOffsetOfLocalHeader;
            const fixed = Buffer.alloc(30);
            await readExactly(handle, fixed, offset);
            if (fixed.readUInt32LE(0) !== ZIP_LOCAL_SIGNATURE
                || fixed.readUInt16LE(4) !== ZIP_VERSION_NEEDED
                || fixed.readUInt16LE(6) !== entry.generalPurposeBitFlag
                || fixed.readUInt16LE(8) !== entry.compressionMethod
                || fixed.readUInt16LE(10) !== entry.lastModFileTime
                || fixed.readUInt16LE(12) !== entry.lastModFileDate
                || fixed.readUInt32LE(14) !== entry.crc32
                || fixed.readUInt32LE(18) !== entry.compressedSize
                || fixed.readUInt32LE(22) !== entry.uncompressedSize) {
                throw new KnowledgePackageError("invalid_archive", `ZIP local header does not match central directory: ${entry.fileName}`);
            }
            const nameLength = fixed.readUInt16LE(26);
            const extraLength = fixed.readUInt16LE(28);
            const name = Buffer.alloc(nameLength);
            await readExactly(handle, name, offset + 30);
            if (extraLength !== 0 || name.toString("utf8") !== entry.fileName || !name.equals(Buffer.from(entry.fileName, "utf8"))) {
                throw new KnowledgePackageError("invalid_archive", `ZIP local member name is noncanonical: ${entry.fileName}`);
            }
            const end = offset + 30 + nameLength + extraLength + entry.compressedSize;
            const next = entries[index + 1]?.relativeOffsetOfLocalHeader ?? centralOffset;
            if (end !== next) throw new KnowledgePackageError("invalid_archive", "Knowledge package ZIP entries overlap or contain gaps");
        }
    } finally {
        await handle.close();
    }
};

export const validatePackageV5MemberBytes = ({ members, runtimeContract, modelContract }) => {
    const manifest = parseCanonicalMember(members, "manifest.json", "manifest");
    if (manifest.schemaVersion !== "5.0") {
        throw new KnowledgePackageError("unsupported_package_schema", "Knowledge package manifest schemaVersion must be 5.0");
    }
    if (manifest.kbVersion === undefined) throw new KnowledgePackageError("package_semantics_invalid", "Knowledge package manifest kbVersion is invalid");
    const bundle = validateBundleRecords({
        skills: members.get("skills.jsonl"),
        mcps: members.get("mcps.jsonl"),
        files: members.get("bundle-files.jsonl"),
        skips: members.get("bundle-skips.jsonl"),
    });
    const checksums = parseCanonicalMember(members, "checksums.json", "checksums");
    validateBundleChecksums(checksums, members);
    const payload = {};
    for (const [name, data] of members) {
        if (name.startsWith(PACKAGE_V5_PAYLOAD_PREFIX)) payload[name] = data;
    }
    const closure = validateBundleClosure({ ...bundle, payload, manifestBundle: manifest.bundle });
    const sources = parseCanonicalMember(members, "sources.jsonl", "sources");
    const documents = parseCanonicalMember(members, "documents.jsonl", "documents");
    const chunks = parseCanonicalMember(members, "chunks.jsonl", "chunks");
    const bm25 = parseCanonicalMember(members, "bm25.json", "bm25");
    const shared = validateSharedMembers({
        manifest,
        sources,
        documents,
        chunks,
        vectors: members.get("vectors.f32"),
        domainDictionaryBytes: members.get("bm25-domain-dict.txt"),
        bm25,
        runtimeContract,
        modelContract,
        expectedSchemaVersion: "5.0",
        runtimeSchemaVersions: [...SUPPORTED_PACKAGE_SCHEMA_VERSIONS],
    });
    validateManifestBundle(manifest.bundle, { ...bundle, payload });
    return {
        ...shared,
        buildAuditBytes: members.get("build-audit.json"),
        checksums,
        bundle,
        closure,
        manifestBundle: manifest.bundle,
        payload,
        contract: runtimeContract,
        bundleInstallRecord: buildBundleInstallRecord(closure, manifest),
    };
};

const buildBundleInstallRecord = (closure, manifest) => {
    void manifest;
    // One record per package path is guaranteed by bundle validation, so each
    // tool path is seen exactly once here; owners simply merge per record.
    const tools = new Map();
    for (const record of closure.files) {
        const owners = record.owners.filter(({ kind }) => kind === "skill-tool").map(({ id }) => id);
        if (!owners.length) continue;
        tools.set(record.sourcePath, { path: record.sourcePath, ownerSkillIds: [...new Set(owners)].sort() });
    }
    return {
        status: closure.status,
        skills: closure.skills.map(({ name, sourceId, rootPath, entryPath, description }) => ({ name, sourceId, rootPath, entryPath, description })),
        mcps: closure.mcps.map((mcp) => ({
            name: mcp.name,
            sourceId: mcp.sourceId,
            rootPath: mcp.rootPath,
            entryPath: mcp.entryPath,
            runtime: mcp.runtime,
            transport: mcp.transport,
            arguments: [...mcp.arguments],
            environment: [...mcp.environment],
        })),
        tools: [...tools.values()],
        skipped: closure.skips.map(({ kind, id, rootPath, reasonCode }) => ({ kind, id, rootPath, reasonCode })),
        overridden: detectOverriddenSkills(closure.skills),
    };
};

const parseCanonicalMember = (members, name, label) => {
    const data = members.get(name);
    if (!Buffer.isBuffer(data)) throw new KnowledgePackageError("invalid_archive", `Knowledge package member is not bytes: ${name}`);
    try {
        return name.endsWith(".jsonl") ? parseCanonicalJsonl(data, label) : parseCanonicalJson(data, label);
    } catch (error) {
        throw normalizeError(error);
    }
};

const compareCodePoints = (left, right) => (left < right ? -1 : left > right ? 1 : 0);

const readEntry = (zipFile, entry) => new Promise((resolvePromise, rejectPromise) => {
    zipFile.openReadStream(entry, (error, stream) => {
        if (error) return rejectPromise(new KnowledgePackageError("invalid_archive", `Unable to read ZIP member ${entry.fileName}`, { cause: error }));
        const chunks = [];
        let length = 0;
        stream.on("data", (chunk) => {
            length += chunk.length;
            if (length > MAX_MEMBER_BYTES) stream.destroy(new KnowledgePackageError("archive_too_large", `ZIP member exceeds the size limit: ${entry.fileName}`));
            else chunks.push(chunk);
        });
        stream.on("error", rejectPromise);
        stream.on("end", () => {
            if (length !== entry.uncompressedSize) return rejectPromise(new KnowledgePackageError("invalid_archive", `ZIP member size mismatch: ${entry.fileName}`));
            resolvePromise(Buffer.concat(chunks, length));
        });
    });
});

const readExactly = async (handle, buffer, position) => {
    const { bytesRead } = await handle.read(buffer, 0, buffer.length, position);
    if (bytesRead !== buffer.length) throw new KnowledgePackageError("invalid_archive", "Knowledge package ZIP is truncated");
};

const parseSidecar = (bytes, expectedName) => {
    const expectedLength = 64 + 2 + expectedName.length + 1;
    if (bytes.length !== expectedLength) throw new KnowledgePackageError("sidecar_invalid", "Knowledge package SHA-256 sidecar has an invalid length");
    const text = bytes.toString("ascii");
    const escapedName = expectedName.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const match = new RegExp(`^([0-9a-f]{64})  ${escapedName}\\n$`).exec(text);
    if (!match || !SHA256_RE.test(match[1])) throw new KnowledgePackageError("sidecar_invalid", "Knowledge package SHA-256 sidecar is noncanonical");
    return match[1];
};

export const fingerprintInstalledFile = async (directory, name) => {
    const path = join(directory, name);
    let info;
    try {
        info = await lstat(path, { bigint: true });
    } catch (error) {
        throw new KnowledgePackageError(
            "pack_unreadable",
            `Unable to fingerprint installed RAG file: ${name}`,
            { cause: error },
        );
    }
    if (info.isSymbolicLink() || !info.isFile() || info.size > BigInt(Number.MAX_SAFE_INTEGER)) {
        throw new KnowledgePackageError(
            "pack_unreadable",
            `Installed RAG fingerprint target is invalid: ${name}`,
        );
    }
    return {
        name,
        sizeBytes: Number(info.size),
        mtimeNs: info.mtimeNs.toString(),
    };
};

const loadInstalledAndVerify = async (target, install, {
    modelContract,
    runtimeContract,
    validateArchive = validatePackageArchive,
    loadInstalledPack = loadKnowledgePack,
}) => {
    const archive = join(target, ARCHIVE_NAME);
    const sidecar = join(target, SIDECAR_NAME);
    const validated = await validateArchive({ archivePath: archive, sidecarPath: sidecar, modelContract, runtimeContract });
    if (validated.sha256 !== install.package.sha256 || validated.sizeBytes !== install.package.sizeBytes) {
        throw new KnowledgePackageError("package_sha256_mismatch", "Installed package no longer matches install.json");
    }
    const extracted = await loadInstalledPack(target, { install, modelContract, runtimeContract });
    if (extracted.manifest.kbVersion !== install.kbVersion
        || extracted.contract.contractSha256 !== install.runtimeContractSha256) {
        throw new KnowledgePackageError("pack_unreadable", "Installed package contents do not match install.json identity");
    }
    return extracted;
};

const assertPointerMatchesInstall = (pointer, install) => {
    if (install.kbVersion !== pointer.kbVersion
        || install.package.sha256 !== pointer.sha256
        || install.kbId !== "mindstudio-insight-ascend") {
        throw new KnowledgePackageError("active_pointer_invalid", "RAG pointer does not match the installed package identity");
    }
};

const withLifecycleLock = async (root, operation) => {
    await ensurePrivateDirectory(root);
    const sentinel = join(root, ".lifecycle");
    try {
        const handle = await open(sentinel, "a", 0o600);
        await handle.close();
        const info = await lstat(sentinel);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("lifecycle sentinel is not a regular file");
    } catch (error) {
        throw new KnowledgePackageError("rag_data_unwritable", `RAG lifecycle sentinel is unavailable: ${error.message}`, { cause: error });
    }
    let release;
    try {
        release = await lockfile.lock(sentinel, { realpath: false, retries: 0, stale: 30000, update: 10000 });
    } catch (error) {
        throw new KnowledgePackageError("lifecycle_busy", "Another RAG lifecycle operation is running", { cause: error });
    }
    try {
        await cleanupStaging(root);
        return await operation();
    } finally {
        await release().catch(() => {});
    }
};

const cleanupStaging = async (root) => {
    const staging = join(root, ".staging");
    try {
        const entries = await readdir(staging, { withFileTypes: true });
        for (const entry of entries) {
            if (entry.isDirectory() && /^[A-Za-z0-9_-]+$/.test(entry.name)) await rm(join(staging, entry.name), { recursive: true, force: true });
        }
    } catch (error) {
        if (error?.code !== "ENOENT") throw error;
    }
};

const inspectExistingInstall = async (target) => {
    try {
        const info = await lstat(target);
        if (info.isSymbolicLink() || !info.isDirectory()) throw new KnowledgePackageError("version_conflict", "Installed package target is not a regular directory");
        return await readInstallRecord(target);
    } catch (error) {
        if (error?.code === "ENOENT") return undefined;
        throw error;
    }
};

const readOptionalPointer = async (root) => {
    if (!await regularPathExists(join(root, "active.json"))) return undefined;
    return (await readActiveKnowledgePointer(root)).pointer;
};

const regularPathExists = async (path) => {
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new KnowledgePackageError("active_pointer_invalid", "RAG active pointer is not a regular file");
        return true;
    } catch (error) {
        if (error?.code === "ENOENT") return false;
        throw error;
    }
};

const writeAtomic = async (root, name, bytes, id) => {
    const destination = join(root, name);
    const temporary = join(root, `.${name}.${process.pid}.${id}.tmp`);
    try {
        await writeDurableFile(temporary, bytes);
        await rename(temporary, destination);
    } finally {
        await rm(temporary, { force: true }).catch(() => {});
    }
};

const writeDurableFile = async (path, bytes) => {
    const handle = await open(path, "wx", 0o600);
    try {
        await handle.writeFile(bytes);
        await handle.sync();
    } finally {
        await handle.close();
    }
};

const fsyncFile = async (path) => {
    const handle = await open(path, "r+");
    try {
        await handle.sync();
    } finally {
        await handle.close();
    }
};

const ensurePrivateDirectory = async (directory) => {
    try {
        await mkdir(directory, { recursive: true, mode: 0o700 });
        const info = await lstat(directory);
        if (info.isSymbolicLink() || !info.isDirectory()) throw new Error("not a regular directory");
    } catch (error) {
        throw new KnowledgePackageError(
            "rag_data_unwritable",
            "RAG data path is not a writable regular directory",
            { cause: error },
        );
    }
};

const requireSourceFile = async (input, expectedName, missingCode, unreadableCode) => {
    const value = String(input ?? "").trim();
    if (!value) throw new KnowledgePackageError(missingCode, `${expectedName} path is required`);
    return requireRegularFile(value, unreadableCode, expectedName);
};

const requireRegularFile = async (input, code, label) => {
    const path = resolve(String(input));
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
    } catch (error) {
        throw new KnowledgePackageError(code, `${label} is unavailable: ${error.message}`, { cause: error });
    }
    return path;
};

const validateVersion = (value) => {
    const version = String(value ?? "").trim();
    if (!/^\d{2}\.[012]\.[1-9]\d*$/.test(version) || !isSafeDirectoryName(version)) throw new KnowledgePackageError("pack_unreadable", "Knowledge package version is invalid");
    return version;
};

const pointerEntry = (kbVersion, digest) => ({ kbVersion, sha256: digest, directory: kbVersion });

const archiveNamesForHandoff = (packPath, sidecarPath) => {
    if (basename(String(packPath)) === ARCHIVE_NAME && basename(String(sidecarPath)) === SIDECAR_NAME) {
        return { archive: ARCHIVE_NAME, sidecar: SIDECAR_NAME, schemaVersion: "5.0" };
    }
    throw new KnowledgePackageError("sidecar_invalid", `Knowledge package handoff files must be named ${ARCHIVE_NAME} and ${SIDECAR_NAME}`);
};

const importResult = (status, validated, installMode) => ({
    status,
    version: validated.pack.manifest.kbVersion,
    kbId: KB_ID,
    sha256: validated.sha256,
    sizeBytes: validated.sizeBytes,
    chunks: validated.pack.chunks.length,
    installMode,
});

const installPackageBundle = async ({ install, pack, agentWorkspacePath }) => {
    if (pack.manifest?.schemaVersion !== "5.0" || !install.bundle) {
        return { status: "absent", skills: [], mcps: [], tools: [], skipped: [], overridden: [], workspace: "no_bundle" };
    }
    if (agentWorkspacePath === undefined) {
        return {
            status: install.bundle.status,
            skills: install.bundle.skills.map(({ name }) => name),
            mcps: install.bundle.mcps.map(({ name }) => name),
            tools: install.bundle.tools.map(({ path }) => path),
            skipped: install.bundle.skipped,
            overridden: install.bundle.overridden,
            workspace: "skipped_no_workspace",
        };
    }
    const staged = await stageBundleForWorkspace({
        skills: pack.bundle.skills,
        mcps: pack.bundle.mcps,
        files: pack.bundle.files,
        skips: pack.bundle.skips,
        payload: pack.payload,
    });
    const installed = await installBundleToWorkspace({ staged, agentWorkspacePath, platform: process.platform });
    return {
        status: install.bundle.status,
        ...installed,
        workspace: "installed",
    };
};

const installModeForImportMode = (mode) => ({
    development: "development-local",
    "product-bundled": "product-bundled",
})[mode];

const sha256File = async (path) => {
    const digest = createHash("sha256");
    for await (const chunk of createReadStream(path)) digest.update(chunk);
    return digest.digest("hex");
};

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

const resolveRequiredPath = (value, label) => {
    const path = String(value ?? "").trim();
    if (!path) throw new KnowledgePackageError("configuration_error", `${label} is required`);
    return resolve(path);
};

const sanitizeId = (value) => {
    const id = String(value ?? "").replace(/[^A-Za-z0-9_-]/g, "");
    if (!id) throw new KnowledgePackageError("configuration_error", "RAG transaction ID is invalid");
    return id;
};

const isSafeDirectoryName = (value) => Boolean(value)
    && value === basename(value)
    && !value.includes("/")
    && !value.includes("\\")
    && !value.includes(":")
    && value !== "."
    && value !== "..";

const safeChildDirectory = (root, directory) => {
    if (!isSafeDirectoryName(directory)) throw new KnowledgePackageError("configuration_error", "RAG directory segment is unsafe");
    const resolvedRoot = resolve(root);
    const child = resolve(resolvedRoot, directory);
    const relativeChild = relative(resolvedRoot, child);
    if (!relativeChild || relativeChild.startsWith("..") || isAbsolute(relativeChild)) throw new KnowledgePackageError("configuration_error", "RAG directory escapes its root");
    return child;
};

const normalizeError = (error) => {
    if (error instanceof KnowledgePackageError) return error;
    if (typeof error?.code === "string" && /^[a-z0-9_]+$/.test(error.code)) return new KnowledgePackageError(error.code, error.message, { cause: error });
    if (["EACCES", "EPERM", "EROFS"].includes(error?.code)) return new KnowledgePackageError("rag_data_unwritable", "RAG data directory is not writable", { cause: error });
    return new KnowledgePackageError("invalid_archive", error?.message || "Knowledge package validation failed", { cause: error });
};

const rejectAndClose = (zipFile, rejectPromise, error) => {
    zipFile.close();
    rejectPromise(error);
};

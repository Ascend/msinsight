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

import { createHash } from "node:crypto";
import { lstat, readFile, readdir } from "node:fs/promises";
import { basename, isAbsolute, join, relative, resolve } from "node:path";
import { loadRuntimeContract } from "./runtimeContract.mjs";
import yauzl from "yauzl";
import { PACKAGE_V5_PAYLOAD_PREFIX, validateBundleRecords } from "./wire/packageBundleContracts.mjs";
import { PACKAGE_V5_METADATA_MEMBERS, PACKAGE_V5_SUFFIX_MEMBERS } from "./wire/packageBundleContracts.mjs";
import { parseCanonicalJson } from "./wire/strictJsonParser.mjs";

export const REQUIRED_PACKAGE_FILES = Object.freeze([...PACKAGE_V5_METADATA_MEMBERS, ...PACKAGE_V5_SUFFIX_MEMBERS]);
const INSTALL_FILE = "install.json";
const ARCHIVE_FILE = "knowledge-pack-v5.zip";
const SIDECAR_FILE = "knowledge-pack-v5.zip.sha256";
const PACKAGE_V5_FIXED_FILES = REQUIRED_PACKAGE_FILES;
const INSTALLED_FILES = new Set([...PACKAGE_V5_FIXED_FILES, INSTALL_FILE, ARCHIVE_FILE, SIDECAR_FILE]);
const FINGERPRINT_FILES = [ARCHIVE_FILE, SIDECAR_FILE, ...PACKAGE_V5_FIXED_FILES];
const SHA256_RE = /^[0-9a-f]{64}$/;

export const loadKnowledgePack = async (kbDir, { runtimeDir, runtimeContract, modelContract, install } = {}) => {
    const directory = resolveRequiredDirectory(kbDir, "knowledge directory");
    await ensureDirectory(directory);
    await ensureRegularFile(join(directory, "manifest.json"), "manifest.json");
    const manifestSchema = peekManifestSchemaVersion(await readFile(join(directory, "manifest.json")));
    const memberNames = PACKAGE_V5_FIXED_FILES;
    const entries = await readdir(directory, { withFileTypes: true });
    const names = new Set(entries.map(({ name }) => name));
    for (const name of memberNames) {
        if (!names.has(name)) throw codedError("pack_unreadable", `RAG knowledge file does not exist: ${name}`);
        await ensureRegularFile(join(directory, name), name);
    }
    const members = new Map(await Promise.all(memberNames.map(async (name) => [name, await readFile(join(directory, name))])));
    if (install) await validateInstalledFingerprints(directory, members, install);
    const loadedRuntime = runtimeContract ?? (await loadRuntimeContract(runtimeDir)).contract;
    const { validatePackageV5MemberBytes } = await import("./knowledgePackageService.mjs");
    return {
        kbDir: directory,
        ...validatePackageV5MemberBytes({
            members: await withInstalledPayload(directory, members, manifestSchema),
            runtimeContract: loadedRuntime,
            modelContract,
        }),
    };
};

export const loadActiveKnowledgePack = async (ragDataDir, options = {}) => {
    const { root, pointer, kbDir } = await readActiveKnowledgePointer(ragDataDir);
    const install = await readInstallRecord(kbDir);
    if (install.kbVersion !== pointer.active.kbVersion
        || install.package.sha256 !== pointer.active.sha256
        || install.kbId !== "mindstudio-insight-ascend") {
        throw codedError("active_pointer_invalid", "RAG active pointer does not match the installed package identity");
    }
    const pack = await loadKnowledgePack(kbDir, { ...options, install });
    if (pack.manifest.kbVersion !== pointer.active.kbVersion
        || pack.contract.contractSha256 !== install.runtimeContractSha256) {
        throw codedError("active_pointer_invalid", "RAG active pointer does not match package contents");
    }
    return { ...pack, root, active: pointer.active, previous: pointer.previous, install };
};

export const readActiveKnowledgePointer = async (ragDataDir) => {
    const root = resolveRequiredDirectory(ragDataDir, "data directory");
    await ensureDirectory(root);
    const activePath = join(root, "active.json");
    await ensureRegularFile(activePath, "active.json");
    const pointer = parseCanonicalJson(await readFile(activePath), "active.json");
    validateActivePointer(pointer);
    return { root, pointer, kbDir: safeChildDirectory(root, pointer.active.directory) };
};

export const readInstallRecord = async (kbDir) => {
    const path = join(kbDir, INSTALL_FILE);
    await ensureRegularFile(path, INSTALL_FILE);
    const install = parseCanonicalJson(await readFile(path), INSTALL_FILE);
    validateInstallRecord(install);
    const closure = INSTALLED_FILES;
    const entries = await readdir(kbDir, { withFileTypes: true });
    for (const entry of entries) {
        if (!closure.has(entry.name) || !entry.isFile()) {
            throw codedError("pack_unreadable", "Installed RAG package contains an unexpected or non-file entry");
        }
    }
    if (entries.length !== closure.size) throw codedError("pack_unreadable", "Installed RAG package file closure is incomplete");
    return install;
};

export const validateActivePointer = (pointer) => {
    exactKeys(pointer, ["active", "previous", "schemaVersion"], "active pointer");
    if (pointer.schemaVersion !== "4.0") throw codedError("active_pointer_invalid", "Unsupported RAG active pointer schemaVersion");
    validatePointerEntry(pointer.active, "active");
    if (pointer.previous !== null) validatePointerEntry(pointer.previous, "previous");
    if (pointer.previous?.sha256 === pointer.active.sha256 && pointer.previous?.kbVersion === pointer.active.kbVersion) {
        throw codedError("active_pointer_invalid", "RAG active and previous pointers must be distinct");
    }
};

const validatePointerEntry = (entry, label) => {
    exactKeys(entry, ["directory", "kbVersion", "sha256"], `${label} pointer`);
    const version = String(entry.kbVersion ?? "");
    if (!/^\d{2}\.[012]\.[1-9]\d*$/.test(version) || entry.directory !== version || !isSafeDirectoryName(entry.directory)) {
        throw codedError("active_pointer_invalid", `RAG ${label} pointer has an invalid directory or version`);
    }
    if (!SHA256_RE.test(entry.sha256)) throw codedError("active_pointer_invalid", `RAG ${label} pointer has an invalid SHA-256`);
};

const validateInstallRecord = (install) => {
    const baseKeys = [
        "files",
        "installMode",
        "kbId",
        "kbVersion",
        "memberSha256",
        "package",
        "runtimeContractSha256",
        "schemaVersion",
    ];
    exactKeys(install, [...baseKeys, "bundle"], "install record");
    if (install.schemaVersion !== "1.1" || install.kbId !== "mindstudio-insight-ascend" || !/^\d{2}\.[012]\.[1-9]\d*$/.test(install.kbVersion)) {
        throw codedError("pack_unreadable", "Installed RAG package identity is invalid");
    }
    validateInstallBundle(install.bundle);
    if (!SHA256_RE.test(install.runtimeContractSha256)) throw codedError("pack_unreadable", "Installed RAG runtime contract digest is invalid");
    if (!["development-local", "product-bundled"].includes(install.installMode)) throw codedError("pack_unreadable", "Installed RAG mode is invalid");
    exactKeys(install.package, ["sha256", "sizeBytes"], "install package identity");
    if (!SHA256_RE.test(install.package.sha256) || !Number.isSafeInteger(install.package.sizeBytes) || install.package.sizeBytes <= 0) {
        throw codedError("pack_unreadable", "Installed RAG package digest or size is invalid");
    }
    validateInstallMemberDigests(install);
    const fingerprintFiles = FINGERPRINT_FILES;
    if (!Array.isArray(install.files) || install.files.length !== fingerprintFiles.length) throw codedError("pack_unreadable", "Installed RAG file fingerprints are invalid");
    const names = install.files.map(({ name }) => name);
    if (names.some((name, index) => name !== fingerprintFiles[index])) throw codedError("pack_unreadable", "Installed RAG file fingerprints are not ordered");
    for (const file of install.files) {
        exactKeys(file, ["mtimeNs", "name", "sizeBytes"], "install file fingerprint");
        if (!Number.isSafeInteger(file.sizeBytes)
            || file.sizeBytes < 0
            || typeof file.mtimeNs !== "string"
            || !/^\d+$/.test(file.mtimeNs)) throw codedError("pack_unreadable", "Installed RAG file fingerprint is invalid");
    }
};

const validateInstallMemberDigests = (install) => {
    // Key order is already enforced by canonical JSON parsing; only the fixed
    // subset and digest shapes are re-checked here.
    for (const digest of Object.values(install.memberSha256)) {
        if (!SHA256_RE.test(digest)) throw codedError("pack_unreadable", "Installed RAG member digest is invalid");
    }
    for (const name of PACKAGE_V5_FIXED_FILES) {
        if (!Object.hasOwn(install.memberSha256, name)) throw codedError("pack_unreadable", `Installed RAG member digest is missing: ${name}`);
    }
};

const validateInstallBundle = (bundle) => {
    exactKeys(bundle, ["mcps", "overridden", "skills", "skipped", "status", "tools"], "install bundle record");
    if (!["complete", "partial", "empty"].includes(bundle.status)) throw codedError("pack_unreadable", "Installed RAG bundle status is invalid");
    if (!Array.isArray(bundle.skills) || !Array.isArray(bundle.mcps) || !Array.isArray(bundle.tools) || !Array.isArray(bundle.skipped) || !Array.isArray(bundle.overridden)) {
        throw codedError("pack_unreadable", "Installed RAG bundle record is invalid");
    }
    for (const skill of bundle.skills) {
        exactKeys(skill, ["description", "entryPath", "name", "rootPath", "sourceId"], "install bundle skill");
        if (!skill.name || !skill.sourceId || !skill.rootPath || !skill.entryPath) throw codedError("pack_unreadable", "Installed RAG bundle skill is invalid");
    }
    for (const mcp of bundle.mcps) {
        exactKeys(mcp, ["arguments", "entryPath", "environment", "name", "rootPath", "runtime", "sourceId", "transport"], "install bundle MCP");
        if (!mcp.name || !mcp.sourceId || !mcp.rootPath || !mcp.entryPath) throw codedError("pack_unreadable", "Installed RAG bundle MCP is invalid");
        if (!Array.isArray(mcp.arguments) || !Array.isArray(mcp.environment)) throw codedError("pack_unreadable", "Installed RAG bundle MCP is invalid");
        if (mcp.environment.some((entry) => typeof entry !== "string" || entry.includes("="))) {
            throw codedError("pack_unreadable", "Installed RAG bundle MCP environment must name variables only");
        }
    }
    for (const tool of bundle.tools) {
        exactKeys(tool, ["ownerSkillIds", "path"], "install bundle tool");
        if (!tool.path || !Array.isArray(tool.ownerSkillIds)) throw codedError("pack_unreadable", "Installed RAG bundle tool is invalid");
    }
    for (const skip of bundle.skipped) {
        exactKeys(skip, ["id", "kind", "reasonCode", "rootPath"], "install bundle skip");
        if (!["skill", "mcp", "skill-tool"].includes(skip.kind) || !skip.id || !skip.reasonCode) {
            throw codedError("pack_unreadable", "Installed RAG bundle skip is invalid");
        }
    }
    for (const name of bundle.overridden) {
        if (typeof name !== "string" || !name) throw codedError("pack_unreadable", "Installed RAG bundle override is invalid");
    }
};

const withInstalledPayload = async (directory, members, manifestSchema) => {
    void manifestSchema;
    const full = new Map(members);
    const payloadNames = (await readInstalledPayloadNames(directory, ARCHIVE_FILE)).filter((name) =>
        name.startsWith(PACKAGE_V5_PAYLOAD_PREFIX),
    );
    for (const name of payloadNames) {
        full.set(name, await readInstalledPayloadMember(directory, ARCHIVE_FILE, name));
    }
    return full;
};

const MAX_INSTALLED_PAYLOAD_BYTES = 512 * 1024 * 1024;

const readInstalledPayloadNames = (directory, archiveName) => new Promise((resolvePromise, rejectPromise) => {
    yauzl.open(join(directory, archiveName), { autoClose: true, decodeStrings: true, lazyEntries: true }, (openError, zipFile) => {
        if (openError) return rejectPromise(codedError("pack_unreadable", `Installed RAG archive is unavailable: ${openError.message}`));
        const names = [];
        zipFile.on("error", (error) => rejectPromise(codedError("pack_unreadable", `Installed RAG archive is unavailable: ${error.message}`)));
        zipFile.on("entry", (entry) => {
            names.push(entry.fileName);
            zipFile.readEntry();
        });
        zipFile.on("end", () => {
            zipFile.close();
            resolvePromise(names);
        });
        zipFile.readEntry();
    });
});

const readInstalledPayloadMember = (directory, archiveName, name) => new Promise((resolvePromise, rejectPromise) => {
    yauzl.open(join(directory, archiveName), { autoClose: true, decodeStrings: true, lazyEntries: true }, (openError, zipFile) => {
        if (openError) return rejectPromise(codedError("pack_unreadable", `Installed RAG archive is unavailable: ${openError.message}`));
        let settled = false;
        const fail = (error) => {
            if (settled) return;
            settled = true;
            zipFile.close();
            rejectPromise(error);
        };
        zipFile.on("error", fail);
        zipFile.on("entry", (entry) => {
            if (entry.fileName !== name) {
                zipFile.readEntry();
                return;
            }
            zipFile.openReadStream(entry, (streamError, stream) => {
                if (streamError) return fail(codedError("pack_unreadable", `Installed RAG member is unavailable: ${name}`));
                const chunks = [];
                let length = 0;
                stream.on("data", (chunk) => {
                    length += chunk.length;
                    if (length > MAX_INSTALLED_PAYLOAD_BYTES) stream.destroy(codedError("pack_unreadable", `Installed RAG member exceeds the size limit: ${name}`));
                    else chunks.push(chunk);
                });
                stream.on("error", fail);
                stream.on("end", () => {
                    if (settled) return;
                    settled = true;
                    zipFile.close();
                    resolvePromise(Buffer.concat(chunks, length));
                });
            });
        });
        zipFile.on("end", () => fail(codedError("pack_unreadable", `Installed RAG member is missing: ${name}`)));
        zipFile.readEntry();
    });
});

const peekManifestSchemaVersion = (bytes) => {
    let manifest;
    try {
        manifest = parseCanonicalJson(bytes, "manifest.json");
    } catch {
        throw codedError("pack_unreadable", "Installed RAG manifest is not readable");
    }
    if (manifest.schemaVersion !== "5.0") {
        throw codedError("pack_unreadable", "Installed RAG manifest schemaVersion must be 5.0");
    }
    return "5.0";
};

const validateInstalledFingerprints = async (directory, members, install) => {
    const fingerprintFiles = FINGERPRINT_FILES;
    const memberFiles = PACKAGE_V5_FIXED_FILES;
    const expectedFiles = new Map(install.files.map((file) => [file.name, file]));
    for (const name of fingerprintFiles) {
        const expected = expectedFiles.get(name);
        const info = await lstat(join(directory, name), { bigint: true });
        if (info.isSymbolicLink()
            || !info.isFile()
            || info.size > BigInt(Number.MAX_SAFE_INTEGER)
            || Number(info.size) !== expected?.sizeBytes) {
            throw codedError("checksum_mismatch", `Installed RAG file fingerprint changed: ${name}`);
        }
    }
    for (const name of memberFiles) {
        const bytes = members.get(name);
        if (createHash("sha256").update(bytes).digest("hex") !== install.memberSha256[name]) {
            throw codedError("checksum_mismatch", `Installed RAG member no longer matches install.json: ${name}`);
        }
    }
};

const exactKeys = (value, expected, label) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw codedError("active_pointer_invalid", `RAG ${label} must be an object`);
    const actual = Object.keys(value).sort();
    const keys = [...expected].sort();
    if (actual.length !== keys.length || actual.some((key, index) => key !== keys[index])) throw codedError("active_pointer_invalid", `RAG ${label} fields are invalid`);
};

const ensureDirectory = async (path) => {
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isDirectory()) throw new Error("not a regular directory");
    } catch (error) {
        throw codedError("pack_unreadable", `RAG directory is unavailable: ${error.message}`);
    }
};

const ensureRegularFile = async (path, label) => {
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
    } catch (error) {
        throw codedError("pack_unreadable", `RAG file is unavailable (${label}): ${error.message}`);
    }
};

const resolveRequiredDirectory = (value, label) => {
    const path = String(value ?? "").trim();
    if (!path) throw codedError("pack_unreadable", `RAG ${label} is required`);
    return resolve(path);
};

const isSafeDirectoryName = (value) => Boolean(value)
    && value === basename(value)
    && !value.includes("/")
    && !value.includes("\\")
    && !value.includes(":")
    && value !== "."
    && value !== "..";

const safeChildDirectory = (root, directory) => {
    const resolvedRoot = resolve(root);
    const child = resolve(resolvedRoot, directory);
    const relativeChild = relative(resolvedRoot, child);
    if (!relativeChild || relativeChild.startsWith("..") || isAbsolute(relativeChild)) {
        throw codedError("active_pointer_invalid", "RAG active pointer resolves outside the data directory");
    }
    return child;
};

const codedError = (code, message) => Object.assign(new Error(message), { code });

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { copyFile, lstat, mkdtemp, mkdir, readFile, rm, utimes, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { test } from "node:test";
import lockfile from "proper-lockfile";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import { CHECKSUM_MEMBERS } from "../../../services/rag/wire/packageContracts.mjs";
import {
    createKnowledgePackageService,
    fingerprintInstalledFile,
    validatePackageArchive,
} from "../../../services/rag/knowledgePackageService.mjs";
import {
    loadActiveKnowledgePack,
    loadKnowledgePack,
    readInstallRecord,
    validateActivePointer,
} from "../../../services/rag/knowledgePackLoader.mjs";
import {
    createCanonicalZip,
    createPackageV4Members,
    MODEL_CONTRACT,
    RUNTIME_CONTRACT,
    sha256,
    writePackageV4Handoff,
} from "./packageV4Fixture.mjs";

test("import is non-activating and repeated import validates the immutable installed package", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);

    const imported = await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });

    assert.equal(imported.status, "imported");
    assert.equal((await service.getStatus()).active, null);
    await assert.rejects(readFile(join(fixture.ragDataDir, "active.json")), { code: "ENOENT" });
    assert.equal((await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath })).status, "already_imported");
    const install = JSON.parse(await readFile(join(fixture.ragDataDir, "26.1.1", "install.json"), "utf8"));
    assert.equal(install.package.sha256, handoff.digest);
    assert.match(install.runtimeContractSha256, /^[0-9a-f]{64}$/);
    assert.equal("consumerContractSha256" in install, false);
    assert.equal("profileSha256" in install, false);
    assert.equal("buildScope" in install, false);
    assert.equal(install.installMode, "development-local");
    assert.equal(install.files.length, 2 + createPackageV4Members().size);
    assert.deepEqual(install.files.map(({ name }) => name), [
        "knowledge-pack-v4.zip",
        "knowledge-pack-v4.zip.sha256",
        ...createPackageV4Members().keys(),
    ]);
    assert.ok(install.files.every(({ sizeBytes, mtimeNs }) => Number.isSafeInteger(sizeBytes) && /^\d+$/.test(mtimeNs)));
    assert.deepEqual(Object.keys(install.memberSha256), [...createPackageV4Members().keys()].sort());
});

test("activate and verify bind the exact installed package identity and are idempotent", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });

    const activated = await service.activate("26.1.1", { sha256: handoff.digest });
    const repeated = await service.activate("26.1.1", { sha256: handoff.digest });
    const verified = await service.verify();

    assert.equal(activated.status, "activated");
    assert.equal(repeated.status, "already_active");
    assert.deepEqual(verified, {
        status: "verified",
        kbId: "mindstudio-insight-ascend",
        kbVersion: "26.1.1",
        sha256: handoff.digest,
        chunks: 1,
        installMode: "development-local",
    });
    assert.deepEqual(await service.getStatus(), {
        active: { directory: "26.1.1", kbVersion: "26.1.1", sha256: handoff.digest },
        previous: null,
        installMode: "development-local",
    });
    assert.deepEqual(JSON.parse(await readFile(join(fixture.ragDataDir, "active.json"), "utf8")), {
        schemaVersion: "4.0",
        active: { directory: "26.1.1", kbVersion: "26.1.1", sha256: handoff.digest },
        previous: null,
    });
    await assert.rejects(service.activate("26.1.1", { sha256: "0".repeat(64) }), errorCode("package_sha256_mismatch"));
});

test("activation retains one previous package and rollback atomically swaps the pointers", async () => {
    const fixture = await createFixture();
    const first = await writePackageV4Handoff(fixture.handoffs, { directory: "first", kbVersion: "26.1.1" });
    const second = await writePackageV4Handoff(fixture.handoffs, { directory: "second", kbVersion: "26.1.2" });
    const service = createService(fixture);
    await service.importPackage(first.archivePath, { mode: "development", sidecarPath: first.sidecarPath });
    await service.activate("26.1.1", { sha256: first.digest });
    await service.importPackage(second.archivePath, { mode: "development", sidecarPath: second.sidecarPath });
    await service.activate("26.1.2", { sha256: second.digest });

    const rolledBack = await service.rollback();

    assert.deepEqual(rolledBack, {
        status: "rolled_back",
        active: { directory: "26.1.1", kbVersion: "26.1.1", sha256: first.digest },
        previous: { directory: "26.1.2", kbVersion: "26.1.2", sha256: second.digest },
        chunks: 1,
        installMode: "development-local",
    });
    assert.equal((await service.verify()).kbVersion, "26.1.1");
});

test("rollback without a previous package fails and damaged previous bytes never change active.json", async () => {
    const fixture = await createFixture();
    const first = await writePackageV4Handoff(fixture.handoffs, { directory: "first", kbVersion: "26.1.1" });
    const second = await writePackageV4Handoff(fixture.handoffs, { directory: "second", kbVersion: "26.1.2" });
    const service = createService(fixture);
    await service.importPackage(first.archivePath, { mode: "development", sidecarPath: first.sidecarPath });
    await service.activate("26.1.1", { sha256: first.digest });
    await assert.rejects(service.rollback(), errorCode("rollback_unavailable"));
    await service.importPackage(second.archivePath, { mode: "development", sidecarPath: second.sidecarPath });
    await service.activate("26.1.2", { sha256: second.digest });
    const pointerPath = join(fixture.ragDataDir, "active.json");
    const before = await readFile(pointerPath);
    await writeFile(join(fixture.ragDataDir, "26.1.1", "knowledge-pack-v4.zip"), Buffer.from("damaged"));

    await assert.rejects(service.rollback(), errorCode("package_sha256_mismatch"));

    assert.deepEqual(await readFile(pointerPath), before);
});

test("same version with different canonical bytes is a permanent version conflict", async () => {
    const fixture = await createFixture();
    const first = await writePackageV4Handoff(fixture.handoffs, { directory: "first", builderVersion: "0.1.0" });
    const second = await writePackageV4Handoff(fixture.handoffs, { directory: "second", builderVersion: "0.1.1" });
    const service = createService(fixture);
    await service.importPackage(first.archivePath, { mode: "development", sidecarPath: first.sidecarPath });

    await assert.rejects(
        service.importPackage(second.archivePath, { mode: "development", sidecarPath: second.sidecarPath }),
        errorCode("version_conflict"),
    );
});

test("service accepts only development imports and exposes no seed provision operation", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);

    await assert.rejects(
        service.importPackage(handoff.archivePath, { mode: "release", sidecarPath: handoff.sidecarPath }),
        errorCode("configuration_error"),
    );
    assert.equal(service.provisionSeed, undefined);
});

test("all-enabled Package still installs only as development-local", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, {
        directory: "all-enabled",
        members: createPackageV4Members({ buildScopeMode: "all-enabled" }),
    });
    const service = createService(fixture);

    await service.importPackage(handoff.archivePath, {
        mode: "development",
        sidecarPath: handoff.sidecarPath,
    });

    const install = JSON.parse(await readFile(
        join(fixture.ragDataDir, "26.1.1", "install.json"),
        "utf8",
    ));
    assert.equal(install.installMode, "development-local");
});

test("all lifecycle mutations reject immediately while another process owns the sentinel lock", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    await mkdir(fixture.ragDataDir, { recursive: true });
    const sentinel = join(fixture.ragDataDir, ".lifecycle");
    await writeFile(sentinel, "");
    const release = await lockfile.lock(sentinel, { realpath: false, retries: 0, stale: 30_000, update: 10_000 });
    try {
        await assert.rejects(
            createService(fixture).importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath }),
            errorCode("lifecycle_busy"),
        );
    } finally {
        await release();
    }
});

test("unwritable fixed data root fails as rag_data_unwritable without fallback", async () => {
    const fixture = await createFixture();
    const blocker = join(fixture.root, "not-a-directory");
    await writeFile(blocker, "blocked");
    const service = createKnowledgePackageService({
        ragDataDir: join(blocker, "rag-data"),
        modelDir: fixture.modelDir,
        runtimeDir: fixture.runtimeDir,
        loadModelContract: async () => ({ modelDir: fixture.modelDir, manifest: MODEL_CONTRACT }),
        loadRuntime: async () => RUNTIME_CONTRACT,
    });

    await assert.rejects(
        service.importPackage("missing", { mode: "development" }),
        errorCode("rag_data_unwritable"),
    );
    await assert.rejects(readFile(join(fixture.root, "rag-data")), { code: "ENOENT" });
});

test("archive validation rejects noncanonical sidecars, member order, metadata, and ZIP comments", async () => {
    const fixture = await createFixture();
    const cases = [
        { name: "sidecar", sidecarBytes: Buffer.from(`${"A".repeat(64)}  knowledge-pack-v4.zip\n`, "ascii"), code: "sidecar_invalid" },
        { name: "order", zipOptions: { names: [...createPackageV4Members().keys()].reverse() }, code: "invalid_archive_entries" },
        { name: "timestamp", zipOptions: { lastModTime: 1 }, code: "invalid_archive" },
        { name: "comment", zipOptions: { comment: Buffer.from("comment") }, code: "invalid_archive" },
    ];
    for (const item of cases) {
        const members = createPackageV4Members();
        const archive = createCanonicalZip(members, item.zipOptions);
        const directory = join(fixture.handoffs, item.name);
        await mkdir(directory, { recursive: true });
        const archivePath = join(directory, "knowledge-pack-v4.zip");
        const sidecarPath = join(directory, "knowledge-pack-v4.zip.sha256");
        await writeFile(archivePath, archive);
        await writeFile(sidecarPath, item.sidecarBytes ?? `${sha256(archive)}  knowledge-pack-v4.zip\n`);
        await assert.rejects(
            validatePackageArchive({ archivePath, sidecarPath, modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
            errorCode(item.code),
            item.name,
        );
    }
});

test("active loader binds extracted members to install.json and rejects post-install tampering", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });
    await service.activate("26.1.1", { sha256: handoff.digest });
    const chunkPath = join(fixture.ragDataDir, "26.1.1", "chunks.jsonl");
    const changed = Buffer.from(await readFile(chunkPath));
    changed[0] ^= 1;
    await writeFile(chunkPath, changed);

    await assert.rejects(
        loadActiveKnowledgePack(fixture.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("checksum_mismatch"),
    );
});

test("active loader allows transport-only mtime drift after content-preserving extraction", async (t) => {
    const fixture = await createActivatedFixture(t);
    const archive = join(fixture.ragDataDir, "26.1.1", "knowledge-pack-v4.zip");
    await utimes(archive, new Date("2000-01-01T00:00:00Z"), new Date("2000-01-01T00:00:00Z"));

    const pack = await loadActiveKnowledgePack(fixture.ragDataDir, {
        modelContract: MODEL_CONTRACT,
        runtimeContract: RUNTIME_CONTRACT,
    });

    assert.equal(pack.manifest.kbVersion, "26.1.1");
});

test("active loader rejects size-changing file tampering before member parsing", async (t) => {
    const fixture = await createActivatedFixture(t);
    const chunkPath = join(fixture.ragDataDir, "26.1.1", "chunks.jsonl");
    await writeFile(chunkPath, Buffer.concat([await readFile(chunkPath), Buffer.from(" ")]));

    await assert.rejects(
        loadActiveKnowledgePack(fixture.ragDataDir, {
            modelContract: MODEL_CONTRACT,
            runtimeContract: RUNTIME_CONTRACT,
        }),
        errorCode("checksum_mismatch"),
    );
});

test("active loader rejects pointer and install identity mutations", async (t) => {
    for (const mutate of [
        ({ pointer }) => { pointer.active.sha256 = "0".repeat(64); },
        ({ pointer }) => { pointer.previous = { ...pointer.active }; },
        ({ pointer }) => { pointer.active.directory = "26.1.2"; },
        ({ install }) => { install.schemaVersion = "2.0"; },
        ({ install }) => { install.package.sha256 = "invalid"; },
        ({ install }) => { install.memberSha256["manifest.json"] = "invalid"; },
        ({ install }) => { install.files = []; },
    ]) {
        const fixture = await createActivatedFixture(t);
        const kbDir = join(fixture.ragDataDir, "26.1.1");
        const pointerPath = join(fixture.ragDataDir, "active.json");
        const installPath = join(kbDir, "install.json");
        const pointer = JSON.parse(await readFile(pointerPath, "utf8"));
        const install = JSON.parse(await readFile(installPath, "utf8"));
        mutate({ pointer, install });
        await writeFile(pointerPath, canonicalJsonBytes(pointer));
        await writeFile(installPath, canonicalJsonBytes(install));
        await assert.rejects(
            loadActiveKnowledgePack(fixture.ragDataDir, {
                modelContract: MODEL_CONTRACT,
                runtimeContract: RUNTIME_CONTRACT,
            }),
        );
    }
});

test("active loader rejects unexpected, missing, and non-file installed members", async (t) => {
    const unexpected = await createActivatedFixture(t);
    await writeFile(join(unexpected.ragDataDir, "26.1.1", "unexpected.txt"), "unexpected");
    await assert.rejects(
        loadActiveKnowledgePack(unexpected.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("pack_unreadable"),
    );

    const missingFixture = await createFixture();
    t.after(() => rm(missingFixture.root, { recursive: true, force: true }));
    const missingRoot = join(missingFixture.root, "missing-rag-data");
    await assert.rejects(
        loadActiveKnowledgePack(missingRoot, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("pack_unreadable"),
    );

    const nonFile = await createActivatedFixture(t);
    const manifest = join(nonFile.ragDataDir, "26.1.1", "manifest.json");
    await rm(manifest);
    await mkdir(manifest);
    await assert.rejects(
        loadActiveKnowledgePack(nonFile.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("pack_unreadable"),
    );
});

test("service normalizes missing handoffs, bad versions, and invalid lifecycle sentinels", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const service = createService(fixture);
    await assert.rejects(service.importPackage("", { mode: "development" }), errorCode("pack_missing"));
    await assert.rejects(service.importPackage("missing", { mode: "development" }), errorCode("pack_unreadable"));
    await assert.rejects(service.activate("../escape"), errorCode("pack_unreadable"));
    await assert.rejects(service.activate("26.1.1"), errorCode("pack_unreadable"));

    await mkdir(fixture.ragDataDir, { recursive: true });
    await rm(join(fixture.ragDataDir, ".lifecycle"), { force: true });
    await mkdir(join(fixture.ragDataDir, ".lifecycle"));
    await assert.rejects(service.getStatus().then(() => service.rollback()), errorCode("rag_data_unwritable"));
});

test("archive validator rejects wrong names, sidecar length, and SHA mismatch", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    await assert.rejects(validatePackageArchive({
        archivePath: handoff.archivePath,
        sidecarPath: join(fixture.handoffs, "first", "wrong-name.sha256"),
        modelContract: MODEL_CONTRACT,
        runtimeContract: RUNTIME_CONTRACT,
    }), errorCode("sidecar_invalid"));

    const shortSidecar = join(fixture.handoffs, "first", "knowledge-pack-v4.zip.sha256");
    await writeFile(shortSidecar, "short\n");
    await assert.rejects(validatePackageArchive({
        archivePath: handoff.archivePath,
        sidecarPath: shortSidecar,
        modelContract: MODEL_CONTRACT,
        runtimeContract: RUNTIME_CONTRACT,
    }), errorCode("sidecar_invalid"));
    await writeFile(shortSidecar, `${"0".repeat(64)}  knowledge-pack-v4.zip\n`);
    await assert.rejects(validatePackageArchive({
        archivePath: handoff.archivePath,
        sidecarPath: shortSidecar,
        modelContract: MODEL_CONTRACT,
        runtimeContract: RUNTIME_CONTRACT,
    }), errorCode("package_sha256_mismatch"));
});

test("active loader binds manifest, runtime contract, and member digests independently", async (t) => {
    const manifestMismatch = await createActivatedFixture(t);
    const manifestDir = join(manifestMismatch.ragDataDir, "26.1.1");
    await rewriteInstalledMember(manifestDir, "manifest.json", (manifest) => {
        manifest.kbVersion = "26.1.2";
    });
    await assert.rejects(
        loadActiveKnowledgePack(manifestMismatch.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("active_pointer_invalid"),
    );

    const contractMismatch = await createActivatedFixture(t);
    await mutateCanonicalFile(join(contractMismatch.ragDataDir, "26.1.1", "install.json"), (install) => {
        install.runtimeContractSha256 = "0".repeat(64);
    });
    await assert.rejects(
        loadActiveKnowledgePack(contractMismatch.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("active_pointer_invalid"),
    );

    const memberMismatch = await createActivatedFixture(t);
    await mutateCanonicalFile(join(memberMismatch.ragDataDir, "26.1.1", "install.json"), (install) => {
        install.memberSha256["manifest.json"] = "0".repeat(64);
    });
    await assert.rejects(
        loadActiveKnowledgePack(memberMismatch.ragDataDir, { modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("checksum_mismatch"),
    );
});

test("active pointer validator covers exact identity and previous-pointer branches", () => {
    const active = { directory: "26.1.1", kbVersion: "26.1.1", sha256: "1".repeat(64) };
    const previous = { directory: "26.1.2", kbVersion: "26.1.2", sha256: "2".repeat(64) };
    assert.doesNotThrow(() => validateActivePointer({ schemaVersion: "4.0", active, previous }));
    for (const pointer of [
        null,
        { schemaVersion: "3.0", active, previous: null },
        { schemaVersion: "4.0", active, previous: active },
        { schemaVersion: "4.0", active: { ...active, directory: "other" }, previous: null },
        { schemaVersion: "4.0", active: { ...active, kbVersion: "invalid" }, previous: null },
        { schemaVersion: "4.0", active: { ...active, sha256: "invalid" }, previous: null },
    ]) assert.throws(() => validateActivePointer(pointer), errorCode("active_pointer_invalid"));
});

test("install record validator rejects each identity, file-list, and fingerprint boundary", async (t) => {
    const fixture = await createActivatedFixture(t);
    const kbDir = join(fixture.ragDataDir, "26.1.1");
    const installPath = join(kbDir, "install.json");
    const original = JSON.parse(await readFile(installPath, "utf8"));
    const mutations = [
        (value) => { value.kbId = "wrong"; },
        (value) => { value.runtimeContractSha256 = "invalid"; },
        (value) => { value.installMode = "release"; },
        (value) => { value.package.sha256 = "invalid"; },
        (value) => { value.package.sizeBytes = 0; },
        (value) => { value.memberSha256["manifest.json"] = "invalid"; },
        (value) => { [value.files[0], value.files[1]] = [value.files[1], value.files[0]]; },
        (value) => { value.files[0].sizeBytes = -1; },
        (value) => { value.files[0].mtimeNs = "invalid"; },
    ];
    for (const mutate of mutations) {
        const changed = structuredClone(original);
        mutate(changed);
        await writeFile(installPath, canonicalJsonBytes(changed));
        await assert.rejects(readInstallRecord(kbDir));
    }
    await writeFile(installPath, canonicalJsonBytes(original));
});

test("knowledge loader can load the stable runtime contract from its fixed directory", async (t) => {
    const fixture = await createActivatedFixture(t);
    const pack = await loadKnowledgePack(join(fixture.ragDataDir, "26.1.1"), {
        runtimeDir: fileURLToPath(new URL("../../../../rag-runtime", import.meta.url)),
        modelContract: MODEL_CONTRACT,
    });

    assert.equal(pack.manifest.kbVersion, "26.1.1");
});

test("service status, pointer, staging, target, and transaction errors fail closed", async (t) => {
    const malformedStatus = await createFixture();
    t.after(() => rm(malformedStatus.root, { recursive: true, force: true }));
    await mkdir(malformedStatus.ragDataDir, { recursive: true });
    await writeFile(join(malformedStatus.ragDataDir, "active.json"), "invalid");
    assert.deepEqual(await createService(malformedStatus).getStatus(), {
        active: null,
        previous: null,
        error: "noncanonical_json",
    });

    const nonFileStatus = await createFixture();
    t.after(() => rm(nonFileStatus.root, { recursive: true, force: true }));
    await mkdir(join(nonFileStatus.ragDataDir, "active.json"), { recursive: true });
    assert.deepEqual(await createService(nonFileStatus).getStatus(), {
        active: null,
        previous: null,
        error: "active_pointer_invalid",
    });

    const pointerMismatch = await createActivatedFixture(t);
    await mutateCanonicalFile(join(pointerMismatch.ragDataDir, "active.json"), (pointer) => {
        pointer.active.sha256 = "0".repeat(64);
    });
    await assert.rejects(createService(pointerMismatch).verify(), errorCode("active_pointer_invalid"));

    const staleStage = await createFixture();
    t.after(() => rm(staleStage.root, { recursive: true, force: true }));
    const stale = join(staleStage.ragDataDir, ".staging", "stale_transaction");
    await mkdir(stale, { recursive: true });
    const preservedFile = join(staleStage.ragDataDir, ".staging", "sentinel.txt");
    const preservedDirectory = join(staleStage.ragDataDir, ".staging", "invalid.name");
    await writeFile(preservedFile, "preserve");
    await mkdir(preservedDirectory);
    await assert.rejects(createService(staleStage).importPackage("missing", { mode: "development" }));
    await assert.rejects(lstat(stale), { code: "ENOENT" });
    assert.equal((await lstat(preservedFile)).isFile(), true);
    assert.equal((await lstat(preservedDirectory)).isDirectory(), true);

    const blockedTarget = await createFixture();
    t.after(() => rm(blockedTarget.root, { recursive: true, force: true }));
    const handoff = await writePackageV4Handoff(blockedTarget.handoffs, { directory: "first" });
    await mkdir(blockedTarget.ragDataDir, { recursive: true });
    await writeFile(join(blockedTarget.ragDataDir, "26.1.1"), "not-a-directory");
    await assert.rejects(
        createService(blockedTarget).importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath }),
        errorCode("version_conflict"),
    );

    const invalidTransaction = await createFixture();
    t.after(() => rm(invalidTransaction.root, { recursive: true, force: true }));
    const transactionHandoff = await writePackageV4Handoff(invalidTransaction.handoffs, { directory: "first" });
    const service = createKnowledgePackageService({
        ragDataDir: invalidTransaction.ragDataDir,
        modelDir: invalidTransaction.modelDir,
        runtimeDir: invalidTransaction.runtimeDir,
        loadModelContract: async () => ({ manifest: MODEL_CONTRACT }),
        loadRuntime: async () => RUNTIME_CONTRACT,
        tempId: () => "***",
    });
    await assert.rejects(
        service.importPackage(transactionHandoff.archivePath, { mode: "development", sidecarPath: transactionHandoff.sidecarPath }),
        errorCode("configuration_error"),
    );
});

test("installed verification rejects archive and extracted identity drift", async (t) => {
    const archiveMismatch = await createActivatedFixture(t);
    const install = JSON.parse(await readFile(join(archiveMismatch.ragDataDir, "26.1.1", "install.json"), "utf8"));
    const archiveService = createInjectedService(archiveMismatch, {
        validateArchive: async () => ({ sha256: "0".repeat(64), sizeBytes: install.package.sizeBytes }),
    });
    await assert.rejects(archiveService.activate("26.1.1"), errorCode("package_sha256_mismatch"));

    const extractedMismatch = await createActivatedFixture(t);
    const extractedInstall = JSON.parse(await readFile(join(extractedMismatch.ragDataDir, "26.1.1", "install.json"), "utf8"));
    const extractedService = createInjectedService(extractedMismatch, {
        validateArchive: async () => ({
            sha256: extractedInstall.package.sha256,
            sizeBytes: extractedInstall.package.sizeBytes,
        }),
        loadInstalledPack: async () => ({
            manifest: { kbVersion: "26.1.2" },
            contract: { contractSha256: extractedInstall.runtimeContractSha256 },
            chunks: [],
        }),
    });
    await assert.rejects(extractedService.activate("26.1.1"), errorCode("pack_unreadable"));
});

test("archive validator normalizes Package semantic failures and rejects wrong basenames", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const directory = join(fixture.handoffs, "semantic");
    await mkdir(directory, { recursive: true });
    const members = createPackageV4Members();
    const manifest = JSON.parse(members.get("manifest.json"));
    manifest.schemaVersion = "3.0";
    members.set("manifest.json", canonicalJsonBytes(manifest));
    members.set("checksums.json", canonicalJsonBytes({
        schemaVersion: "4.0",
        algorithm: "sha256",
        files: Object.fromEntries(CHECKSUM_MEMBERS.map((name) => [name, sha256(members.get(name))])),
    }));
    const archive = createCanonicalZip(members);
    const archivePath = join(directory, "knowledge-pack-v4.zip");
    const sidecarPath = join(directory, "knowledge-pack-v4.zip.sha256");
    await writeFile(archivePath, archive);
    await writeFile(sidecarPath, `${sha256(archive)}  knowledge-pack-v4.zip\n`);
    await assert.rejects(
        validatePackageArchive({ archivePath, sidecarPath, modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("unsupported_package_schema"),
    );

    const wrongArchive = join(directory, "wrong.zip");
    const wrongSidecar = join(directory, "wrong.zip.sha256");
    await copyFile(archivePath, wrongArchive);
    await copyFile(sidecarPath, wrongSidecar);
    await assert.rejects(
        validatePackageArchive({ archivePath: wrongArchive, sidecarPath: wrongSidecar, modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
        errorCode("sidecar_invalid"),
    );
});

test("archive validator rejects oversized metadata, prefixes, and local-header drift", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const canonical = createCanonicalZip(createPackageV4Members());
    const eocd = canonical.length - 22;
    const centralOffset = canonical.readUInt32LE(eocd + 16);
    const oversized = Buffer.from(canonical);
    oversized.writeUInt32LE(512 * 1024 * 1024 + 1, centralOffset + 24);

    const prefixed = Buffer.concat([Buffer.from("x"), canonical]);
    const prefixedEocd = prefixed.length - 22;
    prefixed.writeUInt32LE(centralOffset + 1, prefixedEocd + 16);
    prefixed.writeUInt32LE(1, centralOffset + 1 + 42);

    const localVersion = Buffer.from(canonical);
    localVersion.writeUInt16LE(21, 4);
    const localName = Buffer.from(canonical);
    localName[30] ^= 1;

    for (const [name, archive] of [
        ["oversized", oversized],
        ["prefixed", prefixed],
        ["local-version", localVersion],
        ["local-name", localName],
    ]) {
        const directory = join(fixture.handoffs, name);
        await mkdir(directory, { recursive: true });
        const archivePath = join(directory, "knowledge-pack-v4.zip");
        const sidecarPath = join(directory, "knowledge-pack-v4.zip.sha256");
        await writeFile(archivePath, archive);
        await writeFile(sidecarPath, `${sha256(archive)}  knowledge-pack-v4.zip\n`);
        await assert.rejects(
            validatePackageArchive({ archivePath, sidecarPath, modelContract: MODEL_CONTRACT, runtimeContract: RUNTIME_CONTRACT }),
            (error) => ["archive_too_large", "invalid_archive"].includes(error?.code),
            name,
        );
    }
});

test("installed fingerprinting accepts regular files and rejects missing or non-file targets", async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    await mkdir(fixture.ragDataDir, { recursive: true });
    await writeFile(join(fixture.ragDataDir, "regular"), "bytes");
    await mkdir(join(fixture.ragDataDir, "directory"));

    assert.deepEqual(await fingerprintInstalledFile(fixture.ragDataDir, "regular"), {
        name: "regular",
        sizeBytes: 5,
        mtimeNs: (await lstat(join(fixture.ragDataDir, "regular"), { bigint: true })).mtimeNs.toString(),
    });
    await assert.rejects(fingerprintInstalledFile(fixture.ragDataDir, "missing"), errorCode("pack_unreadable"));
    await assert.rejects(fingerprintInstalledFile(fixture.ragDataDir, "directory"), errorCode("pack_unreadable"));
});

test("service normalizes injected coded, permission, and unknown receiver failures", async (t) => {
    for (const [error, expectedCode] of [
        [Object.assign(new Error("coded"), { code: "custom_receiver_error" }), "custom_receiver_error"],
        [Object.assign(new Error("denied"), { code: "EACCES" }), "rag_data_unwritable"],
        [new Error("unknown"), "invalid_archive"],
    ]) {
        const fixture = await createFixture();
        t.after(() => rm(fixture.root, { recursive: true, force: true }));
        const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
        const service = createInjectedService(fixture, {
            validateArchive: async () => { throw error; },
        });
        await assert.rejects(
            service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath }),
            errorCode(expectedCode),
        );
    }
});

test("service constructor requires all three fixed runtime paths", () => {
    for (const options of [
        { modelDir: "model", runtimeDir: "runtime" },
        { ragDataDir: "rag", runtimeDir: "runtime" },
        { ragDataDir: "rag", modelDir: "model" },
    ]) assert.throws(() => createKnowledgePackageService(options), errorCode("configuration_error"));
});

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-rag-lifecycle-"));
    return {
        root,
        ragDataDir: join(root, "rag-data"),
        handoffs: join(root, "handoffs"),
        modelDir: join(root, "model"),
        runtimeDir: join(root, "runtime"),
    };
};

const createService = (fixture) => createKnowledgePackageService({
    ragDataDir: fixture.ragDataDir,
    modelDir: fixture.modelDir,
    runtimeDir: fixture.runtimeDir,
    loadModelContract: async () => ({ modelDir: fixture.modelDir, manifest: MODEL_CONTRACT }),
    loadRuntime: async () => RUNTIME_CONTRACT,
    logger: { info: () => {} },
});

const createActivatedFixture = async (t) => {
    const fixture = await createFixture();
    t.after(() => rm(fixture.root, { recursive: true, force: true }));
    const handoff = await writePackageV4Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });
    await service.activate("26.1.1", { sha256: handoff.digest });
    return fixture;
};

const createInjectedService = (fixture, overrides) => createKnowledgePackageService({
    ragDataDir: fixture.ragDataDir,
    modelDir: fixture.modelDir,
    runtimeDir: fixture.runtimeDir,
    loadModelContract: async () => ({ manifest: MODEL_CONTRACT }),
    loadRuntime: async () => RUNTIME_CONTRACT,
    ...overrides,
});

const mutateCanonicalFile = async (path, mutate) => {
    const value = JSON.parse(await readFile(path, "utf8"));
    mutate(value);
    await writeFile(path, canonicalJsonBytes(value));
};

const rewriteInstalledMember = async (kbDir, name, mutate) => {
    const path = join(kbDir, name);
    const value = JSON.parse(await readFile(path, "utf8"));
    mutate(value);
    const bytes = canonicalJsonBytes(value);
    await writeFile(path, bytes);
    const checksumsPath = join(kbDir, "checksums.json");
    const checksums = JSON.parse(await readFile(checksumsPath, "utf8"));
    checksums.files[name] = sha256(bytes);
    const checksumBytes = canonicalJsonBytes(checksums);
    await writeFile(checksumsPath, checksumBytes);
    const installPath = join(kbDir, "install.json");
    const install = JSON.parse(await readFile(installPath, "utf8"));
    install.memberSha256[name] = sha256(bytes);
    install.memberSha256["checksums.json"] = sha256(checksumBytes);
    for (const changed of [name, "checksums.json"]) {
        const info = await lstat(join(kbDir, changed), { bigint: true });
        const fingerprint = install.files.find((file) => file.name === changed);
        fingerprint.sizeBytes = Number(info.size);
        fingerprint.mtimeNs = info.mtimeNs.toString();
    }
    await writeFile(installPath, canonicalJsonBytes(install));
};

const errorCode = (code) => (error) => {
    assert.equal(error?.code, code, error?.stack);
    return true;
};

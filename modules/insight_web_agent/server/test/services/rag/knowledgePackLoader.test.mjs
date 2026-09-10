/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import { readActiveKnowledgePointer } from "../../../services/rag/knowledgePackLoader.mjs";
import {
    createKnowledgePackageService,
} from "../../../services/rag/knowledgePackageService.mjs";
import {
    MODEL_CONTRACT,
} from "./packageFixture.mjs";
import {
    RUNTIME_CONTRACT_V5,
    writePackageV5Handoff,
} from "./packageBundleFixture.mjs";

const createService = (fixture) => createKnowledgePackageService({
    ragDataDir: fixture.ragDataDir,
    modelDir: fixture.modelDir,
    runtimeDir: fixture.runtimeDir,
    loadModelContract: async () => ({ modelDir: fixture.modelDir, manifest: MODEL_CONTRACT }),
    loadRuntime: async () => RUNTIME_CONTRACT_V5,
    logger: { info: () => {} },
});

const createFixture = async () => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-rag-loader-"));
    const ragDataDir = join(root, "rag-data");
    const modelDir = join(root, "models");
    const runtimeDir = join(root, "runtime");
    const handoffs = join(root, "handoffs");
    const { mkdir } = await import("node:fs/promises");
    await mkdir(ragDataDir, { recursive: true });
    await mkdir(modelDir, { recursive: true });
    await mkdir(runtimeDir, { recursive: true });
    await mkdir(handoffs, { recursive: true });
    return { root, ragDataDir, modelDir, runtimeDir, handoffs };
};

const importV5 = async (fixture, directory = "first") => {
    const handoff = await writePackageV5Handoff(fixture.handoffs, { directory });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });
    await service.activate("26.1.1");
    return { service, kbDir: join(fixture.ragDataDir, "26.1.1") };
};

const rewriteInstall = async (kbDir, mutate) => {
    const path = join(kbDir, "install.json");
    const payload = JSON.parse(await readFile(path, "utf8"));
    await writeFile(path, canonicalJsonBytes(mutate(JSON.parse(JSON.stringify(payload)))));
};

test("loader rejects tampered install identities digests and ordering", async () => {
    const { service, kbDir } = await importV5(await createFixture());
    const verified = await service.verify();
    assert.equal(verified.status, "verified");

    await rewriteInstall(kbDir, (payload) => ({ ...payload, kbVersion: "26.1.2" }));
    await assert.rejects(service.verify(), /identity|kbVersion|pointer/);
});

test("loader rejects tampered member digests", async () => {
    const { service, kbDir } = await importV5(await createFixture());
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        memberSha256: { ...payload.memberSha256, "manifest.json": "0".repeat(64) },
    }));
    await assert.rejects(service.verify(), /mismatch|checksum|digest|matches|member/);
});

test("loader rejects reordered fingerprints", async () => {
    const { service, kbDir } = await importV5(await createFixture());
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        files: [...payload.files].reverse(),
    }));
    await assert.rejects(service.verify(), /ordered|fingerprint/);
});

test("loader rejects invalid fingerprint payloads", async () => {
    const { service, kbDir } = await importV5(await createFixture());
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        files: payload.files.map((file) => ({ ...file, mtimeNs: "yesterday" })),
    }));
    await assert.rejects(service.verify(), /fingerprint|mtime/);
});

test("loader rejects tampered bundle sections", async () => {
    const { service, kbDir } = await importV5(await createFixture());
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        bundle: { ...payload.bundle, status: "bogus" },
    }));
    await assert.rejects(service.verify(), /bundle|status/);
});

test("loader rejects bundle entries with secret values", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        bundle: {
            ...payload.bundle,
            mcps: [{ name: "svc", sourceId: "group/repo", rootPath: "app", entryPath: "app/svc.py", runtime: "python", transport: "stdio", arguments: [], environment: ["HAS_VALUE=1"] }],
        },
    }));
    await assert.rejects(service.verify(), /environment|variable|names/);
});

test("loader rejects bundle entries with invalid shapes", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        bundle: { ...payload.bundle, skills: [{ name: "" }] },
    }));
    await assert.rejects(service.verify(), /bundle|skill|invalid/);
});

test("loader rejects bundle records with non-array sections", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        bundle: { ...payload.bundle, skills: {} },
    }));
    await assert.rejects(service.verify(), /bundle|record is invalid/);
});

test("loader rejects bundle entries with invalid skip and override shapes", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    await rewriteInstall(kbDir, (payload) => ({
        ...payload,
        bundle: {
            ...payload.bundle,
            skipped: [{ kind: "nope", id: "x", rootPath: "skills/x", reasonCode: "y" }],
        },
    }));
    await assert.rejects(service.verify(), /bundle|skip is invalid/);

    const fixture2 = await createFixture();
    const second = await importV5(fixture2);
    await rewriteInstall(second.kbDir, (payload) => ({
        ...payload,
        bundle: { ...payload.bundle, overridden: ["demo", 42] },
    }));
    await assert.rejects(second.service.verify(), /bundle|override is invalid/);
});

test("loader rejects unreadable and non-v5 installed manifests", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    const manifestPath = join(kbDir, "manifest.json");
    const original = await readFile(manifestPath);
    await writeFile(manifestPath, Buffer.from("not-json\n", "utf8"));
    await assert.rejects(service.verify(), /not readable|manifest/);
    const downgraded = Buffer.from(original.toString("utf8").replace(
        '"schemaVersion":"5.0"',
        '"schemaVersion":"4.0"',
    ));
    assert.equal(downgraded.length, original.length);
    await writeFile(manifestPath, downgraded);
    await assert.rejects(service.verify(), /must be 5\.0|schemaVersion/);
});

test("loader rejects unexpected kb entries and escaping pointers", async () => {
    const fixture = await createFixture();
    const { service, kbDir } = await importV5(fixture);
    await writeFile(join(kbDir, "stray.txt"), "stray");
    await assert.rejects(service.verify(), /unexpected|closure/);
    await rm(join(kbDir, "stray.txt"));

    const pointerDir = await mkdtemp(join(tmpdir(), "msinsight-pointer-"));
    await writeFile(
        join(pointerDir, "active.json"),
        canonicalJsonBytes({ schemaVersion: "4.0", active: { kbVersion: "26.1.1", sha256: "a".repeat(64), directory: ".." }, previous: null }),
    );
    await assert.rejects(readActiveKnowledgePointer(pointerDir), /outside|escapes|directory/);
});

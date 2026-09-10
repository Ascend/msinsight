/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { mkdtemp, readFile, stat } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { createKnowledgePackageService } from "../../../services/rag/knowledgePackageService.mjs";
import {
    MODEL_CONTRACT,
    createPackageV4Members,
    sha256,
} from "./packageFixture.mjs";
import {
    RUNTIME_CONTRACT_V5,
    createPackageV5Members,
    emptyBundleVector,
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
    const root = await mkdtemp(join(tmpdir(), "msinsight-rag-v5-"));
    const ragDataDir = join(root, "rag-data");
    const modelDir = join(root, "models");
    const runtimeDir = join(root, "runtime");
    const handoffs = join(root, "handoffs");
    const { mkdir } = await import("node:fs/promises");
    await mkdir(ragDataDir, { recursive: true });
    await mkdir(modelDir, { recursive: true });
    await mkdir(runtimeDir, { recursive: true });
    await mkdir(handoffs, { recursive: true });
    await mkdir(join(runtimeDir, "contract"), { recursive: true });
    return { root, ragDataDir, modelDir, runtimeDir, handoffs };
};

test("v5 import validates the envelope and stores the package without activating", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV5Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);

    const imported = await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });

    assert.equal(imported.status, "imported");
    assert.equal(imported.sha256, handoff.digest);
    assert.equal((await service.getStatus()).active, null);
    const install = JSON.parse(await readFile(join(fixture.ragDataDir, "26.1.1", "install.json"), "utf8"));
    assert.equal(install.schemaVersion, "1.1");
    assert.equal(install.package.sha256, handoff.digest);
    assert.ok(Array.isArray(install.bundle.skills));
});

test("v5 import rejects envelope mutations", async () => {
    const fixture = await createFixture();
    const service = createService(fixture);
    const members = createPackageV5Members();
    const names = [...members.keys()];

    const swapped = new Map(members);
    const order = [...names];
    [order[7], order[8]] = [order[8], order[7]];
    const { createCanonicalZip } = await import("./packageFixture.mjs");
    const badOrder = createCanonicalZip(swapped, { names: order });
    const badDir = join(fixture.handoffs, "bad-order");
    const { mkdir, writeFile } = await import("node:fs/promises");
    await mkdir(badDir, { recursive: true });
    await writeFile(join(badDir, "knowledge-pack-v5.zip"), badOrder);
    await writeFile(join(badDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(badOrder)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(
        service.importPackage(join(badDir, "knowledge-pack-v5.zip"), { mode: "development", sidecarPath: join(badDir, "knowledge-pack-v5.zip.sha256") }),
        /order|entries/,
    );

    const dropped = new Map([...members.entries()].filter(([name]) => name !== "skills.jsonl"));
    const missing = createCanonicalZip(dropped, { names: names.filter((name) => name !== "skills.jsonl") });
    const missingDir = join(fixture.handoffs, "missing");
    await mkdir(missingDir, { recursive: true });
    await writeFile(join(missingDir, "knowledge-pack-v5.zip"), missing);
    await writeFile(join(missingDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(missing)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(
        service.importPackage(join(missingDir, "knowledge-pack-v5.zip"), { mode: "development", sidecarPath: join(missingDir, "knowledge-pack-v5.zip.sha256") }),
        /entries|member|order|checksum|schema/,
    );
});

test("v5 import rejects symlink payload members", async () => {    const fixture = await createFixture();
    const service = createService(fixture);
    const members = createPackageV5Members();
    const modes = {};
    for (const name of members.keys()) modes[name] = 0o100644 << 16;
    modes["bundles/group/repo/tools/run.sh"] = 0o120777 << 16;
    const { createCanonicalZip } = await import("./packageFixture.mjs");
    const { PACKAGE_V5_METADATA_MEMBERS, PACKAGE_V5_SUFFIX_MEMBERS } = await import("./packageBundleFixture.mjs");
    const names = [...PACKAGE_V5_METADATA_MEMBERS, ...[...members.keys()].filter((name) => name.startsWith("bundles/")).sort(), ...PACKAGE_V5_SUFFIX_MEMBERS];
    const archive = createCanonicalZip(members, { names, externalAttributesByName: modes });
    const directory = join(fixture.handoffs, "symlink");
    const { mkdir, writeFile } = await import("node:fs/promises");
    await mkdir(directory, { recursive: true });
    await writeFile(join(directory, "knowledge-pack-v5.zip"), archive);
    await writeFile(join(directory, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(archive)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(
        service.importPackage(join(directory, "knowledge-pack-v5.zip"), { mode: "development", sidecarPath: join(directory, "knowledge-pack-v5.zip.sha256") }),
        /mode|symlink|metadata/,
    );
});

test("v5 import rejects v4-shaped content claiming v5", async () => {    const fixture = await createFixture();
    const service = createService(fixture);
    const v4members = createPackageV4Members();
    const { createCanonicalZip } = await import("./packageFixture.mjs");
    const { canonicalJsonBytes } = await import("../../../services/rag/wire/canonicalJson.mjs");
    const fakeManifest = { ...JSON.parse(v4members.get("manifest.json").toString("utf8")), schemaVersion: "5.0" };
    v4members.set("manifest.json", canonicalJsonBytes(fakeManifest));
    const forged = createCanonicalZip(v4members, { names: [...v4members.keys()] });
    const forgedDir = join(fixture.handoffs, "forged");
    const { mkdir, writeFile } = await import("node:fs/promises");
    await mkdir(forgedDir, { recursive: true });
    await writeFile(join(forgedDir, "knowledge-pack-v5.zip"), forged);
    await writeFile(join(forgedDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(forged)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(
        service.importPackage(join(forgedDir, "knowledge-pack-v5.zip"), { mode: "development", sidecarPath: join(forgedDir, "knowledge-pack-v5.zip.sha256") }),
        /schema|member|bundle|checksum/,
    );
});

test("activate installs skills and mcps into the workspace without executing payload", async () => {
    const fixture = await createFixture();
    const evilMembers = createPackageV5Members();
    const canary = Buffer.from("throw new Error('executed');");
    evilMembers.set("bundles/group/repo/tools/evil.js", canary);
    const handoff = await writePackageV5Handoff(fixture.handoffs, { directory: "first", members: evilMembers });
    const service = createService(fixture);
    // NOTE: canary bytes are not referenced by any record, so the envelope
    // checksum gate rejects them before activation. A second canary rides
    // inside a recorded payload file below.
    await assert.rejects(
        service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath }),
        /checksum|cover/,
    );

    const clean = await writePackageV5Handoff(fixture.handoffs, { directory: "clean" });
    await service.importPackage(clean.archivePath, { mode: "development", sidecarPath: clean.sidecarPath });
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-agent-workspace-"));
    const activated = await service.activate("26.1.1", { agentWorkspacePath: workspace });

    assert.equal(activated.status, "activated");
    assert.deepEqual(activated.bundle.skills, ["demo"]);
    assert.deepEqual(activated.bundle.mcps, ["svc"]);
    assert.equal(activated.bundle.skipped.length, 1);
    assert.equal(
        await readFile(join(workspace, ".agents", "skills", "demo", "SKILL.md"), "utf8"),
        "# Demo skill.",
    );
    assert.equal(
        await readFile(join(workspace, ".agents", "mcps", "svc", "svc.py"), "utf8"),
        "# svc",
    );
    assert.ok(activated.bundle.registry.mcps[0].environment.every((entry) => !entry.includes("=")));
});

test("activate without a workspace records the skip and still switches the pointer", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV5Handoff(fixture.handoffs, { directory: "first" });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });

    const activated = await service.activate("26.1.1");

    assert.equal(activated.status, "activated");
    assert.equal(activated.bundle.workspace, "skipped_no_workspace");
});

test("activate installs empty bundles as a no-op", async () => {
    const fixture = await createFixture();
    const handoff = await writePackageV5Handoff(fixture.handoffs, {
        directory: "empty",
        members: createPackageV5Members({ bundle: emptyBundleVector() }),
    });
    const service = createService(fixture);
    await service.importPackage(handoff.archivePath, { mode: "development", sidecarPath: handoff.sidecarPath });
    const workspace = await mkdtemp(join(tmpdir(), "msinsight-agent-workspace-"));

    const activated = await service.activate("26.1.1", { agentWorkspacePath: workspace });

    assert.equal(activated.status, "activated");
    assert.deepEqual(activated.bundle.skills, []);
    assert.deepEqual(activated.bundle.mcps, []);
});

test("v5 envelope matrix pins names order modes and sizes", async () => {
    const fixture = await createFixture();
    const service = createService(fixture);
    const { createCanonicalZip } = await import("./packageFixture.mjs");
    const { PACKAGE_V5_METADATA_MEMBERS, PACKAGE_V5_SUFFIX_MEMBERS } = await import("./packageBundleFixture.mjs");
    const { mkdir, writeFile } = await import("node:fs/promises");
    const writeMutant = async (label, members, names, modes) => {
        const archive = createCanonicalZip(members, { names, ...(modes ? { externalAttributesByName: modes } : {}) });
        const directory = join(fixture.handoffs, label);
        await mkdir(directory, { recursive: true });
        await writeFile(join(directory, "knowledge-pack-v5.zip"), archive);
        await writeFile(join(directory, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(archive)}  knowledge-pack-v5.zip\n`, "ascii"));
        return directory;
    };
    const importMutant = (directory) => service.importPackage(
        join(directory, "knowledge-pack-v5.zip"),
        { mode: "development", sidecarPath: join(directory, "knowledge-pack-v5.zip.sha256") },
    );
    const base = createPackageV5Members();
    const payloadNames = [...base.keys()].filter((name) => name.startsWith("bundles/")).sort();
    const plan = [...PACKAGE_V5_METADATA_MEMBERS, ...payloadNames, ...PACKAGE_V5_SUFFIX_MEMBERS];

    const unsorted = await writeMutant("unsorted-payload", base, [
        ...PACKAGE_V5_METADATA_MEMBERS,
        ...[...payloadNames].reverse(),
        ...PACKAGE_V5_SUFFIX_MEMBERS,
    ]);
    await assert.rejects(importMutant(unsorted), /sorted|order/);

    const duped = await writeMutant("dup-member", base, [...plan.slice(0, 8), "skills.jsonl", ...plan.slice(8)]);
    await assert.rejects(importMutant(duped), /duplicate|order|entries/);

    const folded = new Map(base);
    const foldedNames = [];
    for (const name of plan) {
        foldedNames.push(name);
        if (name === "skills.jsonl") {
            foldedNames.push("SKILLS.JSONL");
            folded.set("SKILLS.JSONL", folded.get("skills.jsonl"));
        }
    }
    const foldedDir = await writeMutant("casefold", folded, foldedNames);
    await assert.rejects(importMutant(foldedDir), /case-fold|order|entries/);

    const evil = new Map(base);
    evil.set("/absolute.txt", Buffer.from("{}"));
    const evilDir = await writeMutant("absolute", evil, [...plan.slice(0, 11), "/absolute.txt", ...plan.slice(11)]);
    await assert.rejects(importMutant(evilDir), /unsafe|order|entries|absolute|strict/);

    const trailing = new Map(base);
    trailing.set("bundles/group/repo/dir/", Buffer.alloc(0));
    const trailingDir = await writeMutant("trailing", trailing, [...plan.slice(0, 13), "bundles/group/repo/dir/", ...plan.slice(13)]);
    await assert.rejects(importMutant(trailingDir), /unsafe|order|entries|payload/);

    const canonical = createCanonicalZip(base, { names: plan });
    const eocd = canonical.length - 22;
    const centralOffset = canonical.readUInt32LE(eocd + 16);
    const oversized = Buffer.from(canonical);
    oversized.writeUInt32LE(512 * 1024 * 1024 + 1, centralOffset + 24);
    const oversizedDir = join(fixture.handoffs, "oversized");
    await mkdir(oversizedDir, { recursive: true });
    await writeFile(join(oversizedDir, "knowledge-pack-v5.zip"), oversized);
    await writeFile(join(oversizedDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(oversized)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(importMutant(oversizedDir), /too_large|archive|size/);

    const badMadeBy = Buffer.from(canonical);
    badMadeBy.writeUInt16LE(0, centralOffset + 4);
    const badMadeByDir = join(fixture.handoffs, "madeby");
    await mkdir(badMadeByDir, { recursive: true });
    await writeFile(join(badMadeByDir, "knowledge-pack-v5.zip"), badMadeBy);
    await writeFile(join(badMadeByDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(badMadeBy)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(importMutant(badMadeByDir), /noncanonical|metadata|archive/);

    const emptyDir = join(fixture.handoffs, "empty");
    await mkdir(emptyDir, { recursive: true });
    await writeFile(join(emptyDir, "knowledge-pack-v5.zip"), Buffer.alloc(0));
    await writeFile(join(emptyDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(Buffer.alloc(0))}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(importMutant(emptyDir), /size|range|archive|invalid/);

    const { canonicalJsonBytes } = await import("../../../services/rag/wire/canonicalJson.mjs");
    const refix = (members) => {
        const digests = {};
        for (const name of [...members.keys()].filter((key) => key !== "checksums.json").sort()) {
            digests[name] = sha256(members.get(name));
        }
        members.set("checksums.json", canonicalJsonBytes({ schemaVersion: "4.0", algorithm: "sha256", files: digests }));
        return members;
    };
    const downgraded = new Map(base);
    const manifest = { ...JSON.parse(base.get("manifest.json").toString("utf8")), schemaVersion: "4.0" };
    downgraded.set("manifest.json", canonicalJsonBytes(manifest));
    const downgradedDir = await writeMutant("downgraded", refix(downgraded), plan);
    await assert.rejects(importMutant(downgradedDir), /schema|checksum|package/);

    const noVersion = new Map(base);
    const { schemaVersion: _dropped, ...manifestNoVersion } = JSON.parse(base.get("manifest.json").toString("utf8"));
    noVersion.set("manifest.json", canonicalJsonBytes(manifestNoVersion));
    const noVersionDir = await writeMutant("no-version", refix(noVersion), plan);
    await assert.rejects(importMutant(noVersionDir), /schema|checksum|package|manifest/);
});

test("v5 envelope matrix pins unsafe names payload coverage and local headers", async () => {
    const fixture = await createFixture();
    const service = createService(fixture);
    const { createCanonicalZip } = await import("./packageFixture.mjs");
    const { PACKAGE_V5_METADATA_MEMBERS, PACKAGE_V5_SUFFIX_MEMBERS } = await import("./packageBundleFixture.mjs");
    const { mkdir, writeFile } = await import("node:fs/promises");
    const base = createPackageV5Members();
    const payloadNames = [...base.keys()].filter((name) => name.startsWith("bundles/")).sort();
    const plan = [...PACKAGE_V5_METADATA_MEMBERS, ...payloadNames, ...PACKAGE_V5_SUFFIX_MEMBERS];
    const writeMutant = async (label, members, names, modes) => {
        const archive = createCanonicalZip(members, { names, ...(modes ? { externalAttributesByName: modes } : {}) });
        const directory = join(fixture.handoffs, label);
        await mkdir(directory, { recursive: true });
        await writeFile(join(directory, "knowledge-pack-v5.zip"), archive);
        await writeFile(join(directory, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(archive)}  knowledge-pack-v5.zip\n`, "ascii"));
        return directory;
    };
    const importMutant = (directory) => service.importPackage(
        join(directory, "knowledge-pack-v5.zip"),
        { mode: "development", sidecarPath: join(directory, "knowledge-pack-v5.zip.sha256") },
    );

    for (const [label, evil] of [["backslash", "bundles\\evil.txt"], ["dotdot", "bundles/a/../../evil.txt"]]) {
        const members = new Map(base);
        members.set(evil, Buffer.from("{}"));
        const directory = await writeMutant(`name-${label}`, members, [...plan.slice(0, 11), evil, ...plan.slice(11)]);
        await assert.rejects(importMutant(directory), /unsafe|strict|entries|archive|invalid/);
    }

    const extra = new Map(base);
    extra.set("extra.json", Buffer.from("{}"));
    const extraDir = await writeMutant("extra-member", extra, [...plan.slice(0, 11), "extra.json", ...plan.slice(11)]);
    await assert.rejects(importMutant(extraDir), /order|entries|fixed|payload/);

    const short = new Map([...base.entries()].filter(([name]) => name !== "checksums.json"));
    const shortDir = await writeMutant(
        "drop-checksums",
        short,
        plan.filter((name) => name !== "checksums.json"),
    );
    await assert.rejects(importMutant(shortDir), /order|entries|checksum/);

    const canonical = createCanonicalZip(base, { names: plan });
    const eocd = canonical.length - 22;
    const centralOffset = canonical.readUInt32LE(eocd + 16);
    const badEocd = Buffer.from(canonical);
    badEocd.writeUInt32LE(centralOffset + 1, eocd + 16);
    const badEocdDir = join(fixture.handoffs, "bad-eocd");
    await mkdir(badEocdDir, { recursive: true });
    await writeFile(join(badEocdDir, "knowledge-pack-v5.zip"), badEocd);
    await writeFile(join(badEocdDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(badEocd)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(importMutant(badEocdDir), /central|prefix|trailer|archive/);

    const badLocal = Buffer.from(canonical);
    badLocal.writeUInt16LE(21, 4);
    const badLocalDir = join(fixture.handoffs, "bad-local");
    await mkdir(badLocalDir, { recursive: true });
    await writeFile(join(badLocalDir, "knowledge-pack-v5.zip"), badLocal);
    await writeFile(join(badLocalDir, "knowledge-pack-v5.zip.sha256"), Buffer.from(`${sha256(badLocal)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(importMutant(badLocalDir), /local|archive/);

    const sparseDir = join(fixture.handoffs, "sparse");
    await mkdir(sparseDir, { recursive: true });
    const sparse = join(sparseDir, "knowledge-pack-v5.zip");
    const { writeFileSync, truncateSync } = await import("node:fs");
    writeFileSync(sparse, Buffer.alloc(0));
    truncateSync(sparse, 512 * 1024 * 1024 + 1);
    const sparseSidecar = join(sparseDir, "knowledge-pack-v5.zip.sha256");
    await writeFile(sparseSidecar, Buffer.from(`${"0".repeat(64)}  knowledge-pack-v5.zip\n`, "ascii"));
    await assert.rejects(
        service.importPackage(sparse, { mode: "development", sidecarPath: sparseSidecar }),
        /too_large|size|range|archive|mismatch/,
    );
});

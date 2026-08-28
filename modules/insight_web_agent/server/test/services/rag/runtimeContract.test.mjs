/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { access, cp, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { test } from "node:test";
import {
    loadRuntimeContract,
    validateRuntimeContract,
} from "../../../services/rag/runtimeContract.mjs";

const RUNTIME_DIR = fileURLToPath(new URL("../../../../rag-runtime", import.meta.url));
const CONTRACT_URL = new URL("../../../../rag-runtime/runtime-contract.json", import.meta.url);

test("one stable runtime contract contains no producer Profile, model, or Audit policy", async () => {
    const loaded = await loadRuntimeContract(RUNTIME_DIR);
    const serialized = JSON.stringify(loaded.contract);

    assert.match(loaded.contract.contractSha256, /^[0-9a-f]{64}$/);
    assert.equal(loaded.contract.packageSchemaVersion, "4.0");
    assert.equal(loaded.contract.packageMembers.includes("bm25-domain-dict.txt"), true);
    assert.equal("sharedAssets" in loaded.contract, false);
    for (const forbidden of [
        "profileId",
        "profileSha256",
        "allowedModels",
        "defaultModel",
        "opencodeVersion",
        "promptVersion",
        "buildScope",
        "releaseEligible",
    ]) assert.equal(serialized.includes(forbidden), false, forbidden);
    for (const name of ["bm25-domain-dict.txt", "bm25-tokenizer-fixture.json", "rerank-context-v1.json", "semantic-input-v1.json"]) {
        await assert.rejects(access(new URL(`../../../../rag-runtime/${name}`, import.meta.url)));
    }
    await access(new URL("./fixtures/bm25-tokenizer-fixture.json", import.meta.url));
    await access(new URL("./fixtures/rerank-context-v1.json", import.meta.url));
    await assert.rejects(access(new URL("../../../../rag-runtime/contracts/contract-index.json", import.meta.url)));
    await assert.rejects(access(new URL("../../../../rag-runtime/contracts/msinsight-default-consumer-contract.json", import.meta.url)));
});

test("runtime contract exact fields and local safety limits are fixed", async () => {
    const contract = JSON.parse(await readFile(CONTRACT_URL, "utf8"));
    assert.doesNotThrow(() => validateRuntimeContract(contract));
    assert.deepEqual(contract.resourceLimits, {
        maxArchiveBytes: 536870912,
        maxJsonlLineBytes: 400000,
        maxMemberBytes: 536870912,
        maxUncompressedBytes: 1073741824,
    });
    assert.equal(contract.embedding.dimension, 512);

    for (const mutate of [
        (value) => { value.future = true; },
        (value) => { value.packageSchemaVersion = "3.0"; },
        (value) => { value.embedding.dimension = 384; },
        (value) => { value.resourceLimits.maxArchiveBytes = 0; },
        (value) => { value.packageMembers.pop(); },
    ]) {
        const changed = structuredClone(contract);
        mutate(changed);
        assert.throws(() => validateRuntimeContract(changed));
    }
});

test("runtime contract loader rejects missing and malformed runtime files", async (t) => {
    await assert.rejects(loadRuntimeContract(""), /directory is required/);
    await assert.rejects(loadRuntimeContract(join(RUNTIME_DIR, "missing")), /Unable to read RAG runtime contract/);

    const fixture = await mkdtemp(join(tmpdir(), "rag-runtime-contract-"));
    t.after(() => rm(fixture, { recursive: true, force: true }));
    await cp(RUNTIME_DIR, fixture, { recursive: true });
    await writeFile(join(fixture, "runtime-contract.json"), "not-json", "utf8");
    await assert.rejects(loadRuntimeContract(fixture), /Unable to parse runtime contract/);
});

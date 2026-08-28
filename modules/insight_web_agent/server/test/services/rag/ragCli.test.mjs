/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { test } from "node:test";
import { run } from "../../../rag-cli.mjs";

const output = () => {
    const lines = [];
    return { lines, logger: { log: (line) => lines.push(line) } };
};

test("RAG CLI help exposes development lifecycle without seed or data-root controls", async () => {
    const captured = output();

    assert.equal(await run({ args: ["--help"], output: captured.logger }), 0);

    assert.match(captured.lines[0], /--mode development/);
    assert.match(captured.lines[0], /knowledge-pack-v4\.zip/);
    assert.doesNotMatch(captured.lines[0], /provision-seed|--data-dir|--mode release/);
});

test("RAG CLI imports development Package with adjacent canonical sidecar", async () => {
    const captured = output();
    const calls = [];
    const service = {
        async importPackage(pack, options) {
            calls.push({ pack, options });
            return { status: "imported", version: "26.1.1", installMode: "development-local" };
        },
    };

    await run({
        args: ["import", "--mode", "development", "--pack", "C:/handoff/knowledge-pack-v4.zip"],
        output: captured.logger,
        service,
    });

    assert.deepEqual(calls, [{
        pack: "C:/handoff/knowledge-pack-v4.zip",
        options: {
            mode: "development",
            sidecarPath: "C:/handoff/knowledge-pack-v4.zip.sha256",
        },
    }]);
});

test("RAG CLI activate, status, verify, and rollback require no data-root option", async () => {
    const calls = [];
    const service = {
        async activate(version, options) {
            calls.push(["activate", version, options]);
            return { status: "activated" };
        },
        async getStatus() {
            calls.push(["status"]);
            return { active: null, installMode: "development-local" };
        },
        async verify() {
            calls.push(["verify"]);
            return { status: "verified", installMode: "development-local" };
        },
        async rollback() {
            calls.push(["rollback"]);
            return { status: "rolled_back" };
        },
    };

    await run({ args: ["activate", "--version", "26.1.1", "--sha256", "a".repeat(64)], service, output: { log() {} } });
    await run({ args: ["status"], service, output: { log() {} } });
    await run({ args: ["verify"], service, output: { log() {} } });
    await run({ args: ["rollback"], service, output: { log() {} } });

    assert.deepEqual(calls, [
        ["activate", "26.1.1", { sha256: "a".repeat(64) }],
        ["status"],
        ["verify"],
        ["rollback"],
    ]);
});

test("RAG CLI rejects release, missing mode, data-dir, seed, duplicate, and unknown options", async () => {
    for (const args of [
        ["import", "--pack", "pack.zip"],
        ["import", "--mode", "release", "--pack", "pack.zip"],
        ["import", "--mode", "development", "--pack", "pack.zip", "--data-dir", "other"],
        ["provision-seed", "--seed-dir", "seed"],
        ["status", "--data-dir", "other"],
        ["status", "--unknown", "value"],
        ["status", "--unknown"],
        ["status", "unknown", "value"],
        ["import", "--mode", "development", "--pack", "--sidecar"],
        ["activate", "--version", "  ", "--sha256", "a".repeat(64)],
        ["activate", "--version", "26.1.1", "--version", "26.1.2", "--sha256", "a".repeat(64)],
    ]) await assert.rejects(run({ args, service: {} }), /Usage:/);
});

test("RAG CLI executable returns stable nonzero JSON for invalid input", () => {
    const entry = fileURLToPath(new URL("../../../rag-cli.mjs", import.meta.url));
    const result = spawnSync(process.execPath, [entry, "unknown-command"], { encoding: "utf8" });

    assert.equal(result.status, 1);
    assert.equal(JSON.parse(result.stderr).error.code, "rag_cli_failed");
});

test("RAG CLI supports implicit help and an explicit import sidecar", async () => {
    const captured = output();
    assert.equal(await run({ args: [], output: captured.logger }), 0);
    const calls = [];
    await run({
        args: ["import", "--mode", "development", "--pack", "pack.zip", "--sidecar", "pack.sha256"],
        output: { log() {} },
        service: {
            async importPackage(pack, options) {
                calls.push({ pack, options });
                return { status: "imported" };
            },
        },
    });
    assert.deepEqual(calls[0].options, { mode: "development", sidecarPath: "pack.sha256" });
});

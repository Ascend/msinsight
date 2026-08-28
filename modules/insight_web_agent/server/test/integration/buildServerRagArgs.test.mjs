/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { access, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));
const script = join(packageRoot, "scripts", "build-server.mjs");
const outputDir = join(packageRoot, "dist-server");
const ragEnvironmentNames = ["MSINSIGHT_RAG_PACKAGE", "MSINSIGHT_RAG_PACKAGE_SHA256", "MSINSIGHT_RAG_MODEL_DIR"];

const cleanEnvironment = () => {
    const env = { ...process.env };
    for (const name of ragEnvironmentNames) delete env[name];
    return env;
};

test("RAG build input failures preserve existing output before preflight", async (t) => {
    const sentinel = join(outputDir, ".preflight-sentinel");
    await writeFile(sentinel, "preserve");
    t.after(() => rm(sentinel, { force: true }));

    const cases = [
        { args: ["--rag-dev-pack", "pack.zip"] },
        { args: ["--rag-pack", "pack.zip"] },
        { args: ["--data-dir", "other"] },
        { args: ["--build-report", "report.json"] },
        {
            args: [
                "--rag-dev-pack", "one.zip",
                "--rag-dev-pack", "two.zip",
                "--rag-dev-sidecar", "pack.sha256",
                "--rag-model-dir", "model",
            ],
        },
        {
            args: ["--rag-dev-pack", "pack.zip", "--rag-dev-sidecar", "pack.sha256", "--rag-model-dir", "model"],
            error: /deprecated/,
        },
        {
            args: [],
            env: { MSINSIGHT_RAG_PACKAGE: "pack.zip" },
            error: /MSINSIGHT_RAG_PACKAGE_SHA256/,
        },
        {
            args: ["--rag-dev-pack", "legacy.zip", "--rag-dev-sidecar", "legacy.sha256", "--rag-model-dir", "legacy-model"],
            env: { MSINSIGHT_RAG_PACKAGE: "environment.zip" },
            error: /override deprecated/,
        },
    ];
    for (const testCase of cases) {
        const result = spawnSync(process.execPath, [script, ...testCase.args], {
            cwd: packageRoot,
            env: { ...cleanEnvironment(), ...(testCase.env ?? {}) },
            encoding: "utf8",
        });
        assert.notEqual(result.status, 0, testCase.args.join(" "));
        if (testCase.error) assert.match(result.stderr, testCase.error);
        await access(sentinel);
    }
});

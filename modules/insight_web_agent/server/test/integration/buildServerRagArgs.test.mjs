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
const ragEnvironmentNames = ["MSINSIGHT_RAG_MODE", "MSINSIGHT_RAG_PACKAGE", "MSINSIGHT_RAG_PACKAGE_SHA256", "MSINSIGHT_RAG_MODEL_DIR"];
const targetEnvironmentNames = ["MSINSIGHT_RAG_TARGET_PLATFORM", "MSINSIGHT_RAG_TARGET_ARCH", "MSINSIGHT_RAG_TARGET_LIBC"];

const cleanEnvironment = () => {
    const env = { ...process.env };
    for (const name of [...ragEnvironmentNames, ...targetEnvironmentNames]) delete env[name];
    return env;
};

test("RAG build input failures preserve existing output before preflight", async (t) => {
    const sentinel = join(outputDir, ".preflight-sentinel");
    await writeFile(sentinel, "preserve");
    t.after(() => rm(sentinel, { force: true }));

    const cases = [
        { args: ["--rag-dev-pack", "pack.zip"], error: /Unknown server build option: --rag-dev-pack/ },
        { args: ["--rag-pack", "pack.zip"], error: /no command-line options/ },
        { args: ["--data-dir", "other"], error: /no command-line options/ },
        { args: ["--build-report", "report.json"], error: /no command-line options/ },
        { args: ["--", "--rag-dev-pack", "pack.zip"], error: /Unknown server build option: --/ },
        {
            args: [],
            env: { MSINSIGHT_RAG_PACKAGE: "pack.zip" },
            error: /MSINSIGHT_RAG_PACKAGE_SHA256/,
        },
        {
            args: [],
            env: { MSINSIGHT_RAG_MODE: "release" },
            error: /Unsupported RAG build mode/,
        },
        {
            args: [],
            env: { MSINSIGHT_RAG_MODE: "product-bundled" },
            error: /MSINSIGHT_RAG_PACKAGE/,
        },
        {
            args: [],
            env: { MSINSIGHT_RAG_TARGET_PLATFORM: "freebsd", MSINSIGHT_RAG_TARGET_ARCH: "x64" },
            error: /Unsupported RAG platform/,
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

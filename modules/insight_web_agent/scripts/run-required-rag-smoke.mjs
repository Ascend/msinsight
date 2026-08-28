/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, lstatSync } from "node:fs";
import { isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const packageRoot = fileURLToPath(new URL("../", import.meta.url));
const distDir = resolve(process.env.MSINSIGHT_DIST_SERVER_DIR ?? join(packageRoot, "dist-server"));
const required = [
    "rag-required-smoke.mjs",
    "rag-build-mode.json",
    "rag-data/active.json",
    "rag-runtime/models/bge-small-zh-v1.5/model-manifest.json",
    "rag-runtime/models/bge-small-zh-v1.5/onnx/model.onnx",
    "node_modules/onnxruntime-node/bin/napi-v6/win32/x64/onnxruntime_binding.node",
    "node_modules/onnxruntime-node/bin/napi-v6/win32/x64/onnxruntime.dll",
];

try {
    assert.equal(isAbsolute(distDir), true);
    for (const relativePath of required) {
        const path = join(distDir, relativePath);
        assert.equal(existsSync(path), true, `required packaged-smoke input is missing: ${relativePath}`);
        assert.equal(lstatSync(path).isFile(), true, `required packaged-smoke input is not a regular file: ${relativePath}`);
    }
    const result = spawnSync(process.execPath, [join(distDir, "rag-required-smoke.mjs")], {
        cwd: distDir,
        encoding: "utf8",
        timeout: 120_000,
        windowsHide: true,
    });
    assert.equal(result.error, undefined, "required packaged-smoke child did not exit cleanly");
    assert.equal(result.signal, null, "required packaged-smoke child was terminated");
    assert.equal(result.status, 0, result.stderr);
    const summary = JSON.parse(result.stdout);
    assert.deepEqual(summary, {
        status: "passed",
        failOpen: false,
        capability: "rag_retrieve",
        hits: summary.hits,
        sensitiveLogScan: "passed",
    });
    assert.ok(Number.isSafeInteger(summary.hits) && summary.hits > 0);
    process.stdout.write(`${JSON.stringify(summary)}\n`);
} catch (error) {
    process.stderr.write(`${JSON.stringify({ error: { code: "required_rag_smoke_failed", message: error.message } })}\n`);
    process.exitCode = 1;
}

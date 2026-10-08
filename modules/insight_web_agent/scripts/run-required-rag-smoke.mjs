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
import { expectedNativeFiles } from "../server/services/rag/nativeRuntimeManifest.mjs";
import { resolveRagTarget } from "../server/services/rag/platformSupport.mjs";
import { formatSmokeError, redactSmokeText, sanitizeSmokeDiagnostic, smokeEnvironment } from "../server/services/rag/smokeDiagnostics.mjs";
import { hasNativeLoadFailure, runNativeFailureDiagnostics } from "./rag-native-diagnostics.mjs";

const packageRoot = fileURLToPath(new URL("../", import.meta.url));
const distDir = resolve(process.env.MSINSIGHT_DIST_SERVER_DIR ?? join(packageRoot, "dist-server"));
const sensitiveValues = [join(distDir, "rag-data"), join(distDir, "rag-runtime")];
let stage = "target_resolution";
const diagnostic = (value) => process.stderr.write(`${JSON.stringify(value)}\n`);

try {
    const target = resolveRagTarget();
    const nativeFiles = expectedNativeFiles(target);
    diagnostic(smokeEnvironment(distDir, nativeFiles));
    stage = "input_validation";
    const required = [
        "rag-required-smoke.mjs",
        "rag-build-mode.json",
        "rag-data/active.json",
        "rag-runtime/native-runtime-manifest.json",
        "rag-runtime/models/bge-small-zh-v1.5/model-manifest.json",
        "rag-runtime/models/bge-small-zh-v1.5/onnx/model.onnx",
        ...nativeFiles,
    ];
    assert.equal(isAbsolute(distDir), true);
    for (const relativePath of required) {
        const path = join(distDir, relativePath);
        assert.equal(existsSync(path), true, `required packaged-smoke input is missing: ${relativePath}`);
        assert.equal(lstatSync(path).isFile(), true, `required packaged-smoke input is not a regular file: ${relativePath}`);
    }
    stage = "child_execution";
    const result = spawnSync(process.execPath, [join(distDir, "rag-required-smoke.mjs")], {
        cwd: distDir,
        encoding: "utf8",
        timeout: 120_000,
        windowsHide: true,
    });
    if (result.stderr) {
        const lines = result.stderr.trimEnd().split(/\r?\n/);
        const selected = lines.length <= 64 ? lines : [...lines.slice(0, 32), ...lines.slice(-32)];
        for (const line of selected) {
            try { diagnostic(sanitizeSmokeDiagnostic(JSON.parse(line), sensitiveValues)); }
            catch { process.stderr.write(`${redactSmokeText(line, sensitiveValues, 4096)}\n`); }
        }
        if (lines.length > 64) diagnostic({ event: "rag_smoke_stderr_truncated", omittedLines: lines.length - 64 });
    }
    diagnostic({
        event: "rag_smoke_child_exit",
        status: result.status,
        signal: result.signal,
        ...(result.error ? { error: formatSmokeError(result.error, sensitiveValues) } : {}),
    });
    if (result.status !== 0 && hasNativeLoadFailure(result.stderr)) {
        try {
            const primaryFailure = result.stderr.split(/\r?\n/).map((line) => {
                try { return JSON.parse(line); } catch { return undefined; }
            }).find((value) => value?.event === "rag_smoke_failure");
            runNativeFailureDiagnostics({ packageRoot, distDir, emit: diagnostic, primaryFailure });
        } catch (error) {
            diagnostic({ event: "rag_native_probes_failure", error: formatSmokeError(error, sensitiveValues) });
        }
    }
    assert.equal(result.error, undefined, "required packaged-smoke child did not exit cleanly");
    assert.equal(result.signal, null, "required packaged-smoke child was terminated");
    assert.equal(result.status, 0, "required packaged-smoke child failed; see preceding diagnostics");
    stage = "summary_validation";
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
    diagnostic({
        event: "rag_smoke_wrapper_failure",
        stage,
        error: { ...formatSmokeError(error, sensitiveValues), code: "required_rag_smoke_failed" },
    });
    process.exitCode = 1;
}

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));
const distDir = process.env.MSINSIGHT_DIST_SERVER_DIR
    ? resolve(process.env.MSINSIGHT_DIST_SERVER_DIR)
    : join(packageRoot, "dist-server");

test("code-only bundle contains no seed or first-start provisioning path", () => {
    assert.equal(existsSync(join(distDir, "rag-seed")), false);
    const server = readFileSync(join(distDir, "index.mjs"), "utf8");
    assert.doesNotMatch(server, /provisionSeed|rag-seed|knowledge-pack\.zip/);
});

test("required packaged smoke fails instead of skipping when inputs are missing", () => {
    const result = spawnSync(process.execPath, [join(packageRoot, "scripts", "run-required-rag-smoke.mjs")], {
        cwd: packageRoot,
        env: { ...process.env, MSINSIGHT_DIST_SERVER_DIR: join(packageRoot, "missing-required-smoke-bundle") },
        encoding: "utf8",
    });

    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /required_rag_smoke_failed/);
    assert.doesNotMatch(result.stderr, /skip/i);
});

test("bundled CLI ignores root environment and CWD redirection inputs", () => {
    const redirected = join(distDir, "must-not-be-rag-data");
    const result = spawnSync(process.execPath, [join(distDir, "rag-cli.mjs"), "status"], {
        cwd: packageRoot,
        env: {
            ...process.env,
            ACP_ROOT: redirected,
            ACP_RESOURCE_ROOT: redirected,
            ACP_CWD: redirected,
        },
        encoding: "utf8",
    });

    assert.equal(result.status, 0, result.stderr);
    const status = JSON.parse(result.stdout);
    if (existsSync(join(distDir, "rag-data", "active.json"))) {
        const metadata = JSON.parse(readFileSync(join(distDir, "rag-build-mode.json"), "utf8"));
        assert.equal(status.active.kbVersion, metadata.package.kbVersion);
        assert.equal(status.installMode, metadata.mode === "product-bundled" ? "product-bundled" : "development-local");
    } else {
        assert.deepEqual(status, { active: null, previous: null });
    }
    assert.equal(existsSync(redirected), false);
});

test("bundled RAG is preactivated and passes lifecycle verify", {
    skip: !existsSync(join(distDir, "rag-data", "active.json")),
}, () => {
    const result = spawnSync(process.execPath, [join(distDir, "rag-cli.mjs"), "verify"], {
        cwd: packageRoot,
        encoding: "utf8",
    });

    assert.equal(result.status, 0, result.stderr);
    const verified = JSON.parse(result.stdout);
    const metadata = JSON.parse(readFileSync(join(distDir, "rag-build-mode.json"), "utf8"));
    assert.equal(verified.status, "verified");
    assert.equal(verified.kbVersion, metadata.package.kbVersion);
    assert.ok(verified.chunks > 0);
    assert.equal(verified.installMode, metadata.mode === "product-bundled" ? "product-bundled" : "development-local");
    assert.equal(existsSync(join(distDir, "rag-data", verified.kbVersion, "bm25-domain-dict.txt")), true);
    assert.equal(existsSync(join(distDir, "rag-seed")), false);
    assert.equal(["development", "product-bundled"].includes(metadata.mode), true);
    assert.equal(metadata.releaseEligible, metadata.mode === "product-bundled");
    if (metadata.mode === "development") {
        // Pinned to the current development build version; bump together with
        // the build version for each new dev/beta installer (dev.1 carried KB
        // 26.1.3, 26.2.0-beta-v1 carried KB 26.1.5, beta-v2 adds pre-seeded
        // skills, beta-v3 was superseded before install, beta-v4 adds pip
        // cluster-analysis dependencies, beta-v5 was superseded (missing
        // msprof_analyze), beta-v6 completed all 15 pip packages, and beta-v7
        // carries RAG 26.1.6 without ascend-npu-snapshot-analyzer).
        assert.equal(metadata.productVersion, "26.2.0-beta-v7");
        assert.equal(metadata.peNumericVersion, "26.2.0.7");
    }
    assert.equal(metadata.consumerAcceptanceEvaluated, false);
    assert.equal(metadata.promotionEvaluated, false);
    assert.equal(["clean", "dirty"].includes(metadata.softwareSource.treeState), true);
});

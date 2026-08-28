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
        assert.equal(status.installMode, "development-local");
    } else {
        assert.deepEqual(status, { active: null, previous: null });
    }
    assert.equal(existsSync(redirected), false);
});

test("development bundle is preactivated and passes bundled lifecycle verify", {
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
    assert.equal(verified.installMode, "development-local");
    assert.equal(existsSync(join(distDir, "rag-data", verified.kbVersion, "bm25-domain-dict.txt")), true);
    assert.equal(existsSync(join(distDir, "rag-seed")), false);
    assert.equal(metadata.productVersion, "26.1.1-rag-dev.1");
    assert.equal(metadata.peNumericVersion, "26.1.1.1");
    assert.equal(metadata.releaseEligible, false);
    assert.equal(metadata.consumerAcceptanceEvaluated, false);
    assert.equal(metadata.promotionEvaluated, false);
    assert.equal(metadata.softwareSource.treeState, "dirty");
});

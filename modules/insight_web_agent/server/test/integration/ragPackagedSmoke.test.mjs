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
        // Optional stale-artifact guard: when the build pipeline wants to pin
        // the expected dev installer version (protect against verifying a
        // stale bundle), set MSI_EXPECTED_PRODUCT_VERSION, e.g.
        //   MSI_EXPECTED_PRODUCT_VERSION=26.2.0-beta-v11
        // The numeric cross-check mirrors build/build.py extract_numeric_part
        // on master: keep the leading dotted-numeric segments of the version
        // and compare them against peNumericVersion. The remaining segments
        // (beta-vN, rag-dev.N, B100_002, ...) follow whatever naming the build
        // uses, so the test never pins a specific naming scheme. Without the
        // variable the bundle integrity checks above run for any version.
        const expectedVersion = process.env.MSI_EXPECTED_PRODUCT_VERSION;
        if (expectedVersion) {
            assert.equal(metadata.productVersion, expectedVersion);
            const expected = [];
            for (const part of expectedVersion.split('.')) {
                if (/^\d+$/.test(part)) {
                    expected.push(Number(part));
                } else {
                    break;
                }
            }
            const actual = metadata.peNumericVersion.split('.');
            for (let i = 0; i < expected.length; i++) {
                assert.equal(Number(actual[i]), expected[i], `peNumericVersion segment ${i + 1}`);
            }
        }
    }
    assert.equal(metadata.consumerAcceptanceEvaluated, false);
    assert.equal(metadata.promotionEvaluated, false);
    assert.equal(["clean", "dirty"].includes(metadata.softwareSource.treeState), true);
});

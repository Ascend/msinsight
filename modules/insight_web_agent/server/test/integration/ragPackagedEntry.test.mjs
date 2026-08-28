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

test("packaged RAG CLI loads as ESM and exposes lifecycle commands", () => {
    const result = spawnSync(process.execPath, [join(distDir, "rag-cli.mjs"), "--help"], {
        cwd: distDir,
        encoding: "utf8",
    });

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /mindstudio-insight-rag import/);
    assert.match(result.stdout, /mindstudio-insight-rag verify/);
    assert.doesNotMatch(result.stderr, /Dynamic require/);
});

test("packaged bundle contains the mandatory fail-closed ONNX and capability smoke entry", () => {
    const smokeEntry = join(distDir, "rag-required-smoke.mjs");
    const source = readFileSync(smokeEntry, "utf8");

    assert.match(source, /failOpen:\s*!1|failOpen:\s*false/);
    assert.match(source, /rag_retrieve/);
});

test("packaged server preserves capability resources and contains RAG runtime contracts", () => {
    for (const path of [
        "index.mjs",
        "rag-cli.mjs",
        "capability-center.json",
        "docs",
        "rag-runtime/runtime-contract.json",
    ]) {
        assert.equal(existsSync(join(distDir, path)), true, path);
    }
    for (const name of ["bm25-domain-dict.txt", "bm25-tokenizer-fixture.json", "rerank-context-v1.json", "semantic-input-v1.json"]) {
        assert.equal(existsSync(join(distDir, "rag-runtime", name)), false, name);
    }
    assert.equal(existsSync(join(distDir, "rag-runtime/contracts/contract-index.json")), false);
    assert.equal(existsSync(join(distDir, "rag-runtime/contracts/msinsight-default-consumer-contract.json")), false);
});

test("Windows x64 package contains the native RAG runtime bindings", {
    skip: process.platform !== "win32" || process.arch !== "x64",
}, () => {
    for (const path of [
        "node_modules/onnxruntime-node/bin/napi-v6/win32/x64/onnxruntime_binding.node",
        "node_modules/onnxruntime-node/bin/napi-v6/win32/x64/onnxruntime.dll",
        "node_modules/@node-rs/jieba-win32-x64-msvc/jieba.win32-x64-msvc.node",
    ]) {
        assert.equal(existsSync(join(distDir, path)), true, path);
    }
});

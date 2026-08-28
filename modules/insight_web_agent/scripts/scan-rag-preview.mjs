/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, lstatSync, readdirSync, readFileSync } from "node:fs";
import { isAbsolute, join, relative, resolve } from "node:path";

const args = process.argv.slice(2);
const rootIndex = args.indexOf("--root");

try {
    assert.equal(args.length, 2, "preview scan requires exactly --root <path>");
    assert.notEqual(rootIndex, -1, "preview scan requires --root");
    const requested = resolve(args[rootIndex + 1]);
    assert.equal(isAbsolute(requested), true);
    const bundle = existsSync(join(requested, "rag-build-mode.json"))
        ? requested
        : join(requested, "resources", "profiler", "server", "insight_web_agent");
    assert.equal(lstatSync(bundle).isDirectory(), true, "preview RAG bundle root is missing");

    const required = [
        "index.mjs",
        "rag-cli.mjs",
        "rag-required-smoke.mjs",
        "rag-build-mode.json",
        "rag-data/active.json",
        "rag-runtime/runtime-contract.json",
        "rag-runtime/models/bge-small-zh-v1.5/model-manifest.json",
    ];
    for (const path of required) assert.equal(lstatSync(join(bundle, path)).isFile(), true, `missing preview file: ${path}`);

    const entries = walk(bundle);
    for (const path of entries) {
        assert.doesNotMatch(path, /^(rag-seed|docs)(\/|$)/i);
        assert.doesNotMatch(path, /^rag-data\/(previous|backup)(\/|$)/i);
        assert.doesNotMatch(path, /(^|\/)knowledge-pack\.zip(?:\.sha256)?$/i);
        if (/knowledge-pack-v4\.zip(?:\.sha256)?$/i.test(path)) {
            assert.match(path, /^rag-data\/\d{2}\.[012]\.[1-9]\d*\/knowledge-pack-v4\.zip(?:\.sha256)?$/);
        }
    }
    const metadata = JSON.parse(readFileSync(join(bundle, "rag-build-mode.json"), "utf8"));
    assert.equal(metadata.mode, "development");
    assert.equal(metadata.releaseEligible, false);
    assert.equal(metadata.consumerAcceptanceEvaluated, false);
    assert.equal(metadata.promotionEvaluated, false);

    const verify = spawnSync(process.execPath, [join(bundle, "rag-cli.mjs"), "verify"], {
        cwd: bundle,
        encoding: "utf8",
        windowsHide: true,
    });
    assert.equal(verify.status, 0, verify.stderr);
    process.stdout.write(`${JSON.stringify({ status: "passed", files: entries.length, mode: metadata.mode })}\n`);
} catch (error) {
    process.stderr.write(`${JSON.stringify({ error: { code: "rag_preview_scan_failed", message: error.message } })}\n`);
    process.exitCode = 1;
}

function walk(root) {
    const result = [];
    const visit = (directory) => {
        for (const entry of readdirSync(directory, { withFileTypes: true })) {
            const path = join(directory, entry.name);
            const normalized = relative(root, path).replaceAll("\\", "/");
            const info = lstatSync(path);
            assert.equal(info.isSymbolicLink(), false, `preview contains a symlink: ${normalized}`);
            result.push(normalized);
            if (entry.isDirectory()) visit(path);
            else assert.equal(entry.isFile(), true, `preview contains a non-file entry: ${normalized}`);
        }
    };
    visit(root);
    return result.sort();
}

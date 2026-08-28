/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { lstatSync, readdirSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(fileURLToPath(new URL("../package.json", import.meta.url)));
const TESTS = "server/test/services/rag/*.test.mjs";

export const COVERAGE_GROUPS = Object.freeze({
    "parser/contracts": Object.freeze([
        "server/services/rag/wire/strictJsonParser.mjs",
        "server/services/rag/wire/canonicalJson.mjs",
        "server/services/rag/wire/packageContracts.mjs",
        "server/services/rag/runtimeContract.mjs",
    ]),
    "loader/receiver": Object.freeze([
        "server/services/rag/knowledgePackageService.mjs",
        "server/services/rag/knowledgePackLoader.mjs",
    ]),
    lifecycle: Object.freeze([
        "server/services/rag/knowledgePackageService.mjs",
        "server/rag-cli.mjs",
    ]),
});

export const validateCoverageMembership = (packageRoot = root, groups = COVERAGE_GROUPS) => {
    const classified = new Set(Object.values(groups).flat());
    for (const [name, files] of Object.entries(groups)) {
        assert.ok(files.length > 0, `coverage group is empty: ${name}`);
        assert.equal(new Set(files).size, files.length, `coverage group contains duplicates: ${name}`);
        for (const file of files) {
            const info = lstatSync(join(packageRoot, file));
            assert.equal(info.isFile(), true, `coverage member is not a file: ${file}`);
        }
    }
    const unclassified = discoverCriticalFiles(packageRoot).filter((file) => !classified.has(file));
    assert.deepEqual(unclassified, [], `unclassified critical RAG files: ${unclassified.join(", ")}`);
    return groups;
};

export const runCoverage = (packageRoot = root) => {
    const groups = validateCoverageMembership(packageRoot);
    const summaries = [];
    for (const [name, files] of Object.entries(groups)) {
        process.stdout.write(`coverage group ${name} membership: ${JSON.stringify(files)}\n`);
        const args = [
            "--experimental-test-coverage",
            "--test-coverage-branches=90",
            ...files.map((file) => `--test-coverage-include=${file}`),
            "--test",
            "--test-concurrency=1",
            TESTS,
        ];
        const result = spawnSync(process.execPath, args, {
            cwd: packageRoot,
            encoding: "utf8",
            windowsHide: true,
        });
        if (result.status !== 0) {
            process.stderr.write(result.stdout);
            process.stderr.write(result.stderr);
            throw new Error(`coverage group failed: ${name}`);
        }
        const branch = parseAggregateBranch(result.stdout);
        for (const file of files) assertFileHasCoverage(result.stdout, file);
        summaries.push({ name, branch, files });
        process.stdout.write(`coverage group ${name} branch: ${branch.toFixed(2)}%\n`);
    }
    return summaries;
};

const discoverCriticalFiles = (packageRoot) => {
    const wireRoot = join(packageRoot, "server", "services", "rag", "wire");
    const discovered = readdirSync(wireRoot, { withFileTypes: true })
        .filter((entry) => entry.isFile() && entry.name.endsWith(".mjs"))
        .map((entry) => relative(packageRoot, join(wireRoot, entry.name)).replaceAll("\\", "/"));
    const ragRoot = join(packageRoot, "server", "services", "rag");
    for (const entry of readdirSync(ragRoot, { withFileTypes: true })) {
        if (entry.isFile() && /(Contract|PackageService|PackLoader)\.mjs$/.test(entry.name)) {
            discovered.push(relative(packageRoot, join(ragRoot, entry.name)).replaceAll("\\", "/"));
        }
    }
    discovered.push("server/rag-cli.mjs");
    return [...new Set(discovered)].sort();
};

const parseAggregateBranch = (output) => {
    const matches = [...output.matchAll(/all files\s+\|\s+[\d.]+\s+\|\s+([\d.]+)/g)];
    assert.ok(matches.length > 0, "coverage output has no aggregate branch result");
    const branch = Number(matches.at(-1)[1]);
    assert.ok(Number.isFinite(branch) && branch >= 90, `branch coverage is below 90%: ${branch}`);
    return branch;
};

const assertFileHasCoverage = (output, file) => {
    const escaped = basename(file).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const match = new RegExp(`(?:^|\\n).*${escaped}\\s+\\|\\s+([\\d.]+)\\s+\\|\\s+([\\d.]+)`, "m").exec(output);
    assert.ok(match, `coverage output is missing member: ${file}`);
    assert.ok(Number(match[1]) > 0 || Number(match[2]) > 0, `coverage member has zero coverage: ${file}`);
};

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    try {
        const summaries = runCoverage();
        process.stdout.write(`${JSON.stringify({ status: "passed", groups: summaries })}\n`);
    } catch (error) {
        process.stderr.write(`${JSON.stringify({ error: { code: "rag_coverage_failed", message: error.message } })}\n`);
        process.exitCode = 1;
    }
}

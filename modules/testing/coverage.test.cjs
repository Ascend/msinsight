/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const assert = require('node:assert/strict');
const path = require('node:path');
const { test } = require('node:test');
const { summarizeModules } = require('./coverage.cjs');

function entry(covered, total) {
    return Object.fromEntries(['statements', 'branches', 'functions', 'lines'].map(metric => [metric, { covered, total }]));
}

test('weights coverage by counts, preserves zero-covered files and assigns shared code to its owner', () => {
    const root = path.resolve('fixture');
    const modules = ['feature', 'lib'].map(name => ({ name, rootDir: path.join(root, name) }));
    const result = summarizeModules({
        total: entry(100, 100),
        [path.join(root, 'feature/src/a.ts')]: entry(1, 1),
        [path.join(root, 'feature/src/b.ts')]: entry(0, 9),
        [path.join(root, 'lib/src/shared.ts')]: entry(2, 2),
    }, modules);
    assert.deepEqual(result.feature.lines, { covered: 1, total: 10, pct: 10 });
    assert.equal(result.feature.files, 2);
    assert.deepEqual(result.lib.lines, { covered: 2, total: 2, pct: 100 });
});

test('missing module reports fail rather than silently raising the aggregate percentage', () => {
    assert.throws(() => summarizeModules({}, [{ name: 'feature', rootDir: path.resolve('feature') }]), /Missing coverage/);
});

test('a transform failure cannot silently remove a source file from the coverage denominator', () => {
    assert.throws(() => summarizeModules({}, [], ['/repo/worker.ts']), /Missing source coverage/);
});

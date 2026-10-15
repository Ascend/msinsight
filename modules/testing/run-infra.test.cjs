/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

function fixture(t) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'insight infra '));
    t.after(() => fs.rmSync(root, { recursive: true, force: true }));
    const directory = path.join(root, 'testing');
    fs.mkdirSync(directory);
    const runner = path.join(directory, 'run-infra.cjs');
    fs.copyFileSync(path.join(__dirname, 'run-infra.cjs'), runner);
    // Start a standalone CLI process rather than inheriting node:test's worker context.
    const env = { ...process.env };
    delete env.NODE_TEST_CONTEXT;
    return {
        directory,
        run: () => spawnSync(process.execPath, [runner], { cwd: os.tmpdir(), env, encoding: 'utf8', shell: false }),
    };
}

test('infra runner discovers files without a shell, including paths with spaces', t => {
    const { directory, run } = fixture(t);
    fs.writeFileSync(path.join(directory, 'first.test.cjs'), "require('node:test')('first explicit file', () => {});");
    fs.writeFileSync(path.join(directory, 'second.test.cjs'), "require('node:test')('second explicit file', () => {});");
    fs.writeFileSync(path.join(directory, 'helper.cjs'), "throw new Error('Not a test');");
    fs.mkdirSync(path.join(directory, 'ignored.test.cjs'));
    const result = run();
    assert.equal(result.status, 0, result.stderr + result.stdout);
    assert.match(result.stdout, /first explicit file/);
    assert.match(result.stdout, /second explicit file/);
});

test('infra runner propagates test failures', t => {
    const { directory, run } = fixture(t);
    fs.writeFileSync(path.join(directory, 'failure.test.cjs'),
        "require('node:test')('failure', () => { require('node:assert/strict').fail('expected failure'); });");
    assert.equal(run().status, 1);
});

test('infra runner rejects an empty test directory', t => {
    const result = fixture(t).run();
    assert.equal(result.status, 1);
    assert.match(result.stderr, /No infrastructure tests found/);
});

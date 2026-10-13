/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const syncScript = fileURLToPath(new URL('./sync.mjs', import.meta.url));
const modules = [
    ['reinforcement-learning', 'RL'],
    ['cluster', 'Cluster'],
    ['compute', 'Compute'],
    ['insight_web_agent', 'InsightWebAgent'],
    ['leaks', 'Leaks'],
    ['memory', 'Memory'],
    ['operator', 'Operator'],
    ['statistic', 'Statistic'],
    ['timeline', 'Timeline'],
    ['numa', 'NUMA'],
];
const fixtureBase = process.env.MSINSIGHT_SYNC_TEST_FIXTURE_ROOT
    ? path.resolve(process.env.MSINSIGHT_SYNC_TEST_FIXTURE_ROOT)
    : await fs.realpath(tmpdir());
const ownedFixtures = new Set();

async function assertOwnedFixture(fixture) {
    assert.ok(ownedFixtures.has(fixture), 'Only this test run owns the cleanup target');
    assert.equal(path.dirname(fixture), fixtureBase);
    assert.ok(path.basename(fixture).startsWith('msinsight-sync-'));
    assert.equal(await fs.realpath(fixtureBase), fixtureBase, 'Fixture parent must not escape through a symlink');
    assert.equal(await fs.realpath(fixture), fixture, 'Fixture target must not escape through a symlink');
    const info = await fs.lstat(fixture);
    assert.ok(info.isDirectory() && !info.isSymbolicLink());
}

async function makeFixture(t, { missing = [], invalid = [] } = {}) {
    assert.ok(path.isAbsolute(fixtureBase));
    assert.equal(await fs.realpath(fixtureBase), fixtureBase);
    const fixture = await fs.mkdtemp(path.join(fixtureBase, 'msinsight-sync-'));
    ownedFixtures.add(fixture);
    await assertOwnedFixture(fixture);
    t.after(async () => {
        // Resolve and verify the exact fresh, owned target before any cleanup.
        await assertOwnedFixture(fixture);
        await fs.rm(fixture, { recursive: true, force: true });
        ownedFixtures.delete(fixture);
    });
    for (const [folder] of modules) {
        if (missing.includes(folder)) {
            continue;
        }
        const build = path.join(fixture, folder, 'build');
        if (invalid.includes(folder)) {
            await fs.mkdir(path.dirname(build), { recursive: true });
            await fs.writeFile(build, 'Not a build directory');
        } else {
            await fs.mkdir(build, { recursive: true });
            await fs.writeFile(path.join(build, 'index.html'), `fixture:${folder}`);
        }
    }
    return fixture;
}

async function runSync(fixture) {
    await assertOwnedFixture(fixture);
    // The real script's removal/copy destinations are all inside this fresh fixture.
    for (const [, targetName] of modules) {
        const target = path.resolve(fixture, 'framework', 'public', 'plugins', targetName);
        const relative = path.relative(fixture, target);
        assert.ok(relative && !relative.startsWith('..') && !path.isAbsolute(relative));
        try {
            assert.equal(await fs.realpath(target), target, 'Sync destination must remain inside the owned fixture');
        } catch (error) {
            if (error.code !== 'ENOENT') {
                throw error;
            }
        }
    }
    const result = spawnSync(process.execPath, [syncScript], {
        cwd: fixture,
        env: { PATH: process.env.PATH ?? '' },
        encoding: 'utf8',
        timeout: 20000,
        maxBuffer: 1024 * 1024,
    });
    assert.equal(result.error, undefined, 'Sync child must actually run to a terminal result');
    assert.equal(result.signal, null, 'Sync child must not be cancelled or time out');
    return result;
}

async function readPlugin(fixture, targetName) {
    return fs.readFile(path.join(fixture, 'framework', 'public', 'plugins', targetName, 'index.html'), 'utf8');
}

test('complete builds synchronize NUMA and Agent exactly once with correct contents', async (t) => {
    const fixture = await makeFixture(t);
    const result = await runSync(fixture);
    assert.equal(result.status, 0, result.stderr);
    for (const [folder, targetName] of [['numa', 'NUMA'], ['insight_web_agent', 'InsightWebAgent']]) {
        const lines = result.stdout.split('\n').filter((line) => line.includes(`已同步: ${folder} -> ${targetName}`));
        assert.equal(lines.length, 1, `${folder} must synchronize exactly once`);
        assert.equal(await readPlugin(fixture, targetName), `fixture:${folder}`);
    }
});

test('missing required NUMA build reports failure rather than successful incomplete sync', async (t) => {
    const fixture = await makeFixture(t, { missing: ['numa'] });
    const result = await runSync(fixture);
    assert.equal(result.status, 1, result.stderr);
    assert.match(result.stderr, /numa/);
    assert.equal(await readPlugin(fixture, 'InsightWebAgent'), 'fixture:insight_web_agent');
});

test('missing required Agent build reports failure while preserving NUMA synchronization', async (t) => {
    const fixture = await makeFixture(t, { missing: ['insight_web_agent'] });
    const result = await runSync(fixture);
    assert.equal(result.status, 1, result.stderr);
    assert.match(result.stderr, /insight_web_agent/);
    assert.equal(await readPlugin(fixture, 'NUMA'), 'fixture:numa');
});

test('Agent copy errors produce a nonzero terminal result without losing other synchronization', async (t) => {
    const fixture = await makeFixture(t, { invalid: ['insight_web_agent'] });
    const result = await runSync(fixture);
    assert.equal(result.status, 1, result.stderr);
    assert.match(result.stderr, /同步 insight_web_agent 失败/);
    assert.equal(await readPlugin(fixture, 'NUMA'), 'fixture:numa');
});

test('missing optional builds retain the existing warning-only behavior', async (t) => {
    const fixture = await makeFixture(t, { missing: ['compute'] });
    const result = await runSync(fixture);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stderr, /跳过: compute/);
    assert.equal(await readPlugin(fixture, 'NUMA'), 'fixture:numa');
    assert.equal(await readPlugin(fixture, 'InsightWebAgent'), 'fixture:insight_web_agent');
});

test('optional module copy errors also produce a nonzero terminal result', async (t) => {
    const fixture = await makeFixture(t, { invalid: ['compute'] });
    const result = await runSync(fixture);
    assert.equal(result.status, 1, result.stderr);
    assert.match(result.stderr, /同步 compute 失败/);
    assert.equal(await readPlugin(fixture, 'InsightWebAgent'), 'fixture:insight_web_agent');
});

test('required destination replacement removes stale files only inside the fresh fixture', async (t) => {
    const fixture = await makeFixture(t);
    for (const targetName of ['NUMA', 'InsightWebAgent']) {
        const plugin = path.join(fixture, 'framework', 'public', 'plugins', targetName);
        await fs.mkdir(plugin, { recursive: true });
        await fs.writeFile(path.join(plugin, 'stale.txt'), 'old fixture output');
    }
    const result = await runSync(fixture);
    assert.equal(result.status, 0, result.stderr);
    for (const [folder, targetName] of [['numa', 'NUMA'], ['insight_web_agent', 'InsightWebAgent']]) {
        assert.equal(await readPlugin(fixture, targetName), `fixture:${folder}`);
        await assert.rejects(fs.stat(path.join(fixture, 'framework', 'public', 'plugins', targetName, 'stale.txt')), {
            code: 'ENOENT',
        });
    }
});

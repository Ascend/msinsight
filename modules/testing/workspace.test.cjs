/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test } = require('node:test');
const glob = require('fast-glob');
const { discoverWorkspace } = require('./workspace.cjs');

function projectTestFiles(project) {
    // fast-glob treats backslashes as escapes, not Windows path separators.
    const rootDir = project.rootDir.replaceAll('\\', '/');
    const patterns = project.testMatch.map(pattern => pattern.replace('<rootDir>', rootDir));
    const ignored = project.testPathIgnorePatterns.map(pattern => new RegExp(pattern));
    return glob.sync(patterns).filter(file => !ignored.some(pattern => pattern.test(file)));
}

function fixture(t, packages) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'insight-test-discovery-'));
    t.after(() => fs.rmSync(root, { recursive: true, force: true }));
    fs.writeFileSync(path.join(root, 'pnpm-workspace.yaml'), `packages:\n${packages.map(name => `  - '${name}'`).join('\n')}\n`);
    return root;
}

function addTest(root, module, name = 'example.test.ts') {
    const directory = path.join(root, module, 'src');
    fs.mkdirSync(directory, { recursive: true });
    fs.writeFileSync(path.join(directory, name), '');
}

test('discovers workspace packages and excludes unrelated local directories', t => {
    const root = fixture(t, ['feature-*']);
    addTest(root, 'feature-a');
    addTest(root, 'feature-b', 'example.unit.test.ts');
    addTest(root, 'untracked-app');
    assert.deepEqual(discoverWorkspace(root, {}).map(module => module.name), ['feature-a', 'feature-b']);
});

test('fails if an existing test module loses all tests', t => {
    const root = fixture(t, ['feature']);
    fs.mkdirSync(path.join(root, 'feature'));
    assert.throws(() => discoverWorkspace(root, {}), /No tests found in feature/);
});

test('explicit no-test exceptions remain visible and start discovering newly added tests', t => {
    const root = fixture(t, ['feature']);
    fs.mkdirSync(path.join(root, 'feature'));
    const exceptions = { feature: 'Migration backlog' };
    assert.equal(discoverWorkspace(root, exceptions)[0].tests.length, 0);
    addTest(root, 'feature');
    assert.equal(discoverWorkspace(root, exceptions)[0].tests.length, 1);
});

test('every repository test belongs to exactly one Jest project', () => {
    const config = require('./jest.config.cjs');
    const discovered = new Map(discoverWorkspace().flatMap(module =>
        module.tests.map(file => [path.join(module.rootDir, file).replaceAll('\\', '/'), 0])));
    for (const project of config.projects) {
        for (const file of projectTestFiles(project)) {
            assert.ok(discovered.has(file), `Unexpected test: ${file}`);
            discovered.set(file, discovered.get(file) + 1);
        }
    }
    for (const [file, count] of discovered) assert.equal(count, 1, file);
});

test('project discovery handles Windows separators and preserves test exclusions', t => {
    const root = fixture(t, ['feature']);
    addTest(root, 'feature');
    addTest(root, 'feature', 'logic.unit.test.ts');
    const project = {
        rootDir: root.replaceAll('/', '\\'),
        testMatch: ['<rootDir>/feature/src/**/*.test.ts'],
        testPathIgnorePatterns: ['\\.unit\\.test\\.'],
    };
    const expected = [path.join(root, 'feature/src/example.test.ts').replaceAll('\\', '/')];
    assert.deepEqual(projectTestFiles(project), expected);
    assert.deepEqual(projectTestFiles({ ...project, rootDir: root.replaceAll('\\', '/') }), expected);
});

test('the ESM allowlist handles pnpm paths without transforming CommonJS packages', () => {
    const project = require('./jest.config.cjs').projects[0];
    const ignored = new RegExp(project.transformIgnorePatterns[0]);
    for (const prefix of ['/repo/node_modules/', '/repo/node_modules/.pnpm/dependency@1/node_modules/']) {
        assert.equal(ignored.test(`${prefix}d3-array/src/index.js`), false);
        assert.equal(ignored.test(`${prefix}antd/es/locale/zh_CN.js`), false);
        assert.equal(ignored.test(`${prefix}@babel/runtime/helpers/esm/extends.js`), false);
        assert.equal(ignored.test(`${prefix}react/index.js`), true);
    }
});

test('untested package source remains visible to a tested project for coverage collection', () => {
    const modules = discoverWorkspace();
    const config = require('./jest.config.cjs');
    const testedNames = new Set(modules.filter(module => module.tests.length > 0).map(module => module.name));
    const roots = config.projects.filter(project => testedNames.has(project.displayName)).flatMap(project => project.roots);
    for (const module of modules.filter(module => module.tests.length === 0)) {
        assert.ok(roots.includes(path.join(module.rootDir, 'src')), module.name);
    }
});

test('Worker URLs can be instrumented in CommonJS without executing a Worker', () => {
    const { createRequire } = require('node:module');
    const { pathToFileURL } = require('node:url');
    const craRequire = createRequire(require.resolve('react-scripts/package.json'));
    const filename = path.resolve('worker.ts');
    const result = craRequire('@babel/core').transformSync('const url = new URL("./", import.meta.url);', {
        filename,
        configFile: false,
        babelrc: false,
        plugins: [require('./import-meta-url.cjs')],
    });
    assert.ok(!result.code.includes('import.meta'));
    assert.ok(result.code.includes(pathToFileURL(filename).href));
});

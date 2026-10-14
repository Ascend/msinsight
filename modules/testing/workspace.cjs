/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const fs = require('node:fs');
const path = require('node:path');
const glob = require('fast-glob');
const { parse } = require('yaml');

const modulesRoot = path.resolve(__dirname, '..');
const untested = {
    'memory-on-chip': 'No UT baseline yet.',
    operator: 'No UT baseline yet.',
    'reinforcement-learning': 'No UT baseline yet.',
    statistic: 'No UT baseline yet.',
};

function discoverWorkspace(root = modulesRoot, exceptions = untested) {
    const { packages } = parse(fs.readFileSync(path.join(root, 'pnpm-workspace.yaml'), 'utf8'));
    const directories = glob.sync(packages, { cwd: root, onlyDirectories: true, ignore: ['**/node_modules/**'] }).sort();
    const modules = directories.map(directory => {
        const rootDir = path.join(root, directory);
        const tests = glob.sync('src/**/*.{test,spec}.{ts,tsx,js,jsx}', { cwd: rootDir }).sort();
        return { name: directory, rootDir, tests };
    });
    if (modules.length === 0) throw new Error('No frontend workspace packages found.');
    for (const module of modules) {
        if (module.tests.length === 0 && !exceptions[module.name]) {
            throw new Error(`No tests found in ${module.name}. Add tests or document an explicit exception in testing/workspace.cjs.`);
        }
    }
    return modules;
}

module.exports = { discoverWorkspace, modulesRoot, untested };

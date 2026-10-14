/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const path = require('node:path');
const fs = require('node:fs');
const glob = require('fast-glob');
const { discoverWorkspace, modulesRoot, untested } = require('./workspace.cjs');
const { summarizeModules } = require('./coverage.cjs');

process.env.NODE_ENV = 'test';
process.env.BABEL_ENV = 'test';
process.env.REACT_APP_TEST = '1';

async function main() {
    const modules = discoverWorkspace();
    const args = process.argv.slice(2).filter(arg => arg !== '--');
    const coverage = args.includes('--coverage') || args.includes('--coverage=true');
    if (args.includes('--ci')) process.env.CI = 'true';
    const selected = [];
    for (let index = 0; index < args.length; index++) {
        if (args[index] !== '--module') continue;
        const name = args[index + 1];
        const module = modules.find(item => item.name === name);
        if (!module) throw new Error(`Unknown module: ${name}. Available: ${modules.map(item => item.name).join(', ')}`);
        selected.push(module);
        args.splice(index, 2);
        index--;
    }
    const targets = selected.length ? selected : modules;
    const empty = targets.filter(module => module.tests.length === 0);
    for (const module of empty) console.log(`[UT] ${module.name}: ${untested[module.name]}`);
    if (selected.length && targets.every(module => module.tests.length === 0)) {
        throw new Error('The selected modules have no tests; no validation was performed.');
    }
    if (selected.length) {
        args.push('--selectProjects', ...selected.flatMap(module => [
            module.name,
            ...(module.tests.some(file => file.includes('.unit.test.')) ? [`${module.name}:unit`] : []),
        ]));
    }
    if (coverage) {
        for (const file of ['coverage/module-summary.json', 'coverage/coverage-summary.json', 'test-results/run-summary.json']) {
            fs.rmSync(path.join(modulesRoot, file), { force: true });
        }
    }
    await require('jest').run(['--config', path.join(__dirname, 'jest.config.cjs'), ...args]);
    if (coverage) {
        const summary = JSON.parse(fs.readFileSync(path.join(modulesRoot, 'coverage/coverage-summary.json'), 'utf8'));
        const run = JSON.parse(fs.readFileSync(path.join(modulesRoot, 'test-results/run-summary.json'), 'utf8'));
        const expected = modules.flatMap(module => module.tests.map(file => path.join(module.rootDir, file)));
        const fullWorkspace = expected.length === run.testFiles.length && expected.every(file => run.testFiles.includes(file));
        const sourceFiles = glob.sync(require('./jest.config.cjs').collectCoverageFrom, { cwd: modulesRoot, absolute: true })
            .filter(file => targets.some(module => file.startsWith(`${module.rootDir.replaceAll('\\', '/')}/src/`)))
            .map(file => path.normalize(file));
        fs.writeFileSync(path.join(modulesRoot, 'coverage/module-summary.json'), JSON.stringify({
            fullWorkspace,
            testsPassed: run.failed === 0 && run.failedSuites === 0 && run.skipped === 0,
            total: summary.total,
            modules: summarizeModules(summary, targets, sourceFiles),
        }, null, 2));
    }
}

main().catch(error => {
    console.error(error.message);
    process.exitCode = 1;
});

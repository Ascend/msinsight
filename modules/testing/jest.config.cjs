/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const fs = require('node:fs');
const path = require('node:path');
const { discoverWorkspace, modulesRoot } = require('./workspace.cjs');

const libRoot = path.join(modulesRoot, 'lib');
const libExports = require('../lib/package.json').exports;
const escapeRegex = value => value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const libAliases = Object.fromEntries(Object.entries(libExports).map(([key, value]) => [
    `^${escapeRegex(`@insight/lib${key === '.' ? '' : key.slice(1)}`)}$`,
    path.join(libRoot, value),
]));

function project(module, environment) {
    const unit = environment === 'node';
    const setup = path.join(module.rootDir, 'src/setupTests.ts');
    return {
        displayName: unit ? `${module.name}:unit` : module.name,
        rootDir: modulesRoot,
        roots: [path.join(module.rootDir, 'src'), ...(!unit && module.name === coverageCollector
            ? modules.filter(item => item.tests.length === 0).map(item => path.join(item.rootDir, 'src')) : [])],
        testEnvironment: require.resolve(`jest-environment-${environment}`),
        testMatch: [unit ? `<rootDir>/${module.name}/src/**/*.unit.test.{ts,tsx,js,jsx}` : `<rootDir>/${module.name}/src/**/*.{test,spec}.{ts,tsx,js,jsx}`],
        testPathIgnorePatterns: unit ? [] : ['\\.unit\\.test\\.'],
        setupFilesAfterEnv: unit ? [] : [
            path.join(__dirname, 'setup-dom.ts'),
            ...(fs.existsSync(setup) ? [setup] : []),
        ],
        clearMocks: true,
        resetMocks: true,
        restoreMocks: true,
        // Keep the production JSX/asset transforms while decoupling test orchestration from CRA.
        transform: {
            '^.+\\.[cm]?[jt]sx?$': path.join(__dirname, 'babel-transform.cjs'),
            '^.+\\.css$': require.resolve('react-scripts/config/jest/cssTransform'),
            '^(?!.*\\.(js|jsx|mjs|cjs|ts|tsx|css|json)$)': require.resolve('react-scripts/config/jest/fileTransform'),
        },
        // pnpm's real paths contain a second node_modules directory: match the package nearest the file.
        transformIgnorePatterns: [
            '[/\\\\]node_modules[/\\\\](?!(?:\\.pnpm[/\\\\]|d3[^/\\\\]*[/\\\\]|internmap[/\\\\]|delaunator[/\\\\]|robust-predicates[/\\\\]|@babel[/\\\\]runtime[/\\\\]|antd[/\\\\]|@ant-design[/\\\\]|rc-[^/\\\\]*[/\\\\]))',
        ],
        moduleNameMapper: { '^@/(.*)$': `<rootDir>/${module.name}/src/$1`, ...libAliases },
    };
}

const modules = discoverWorkspace();
// Jest collects unexecuted source only from projects that ran tests.
// Give one tested project the source roots of explicitly untested packages.
const coverageCollector = modules.find(module => module.tests.length > 0).name;

module.exports = {
    rootDir: modulesRoot,
    maxWorkers: 2,
    projects: modules.flatMap(module => [
        project(module, 'jsdom'),
        ...(module.tests.some(file => file.includes('.unit.test.')) ? [project(module, 'node')] : []),
    ]),
    collectCoverageFrom: [
        ...modules.map(module => `${module.name}/src/**/*.{ts,tsx}`),
        '!**/*.d.ts',
        '!**/*.{test,spec}.{ts,tsx}',
        '!**/setupTests.ts',
        '!**/{__test__,__tests__,__mock__,__mocks__,__snapshots__,testUtils}/**',
        '!**/src/{i18n,style,styles}/**',
    ],
    coverageDirectory: path.join(modulesRoot, 'coverage'),
    coverageReporters: ['json', 'json-summary', 'lcov', 'text-summary'],
    reporters: ['default', path.join(__dirname, 'run-summary.cjs'), [require.resolve('jest-junit'), {
        outputDirectory: path.join(modulesRoot, 'test-results'),
        outputName: 'junit.xml',
        suiteNameTemplate: '{displayName}: {filepath}',
        reportTestSuiteErrors: true,
    }]],
};

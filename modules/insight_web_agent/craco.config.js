/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
const path = require('path');
const ModuleScopePlugin = require('react-dev-utils/ModuleScopePlugin');

// CRA's Jest runner needs to transpile the ESM Markdown pipeline, including pnpm's virtual store.
const markdownEsm = [
    '@ungap/structured-clone', 'react-markdown', 'unified', 'hastscript', 'markdown-table',
    '(?:remark|rehype|hast|mdast|micromark|unist|vfile)(?:-[^/\\\\]+)?',
    'character-entities(?:-[^/\\\\]+)?', 'character-reference-invalid', 'decode-named-character-reference',
    '(?:comma|space)-separated-tokens', 'property-information', 'html-url-attributes', 'estree-util-is-identifier-name',
    'is-(?:alphabetical|alphanumerical|decimal|hexadecimal|plain-obj)',
    '(?:parse|stringify)-entities', 'entities', 'parse5', 'web-namespaces', 'escape-string-regexp',
    'bail', 'ccount', 'devlop', 'longest-streak', 'trim-lines', 'trough', 'zwitch',
].join('|').replace('@ungap/structured-clone', String.raw`@ungap[/\\]structured-clone`);

module.exports = {
    devServer: {
        port: 3011,
        open: false,
    },
    webpack: {
        alias: {
            '@': path.resolve(__dirname, 'src'),
            '@insight/lib': path.resolve(__dirname, '../lib/src'),
        },
        configure: (webpackConfig) => {
            webpackConfig.resolve.plugins = webpackConfig.resolve.plugins.filter(
                (plugin) => !(plugin instanceof ModuleScopePlugin),
            );
            const oneOfRule = webpackConfig.module.rules.find((rule) => Array.isArray(rule.oneOf));
            const babelRule = oneOfRule?.oneOf.find((rule) => String(rule.loader ?? '').includes('babel-loader') && rule.options?.presets);
            if (babelRule) {
                babelRule.include = [path.resolve(__dirname, 'src'), path.resolve(__dirname, '../lib/src')];
            }
            return webpackConfig;
        },
    },
    jest: {
        configure: (jestConfig) => ({
            ...jestConfig,
            resolver: require.resolve('./scripts/jest-markdown-resolver.cjs'),
            transformIgnorePatterns: [
                String.raw`[/\\]node_modules[/\\](?!(?:\.pnpm|${markdownEsm})[/\\]).+\.(?:js|jsx|mjs|cjs|ts|tsx)$`,
                '^.+\\.module\\.(css|sass|scss)$',
            ],
            moduleNameMapper: {
                ...jestConfig.moduleNameMapper,
                '^@insight/lib$': '<rootDir>/../lib/src',
                '^@insight/lib/(.*)$': '<rootDir>/../lib/src/$1',
            },
        }),
    },
};

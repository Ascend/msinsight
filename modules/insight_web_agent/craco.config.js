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

module.exports = {
    devServer: {
        port: 3010,
        open: false,
        proxy: {
            '/api': {
                target: 'http://127.0.0.1:9090',
                changeOrigin: true,
            },
        },
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
};

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
const isVscode = process.env.REACT_APP_IS_VSCODE === 'true';
const { webpackCfg, configureConfig } = isVscode
  ? require('../build-vscode-config')
  : require('../build-config');

const path = require('path');

const libPath = path.resolve(__dirname, '../lib/src');
const echartsPath = require.resolve('echarts');

module.exports = {
    devServer: {
        port: 3007,
        open: false,
    },
    webpack: {
        alias: webpackCfg.alias,
        configure: (webpackConfig) => {
            // 添加 GLSL 文件处理规则 - 使用 webpack 5 的资产模块
            webpackConfig.module.rules.push({
                test: /\.glsl$/,
                type: 'asset/source'
            });
            webpackConfig.output.workerPublicPath = './';
            // 只在非 VS Code 构建时设置 chunkFilename
            if (!isVscode) {
                webpackConfig.output.chunkFilename = '[name].[contenthash:8].chunk.js';
            }
            return configureConfig(webpackConfig, [libPath, echartsPath]);
        }
    },
};

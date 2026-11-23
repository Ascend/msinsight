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
const { webpackCfg, configureConfig } = process.env.REACT_APP_IS_VSCODE === 'true'
    ? require('../build-vscode-config')
    : require('../build-config');

module.exports = {
    devServer: { port: 3013, open: false },
    webpack: {
        alias: webpackCfg.alias,
        configure: webpackConfig => {
            webpackConfig.entry = path.resolve(__dirname, 'src/bootstrap.tsx');
            return configureConfig(webpackConfig, [path.resolve(__dirname, '../lib/src')]);
        },
    },
};

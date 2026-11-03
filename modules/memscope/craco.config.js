/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
const path = require('path');

// Compile this application and existing project infrastructure only.
module.exports = (() => {
    const { configureConfig } = require('../build-config');
    const applicationSrc = path.resolve(__dirname, 'src');
    const libRoot = path.resolve(__dirname, '../lib');
    const libSrc = path.join(libRoot, 'src');
    const libAliases = Object.fromEntries(Object.entries(require('../lib/package.json').exports).map(([key, target]) => [key === '.' ? '@insight/lib$' : `@insight/lib/${key.slice(2)}$`, path.resolve(libRoot, target)]));
    return {
        devServer: { port: Number(process.env.PORT) || 3007, open: false },
        webpack: {
            alias: { '@': applicationSrc, ...libAliases },
            configure: config => {
                config.module.rules.push({ test: /\.glsl$/, type: 'asset/source' });
                config.output.workerPublicPath = './';
                config.output.chunkFilename = '[name].[contenthash:8].chunk.js';
                const scope = config.resolve.plugins.find(plugin => plugin.constructor.name === 'ModuleScopePlugin');
                if (scope) scope.appSrcs.push(libRoot);
                return configureConfig(config, [libRoot, libSrc, require.resolve('echarts')]);
            },
        },
    };
})();

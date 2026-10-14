/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const { createRequire } = require('node:module');
const path = require('node:path');
const craRequire = createRequire(require.resolve('react-scripts/package.json'));

module.exports = craRequire('babel-jest').default.createTransformer({
    presets: [[craRequire.resolve('babel-preset-react-app'), { runtime: 'automatic' }]],
    plugins: [path.join(__dirname, 'import-meta-url.cjs')],
    babelrc: false,
    configFile: false,
});

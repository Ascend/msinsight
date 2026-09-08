const { webpackCfg, configureConfig } = require('../build-config');
const path = require('path');

module.exports = {
    devServer: {
        port: 3010,
        open: false,
    },
    webpack: {
        alias: webpackCfg.alias,
        configure: (webpackConfig) => configureConfig(webpackConfig, [path.resolve(__dirname, '../lib/src')]),
    },
};

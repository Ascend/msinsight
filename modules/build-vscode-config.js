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
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');

const path = require('path');

const configureConfig = (webpackConfig, paths, moduleName) => {
  if (!moduleName) { moduleName = path.basename(process.cwd()); }
  const oneOfRule = webpackConfig.module.rules.find((r) => r.oneOf);
  if (oneOfRule) {
    const loaderRule = oneOfRule.oneOf.find(
      (r) =>
        r.test &&
        r.test.toString().includes('tsx') &&
        r.loader &&
        r.loader.includes('babel-loader')
    );
    if (loaderRule) {
      if (!Array.isArray(loaderRule.include)) loaderRule.include = [loaderRule.include];
      loaderRule.include.push(...paths, path.resolve(__dirname, '../lib/src'));
    }
  }

  webpackConfig.optimization.runtimeChunk = false;
  webpackConfig.optimization.splitChunks = false;
  if (moduleName && ['cluster', 'compute', 'leaks', 'triton'].includes(moduleName)) {
    return webpackConfig;
  }

  webpackConfig.optimization = {
    ...webpackConfig.optimization,
    runtimeChunk: false,
    splitChunks: {
      chunks: 'all',
      cacheGroups: {
        default: false,
        vendors: false,
        common: false,
        main: {
          name: 'main',
          chunks: 'all',
          enforce: true,
        }

      }
    }
  }
  webpackConfig.module.rules.forEach((rule) => {
    if (rule.oneOf) {
      rule.oneOf.forEach((oneOfRule) => {
        if (oneOfRule.use && Array.isArray(oneOfRule.use)) {
          oneOfRule.use = oneOfRule.use.map((loader) => {
            if (loader.loader && loader.loader.includes('style-loader')) {
              return { loader: MiniCssExtractPlugin.loader }
            }
            return loader;
          })
        }
      });
    }
  })


  webpackConfig.plugins = webpackConfig.plugins.map((plugin) => {
    if (plugin instanceof MiniCssExtractPlugin) {
      return new MiniCssExtractPlugin({
        filename: 'static/css/main.[contenthash:8].css',
        chunkFilename: 'static/css/main.[contenthash:8].css',
        ignoreOrder: true,
      })
    }
    return plugin;
  })

  if (webpackConfig.optimization && webpackConfig.optimization.splitChunks) {
    webpackConfig.optimization.splitChunks = {
      ...webpackConfig.optimization.splitChunks,
      maxAsyncRequests: 1,
      maxInitialRequests: 1,
    };
  }

  return webpackConfig;
};

const htmllist = [
  new HtmlWebpackPlugin({
    template: 'public/index.html',
    filename: 'summary.html',
    chunks: ['summary'],
  }),
  new HtmlWebpackPlugin({
    template: 'public/index.html',
    filename: 'communication.html',
    chunks: ['communication'],
  }),
];

const webpackCfg = {
  clusterConfigure: (webpackConfig, paths) => {
    webpackConfig.entry = {
      main: webpackConfig.entry,
      summary: './src/SummaryIndex.tsx',
      communication: './src/CommunicationIndex.tsx',
    };
    webpackConfig.output.filename = 'static/js/[name].bundle.js';
    webpackConfig.optimization.splitChunks = false;
    webpackConfig.optimization.runtimeChunk = false;
    const webpack = require('webpack');
    webpackConfig.plugins.push(
      new webpack.optimize.LimitChunkCountPlugin({ maxChunks: 1 }), ...htmllist);
    return configureConfig(webpackConfig, paths, 'cluster');
  },
  computeConfigure: (webpackConfig, paths) => {
    webpackConfig.entry = {
      main: webpackConfig.entry,
      detail: './src/detailIndex.ts',
      source: './src/sourceIndex.ts',
      cache: './src/cacheKitIndex.ts',
    };
    webpackConfig.output.filename = 'static/js/[name].bundle.js';
    if (process.env.REACT_APP_IS_VSCODE === 'true') {
      const webpack = require('webpack');
      webpackConfig.plugins.push(
        new webpack.optimize.LimitChunkCountPlugin({ maxChunks: 1 }),
      );
    }
    webpackConfig.plugins.push(
      new HtmlWebpackPlugin({
        template: 'public/index.html',
        filename: 'detail.html',
        chunks: ['detail'],
      }),
      new HtmlWebpackPlugin({
        template: 'public/index.html',
        filename: 'source.html',
        chunks: ['source'],
      }),
      new HtmlWebpackPlugin({
        template: 'public/index.html',
        filename: 'cache.html',
        chunks: ['cache'],
      }),
    );
    return configureConfig(webpackConfig, paths, 'compute');
  },
  alias: {
    '@': path.resolve('src'),
  },
};

module.exports = { webpackCfg, configureConfig };

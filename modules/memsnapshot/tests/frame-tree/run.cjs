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
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { createRequire } = require('node:module');
const modulesRoot = path.resolve(__dirname, '../../..');
const req = createRequire(path.join(modulesRoot, 'package.json'));
const cra = createRequire(req.resolve('react-scripts/scripts/start.js'));
const webpack = cra('webpack');
const { chromium } = req('@playwright/test');
const output = fs.mkdtempSync(path.join(os.tmpdir(), 'insight-frame-tree-'));
const aliases = { '@': path.join(modulesRoot, 'memsnapshot/src') };
for (const [key, value] of Object.entries(req('./lib/package.json').exports)) {
    aliases[`@insight/lib${key === '.' ? '' : key.slice(1)}$`] = path.join(modulesRoot, 'lib', value);
}
process.env.NODE_ENV = 'development';

async function assertDetailsHeight(page, width, context) {
    const dimensions = await page.getByTestId('frame-tree-details').evaluate(element => {
        const chart = document.querySelector('[data-testid="frame-tree-icicle"]').getBoundingClientRect();
        const details = element.getBoundingClientRect();
        return { chartHeight: chart.height, detailsHeight: details.height, chartTop: chart.top, detailsTop: details.top };
    });
    if (width > 1000) {
        assert(Math.abs(dimensions.chartHeight - dimensions.detailsHeight) <= 1,
            `${context}: details height ${dimensions.detailsHeight} must match icicle height ${dimensions.chartHeight}`);
        assert(Math.abs(dimensions.chartTop - dimensions.detailsTop) <= 1, `${context}: panel tops must align`);
    } else {
        assert(dimensions.detailsHeight > 100, `${context}: stacked details must remain readable`);
    }
}

async function assertChartColors(page) {
    const result = await page.evaluate(() => {
        const icicle = document.querySelector('[data-testid="frame-tree-icicle"]');
        const treemap = document.querySelector('[data-testid="frame-tree-chart"]');
        const colors = new Map([...icicle.querySelectorAll('g[data-node-id]')].map(cell =>
            [cell.getAttribute('data-node-id'), cell.querySelector('rect').getAttribute('fill')]));
        const mismatches = [...treemap.querySelectorAll('g[data-node-id]')].filter(cell =>
            cell.querySelector('rect').getAttribute('fill') !== colors.get(cell.getAttribute('data-node-id'))).length;
        const luminance = hex => {
            const [r, g, b] = [1, 3, 5].map(offset => {
                const value = parseInt(hex.slice(offset, offset + 2), 16) / 255;
                return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4;
            });
            return 0.2126 * r + 0.7152 * g + 0.0722 * b;
        };
        const ratios = [icicle, treemap].flatMap(chart => [...chart.querySelectorAll('g[data-node-id]')].flatMap(cell => {
            const fill = luminance(cell.querySelector('rect').getAttribute('fill'));
            return [...cell.querySelectorAll('text')].map(text => {
                const foreground = luminance(text.getAttribute('fill'));
                return (Math.max(fill, foreground) + 0.05) / (Math.min(fill, foreground) + 0.05);
            });
        }));
        return { mismatches, textCount: ratios.length, minimumContrast: Math.min(...ratios) };
    });
    assert.equal(result.mismatches, 0, 'Both charts must use identical colors for each node, including self allocations');
    assert(result.textCount > 0 && result.minimumContrast >= 4.5, `Chart text must remain readable: ${JSON.stringify(result)}`);
}

async function build() {
    const compiler = webpack({
        mode: 'development', devtool: false, context: __dirname,
        entry: path.join(__dirname, 'entry.tsx'), output: { path: output, filename: 'app.js' },
        resolve: { extensions: ['.tsx', '.ts', '.js', '.json'], alias: aliases,
            modules: [path.join(modulesRoot, 'node_modules'), 'node_modules'] },
        module: { rules: [
            { test: /\.tsx?$/, exclude: /node_modules/, use: { loader: cra.resolve('babel-loader'),
                options: { babelrc: false, configFile: false, presets: [cra.resolve('babel-preset-react-app')] } } },
            { test: /\.css$/, use: [req.resolve('style-loader'), req.resolve('css-loader')] },
            { test: /\.svg$/, use: [cra.resolve('@svgr/webpack'), cra.resolve('file-loader')] },
            { test: /\.(png|jpg|gif|woff2?|ttf)$/, type: 'asset/resource' },
        ] },
        plugins: [new webpack.DefinePlugin({ 'process.env.NODE_ENV': JSON.stringify('development'),
            'process.env.REACT_APP_TEST': JSON.stringify('1'), 'process.env.REACT_APP_IS_VSCODE': JSON.stringify('false') })],
        optimization: { minimize: false }, performance: { hints: false },
    });
    try {
        await new Promise((resolve, reject) => compiler.run((error, stats) => {
            if (error) return reject(error);
            if (stats.hasErrors()) return reject(new Error(stats.toString({ all: false, errors: true })));
            resolve();
        }));
    } finally {
        await new Promise((resolve, reject) => compiler.close(error => error ? reject(error) : resolve()));
    }
    fs.writeFileSync(path.join(output, 'index.html'),
        '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><div id="root"></div><script src="app.js"></script>');
}

async function main() {
    await build();
    const server = http.createServer((request, response) => {
        const name = path.basename(new URL(request.url, 'http://localhost').pathname) || 'index.html';
        const file = path.join(output, name);
        if (!fs.existsSync(file)) return response.writeHead(404).end();
        response.setHeader('Content-Type', name.endsWith('.js') ? 'application/javascript' : name.endsWith('.html') ? 'text/html' : 'application/octet-stream');
        fs.createReadStream(file).pipe(response);
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    let browser;
    try {
        browser = await chromium.launch({ headless: true, channel: process.env.BROWSER_CHANNEL || undefined });
        const page = await browser.newPage();
        const errors = [];
        page.on('pageerror', error => errors.push(error.message));
        for (const width of [1440, 390]) {
            for (const theme of ['light', 'dark']) {
                await page.setViewportSize({ width, height: 500 });
                await page.goto(`http://127.0.0.1:${server.address().port}/?tabs=1&lang=zhCN&theme=${theme}`);
                const tabs = page.locator('.ant-tabs');
                const colors = await tabs.evaluate(element => ({
                    active: getComputedStyle(element.querySelector('.ant-tabs-tab-active .ant-tabs-tab-btn')).color,
                    close: getComputedStyle(element.querySelector('.ant-tabs-tab-remove')).color,
                }));
                assert.equal(colors.close, theme === 'dark' ? 'rgb(211, 220, 233)' : 'rgb(78, 88, 101)', 'Close icons must use readable theme colors');
                assert.equal(colors.active, theme === 'dark' ? 'rgb(102, 177, 255)' : 'rgb(0, 0, 0)', 'The active tab must remain readable in both themes');
                await tabs.getByRole('tab').last().click();
                assert.equal(await tabs.getByRole('tab').last().getAttribute('aria-selected'), 'true');
                await page.waitForFunction(color =>
                    getComputedStyle(document.querySelector('.ant-tabs-tab:not(.ant-tabs-tab-active) .ant-tabs-tab-btn')).color === color,
                theme === 'dark' ? 'rgb(211, 220, 233)' : 'rgb(78, 88, 101)');
                if (process.env.FRAME_TREE_SCREENSHOT_DIR) {
                    fs.mkdirSync(process.env.FRAME_TREE_SCREENSHOT_DIR, { recursive: true });
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `tabs-${width}-${theme}.png`) });
                }
            }
        }
        for (const width of [1440, 390]) {
            for (const language of ['enUS', 'zhCN']) {
              for (const theme of ['light', 'dark']) {
                await page.setViewportSize({ width, height: 1800 });
                await page.goto(`http://127.0.0.1:${server.address().port}/?lang=${language}&theme=${theme}`);
                await page.getByTestId('frame-tree-ready').waitFor();
                const initialMerge = page.getByRole('checkbox', { name: language === 'zhCN' ? '合并单子链' : 'Merge single-child chains', exact: true });
                assert(await page.getByRole('checkbox', { name: language === 'zhCN' ? '只展示 Python 调用栈' : 'Python frames only', exact: true }).isChecked());
                assert.equal(await initialMerge.isChecked(), true, 'Single-child-chain merging defaults on');
                assert(await page.getByTestId('frame-tree-icicle').locator('[data-merged-count]').count() > 0);
                await initialMerge.uncheck();
                await page.waitForFunction(() => {
                    const chart = document.querySelector('[data-testid="frame-tree-chart"]');
                    return chart && Number(chart.getAttribute('data-layout-width')) <= innerWidth;
                });
                assert.doesNotMatch(await page.getByTestId('frame-tree-context').innerText(), /MOCK/);
                assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), true,
                    `Page overflow at ${width}/${language}`);
                const icicle = page.getByTestId('frame-tree-icicle');
                const treemap = page.getByTestId('frame-tree-chart');
                const details = page.getByTestId('frame-tree-details');
                const controls = page.locator('.frame-tree-toolbar');
                const appearance = await icicle.evaluate(element => {
                    const style = getComputedStyle(element);
                    return { backgroundColor: style.backgroundColor, backgroundImage: style.backgroundImage };
                });
                assert.deepEqual(appearance, {
                    backgroundColor: theme === 'dark' ? 'rgb(31, 35, 41)' : 'rgb(253, 254, 255)',
                    backgroundImage: 'none',
                }, 'Icicle must use the host theme, not the prototype paper skin');
                assert.equal(await controls.getByRole('button', { name: language === 'zhCN' ? '冰柱图' : 'Icicle', exact: true }).count(), 0);
                assert.equal(await controls.getByRole('button', { name: language === 'zhCN' ? '平铺图' : 'Treemap', exact: true }).count(), 0);
                assert.equal(await page.getByRole('tree').count(), 0);
                assert.equal(await treemap.getAttribute('data-view'), 'treemap');
                const rootRect = page.getByTestId('frame-tree-icicle-snapshot-root').locator('rect');
                assert.equal(await rootRect.getAttribute('x'), '42');
                assert.equal(await rootRect.getAttribute('y'), '26');
                assert.equal(await rootRect.getAttribute('height'), '19');
                assert.equal(await rootRect.getAttribute('fill'), theme === 'dark' ? '#35404E' : '#E5EAF1');
                await assertChartColors(page);
                const upper = await icicle.boundingBox();
                const lower = await treemap.boundingBox();
                const board = await details.boundingBox();
                assert(lower.y >= upper.y + upper.height, 'Treemap must stay below the icicle');
                assert(width < 1000 ? board.y >= upper.y + upper.height : board.x >= upper.x + upper.width,
                    'Information board must be right of the icicle, or below it on narrow screens');
                await assertDetailsHeight(page, width, 'Root');
                assert(upper.height >= 600 && upper.height <= 902, 'V2 must increase the stable icicle viewport without unbounded page growth');
                const deepest = await icicle.locator('[data-node-id][data-testid]').evaluateAll(elements =>
                    elements.map(element => ({
                        id: element.getAttribute('data-node-id'),
                        depth: Number(element.getAttribute('data-depth')),
                    })).sort((a, b) => b.depth - a.depth)[0]);
                const target = await icicle.locator('[data-node-id][data-depth="12"][data-testid]').evaluateAll(elements =>
                    elements.map(element => ({
                        id: element.getAttribute('data-node-id'),
                        parent: element.getAttribute('data-parent-id'),
                        name: element.getAttribute('aria-label').split(':')[0],
                        width: Number(element.querySelector('rect').getAttribute('width')),
                    })).sort((a, b) => b.width - a.width)[0]);
                assert(target && target.width >= 6, 'Deep call stacks must remain visible and clickable');
                const hovered = page.getByTestId(`frame-tree-icicle-${target.id}`);
                const originalFill = await hovered.locator('rect').getAttribute('fill');
                await hovered.hover();
                assert.equal(await hovered.locator('rect').getAttribute('fill'), originalFill, 'Hover must preserve branch colors');
                assert.equal(await hovered.locator('rect').evaluate(element => getComputedStyle(element).stroke),
                    theme === 'dark' ? 'rgb(155, 198, 244)' : 'rgb(40, 91, 145)', 'Hover outlines must adapt to the theme');
                await page.waitForFunction(id => document.querySelector('.frame-tree-inspector')?.getAttribute('data-node-id') === id, target.id);
                const ancestorWindow = await page.getByTestId('frame-tree-ancestors').evaluate(element => {
                    const bounds = element.getBoundingClientRect();
                    const panel = element.closest('[data-testid="frame-tree-details"]').getBoundingClientRect();
                    const rows = [...element.querySelectorAll('button')].filter(row => {
                        const box = row.getBoundingClientRect();
                        return box.top >= Math.max(bounds.top, panel.top) - 1 && box.bottom <= Math.min(bounds.bottom, panel.bottom) + 1;
                    });
                    return { height: element.clientHeight, indents: rows.slice(-11).map(row => parseFloat(getComputedStyle(row).paddingLeft)) };
                });
                assert(ancestorWindow.height >= 264, 'The hovered node and at least ten ancestors must fit');
                assert.equal(ancestorWindow.indents.length, 11);
                assert(ancestorWindow.indents.every(indent => indent === 4),
                    'All visible ancestor levels must align without progressive indentation');
                assert(await page.getByTestId('frame-tree-ancestors').evaluate(element =>
                    Array.from(element.querySelectorAll('button')).every(row => {
                        const value = row.querySelector('.frame-tree-ancestor-bytes').getBoundingClientRect();
                        return value.left >= element.getBoundingClientRect().left &&
                            value.right <= element.getBoundingClientRect().right + 1;
                    })), 'The fixed byte column must remain visible beside long call paths');
                assert.equal(await icicle.getByRole('tooltip').count(), 0, 'Icicle popup must be removed');
                const chain = await hovered.evaluate(element => {
                    const svg = element.ownerSVGElement;
                    const nodes = new Map([...svg.querySelectorAll('[data-testid][data-node-id]')].map(node => [node.getAttribute('data-node-id'), node]));
                    const result = [];
                    for (let node = element; node; node = nodes.get(node.getAttribute('data-parent-id'))) {
                        result.push({ active: node.getAttribute('data-call-path'), opacity: getComputedStyle(node).opacity });
                    }
                    return result;
                });
                assert(chain.length >= 13);
                assert(chain.every(node => node.active === 'true' && node.opacity === '1'), 'Hover must highlight the complete root-to-node stack');
                await hovered.click();
                assert.equal(await icicle.getAttribute('data-locked'), 'true');
                const rootCell = page.getByTestId('frame-tree-icicle-snapshot-root');
                await rootCell.hover();
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), target.id);
                assert.equal(await hovered.getAttribute('data-call-path'), 'true');
                const lockedPath = page.getByTestId('frame-tree-ancestors');
                await lockedPath.hover();
                await lockedPath.evaluate(element => { element.scrollTop = 0; });
                await page.mouse.move(0, 0);
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), target.id);
                assert.equal(await hovered.getAttribute('data-call-path'), 'true');
                if (process.env.FRAME_TREE_SCREENSHOT_DIR && width === 1440 && language === 'zhCN' && theme === 'dark') {
                    fs.mkdirSync(process.env.FRAME_TREE_SCREENSHOT_DIR, { recursive: true });
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, 'locked-selection.png'), fullPage: true });
                }
                await hovered.click();
                assert.equal(await icicle.getAttribute('data-locked'), 'false');
                await rootCell.hover();
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), 'snapshot-root');
                assert.equal(await hovered.getAttribute('data-call-path'), null);
                await hovered.press('Space');
                assert.equal(await icicle.getAttribute('data-locked'), 'true');
                await rootCell.hover();
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), target.id);
                await hovered.press('Space');
                assert.equal(await icicle.getAttribute('data-locked'), 'false');
                await page.mouse.move(0, 0);
                await rootCell.hover();
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), 'snapshot-root');
                assert.equal(await icicle.locator('title').count(), 0, 'No duplicate native tooltip');
                assert.equal(await treemap.locator('title').count(), 0, 'No native treemap tooltip');
                await page.mouse.move(0, 0);
                assert.equal(await details.locator('.frame-tree-raw').count(), 0, 'Exact-values/source module must be removed');
                const branch = await treemap.locator('g[data-node-id][data-child-count]').evaluateAll(elements =>
                    elements.filter(element => Number(element.getAttribute('data-child-count')) > 1 &&
                        element.getAttribute('data-node-id') !== 'snapshot-root' &&
                        element.getBoundingClientRect().width > 100 && element.getBoundingClientRect().height > 50)
                        .map(element => element.getAttribute('data-node-id'))[0]);
                assert(branch, 'Full treemap must expose branching nodes beyond the old depth limit');
                const branchCell = treemap.locator(`g[data-node-id="${branch}"]:not([data-child-count="0"])`);
                const branchRect = branchCell.locator('> rect').first();
                await branchRect.hover({ position: { x: 5, y: 2 } });
                assert.equal(await page.getByRole('tooltip').count(), 0, 'Treemap hover must not open a popup');
                assert.equal(await branchCell.getAttribute('data-hovered'), 'true');
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), branch);
                await branchRect.click({ position: { x: 5, y: 2 } });
                assert.equal(await treemap.getAttribute('data-locked'), 'true', 'Treemap click must lock the inspector');
                const lockedDetails = await details.innerText();
                await rootCell.hover();
                await treemap.locator('g[data-node-id="snapshot-root"] > rect').first().hover({ position: { x: 5, y: 2 } });
                await details.locator('[data-testid="frame-tree-ancestors"]').evaluate(element => { element.scrollTop = 0; });
                assert.equal(await details.innerText(), lockedDetails, 'Hover in either chart and scrolling must not replace locked data');
                await branchRect.click({ position: { x: 5, y: 2 } });
                assert.equal(await treemap.getAttribute('data-locked'), 'false');
                await rootCell.hover();
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), 'snapshot-root');
                await branchRect.hover({ position: { x: 5, y: 2 } });
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), branch);
                if (process.env.FRAME_TREE_SCREENSHOT_DIR && width === 1440 && language === 'zhCN' && theme === 'dark') {
                    fs.mkdirSync(process.env.FRAME_TREE_SCREENSHOT_DIR, { recursive: true });
                    await treemap.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, 'treemap-no-popup.png') });
                }
                const beforeDrill = await treemap.evaluate(element => ({ top: element.getBoundingClientRect().top + scrollY, scroll: scrollY }));
                await branchRect.dblclick({ position: { x: 5, y: 2 } });
                assert.equal(await treemap.getAttribute('data-focus'), branch);
                assert.equal(await icicle.getAttribute('data-focus'), 'snapshot-root', 'Treemap navigation must not change the icicle focus');
                const afterDrill = await treemap.evaluate(element => ({ top: element.getBoundingClientRect().top + scrollY, scroll: scrollY }));
                assert.deepEqual(afterDrill, beforeDrill, 'Treemap drill-down must not move the chart or the page');
                await page.getByTestId('frame-tree-treemap-navigation')
                    .getByRole('button', { name: language === 'zhCN' ? '返回根' : 'Back to root', exact: true }).click();
                const search = page.getByRole('textbox');
                await search.fill(target.id);
                await page.getByTestId(`frame-tree-search-${target.id}`).click();
                assert((await details.innerText()).includes(target.name));
                assert.equal(await treemap.getAttribute('data-focus'), target.parent);
                assert.equal(await icicle.getAttribute('data-focus'), target.parent);
                await assertDetailsHeight(page, width, 'Search focus');
                await search.fill(deepest.id);
                await page.getByTestId(`frame-tree-search-${deepest.id}`).click();
                await assertDetailsHeight(page, width, 'Short subtree');
                if (width > 1000) {
                    assert.equal((await icicle.boundingBox()).height, upper.height, 'Focused stacks must keep the viewport height stable');
                    assert(await page.getByTestId('frame-tree-ancestors').evaluate(element => {
                        element.scrollTop = 40;
                        const scrolls = element.scrollHeight > element.clientHeight && element.scrollTop > 0;
                        element.scrollTop = element.scrollHeight;
                        return scrolls;
                    }), 'Long ancestor paths must scroll inside the inspector');
                }
                if (process.env.FRAME_TREE_SCREENSHOT_DIR && width === 1440 && language === 'zhCN') {
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `selected-path-${theme}.png`), fullPage: true });
                }
                await search.fill('');
                await controls.getByRole('button', { name: language === 'zhCN' ? '返回根' : 'Back to root', exact: true }).click();
                assert.equal(await treemap.getAttribute('data-view'), 'treemap');
                await assertDetailsHeight(page, width, 'Back to root');
                assert.deepEqual(await icicle.locator('.icicle-stage').evaluate(element => ({
                    left: element.scrollLeft, top: element.scrollTop,
                })), { left: 0, top: 0 }, 'Back to root must restore the beginning of the icicle viewport');
                const alternate = theme === 'dark' ? 'light' : 'dark';
                await page.evaluate(value => window.setTheme(value === 'dark'), alternate);
                await page.waitForFunction(value => document.querySelector('[data-testid="frame-tree-icicle"]').getAttribute('data-theme') === value, alternate);
                assert.equal(await rootRect.getAttribute('fill'), alternate === 'dark' ? '#35404E' : '#E5EAF1');
                await assertChartColors(page);
                await page.evaluate(value => window.setTheme(value === 'dark'), theme);
                await page.waitForFunction(value => document.querySelector('[data-testid="frame-tree-icicle"]').getAttribute('data-theme') === value, theme);
                const mergeToggle = page.getByRole('checkbox', { name: language === 'zhCN' ? '合并单子链' : 'Merge single-child chains', exact: true });
                assert.equal(await mergeToggle.isChecked(), false);
                const beforeMerge = await icicle.locator('.icicle-cell').count();
                await mergeToggle.check();
                assert(await icicle.locator('.icicle-cell').count() < beforeMerge);
                const mergedCell = icicle.locator('[data-merged-count]').first();
                const mergedCount = Number(await mergedCell.getAttribute('data-merged-count'));
                assert(mergedCount > 1);
                assert((await mergedCell.getAttribute('aria-label')).includes(`\u00d7${mergedCount}`));
                assert.equal(await mergedCell.locator('rect').count(), 1, 'A merged stack has one outer block');
                assert.equal(Number(await mergedCell.locator('rect').getAttribute('height')), 19, 'Merged icicle blocks stay on one compact row');
                assert.equal(await mergedCell.locator('foreignObject').count(), 0, 'No per-block frame list or scrollbar');
                await mergedCell.click();
                const lockedGroup = await mergedCell.getAttribute('data-node-id');
                await rootCell.hover();
                assert.equal(await icicle.getAttribute('data-locked'), 'true');
                assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), lockedGroup);
                assert.equal(await mergedCell.getAttribute('data-call-path'), 'true');
                assert.equal(await details.locator('[data-merged="true"]').count(), mergedCount);
                assert((await details.locator('.frame-tree-ancestors').innerText()).includes('.'));
                assert.equal(await icicle.getByRole('tooltip').count(), 0);
                await assertDetailsHeight(page, width, 'Merged chain');
                if (process.env.FRAME_TREE_SCREENSHOT_DIR) {
                    fs.mkdirSync(process.env.FRAME_TREE_SCREENSHOT_DIR, { recursive: true });
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `frame-tree-${width}-${language}-${theme}.png`), fullPage: true });
                    await icicle.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `icicle-${width}-${language}-${theme}.png`) });
                    await treemap.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `treemap-${width}-${language}-${theme}.png`) });
                }
                await mergeToggle.uncheck();
                assert.equal(await icicle.getAttribute('data-locked'), 'false');
                assert.equal(await icicle.locator('.icicle-cell').count(), beforeMerge);
              }
            }
        }
        await page.setViewportSize({ width: 1440, height: 600 });
        await page.goto(`http://127.0.0.1:${server.address().port}/?lang=zhCN&theme=dark`);
        await page.getByTestId('frame-tree-ready').waitFor();
        await assertDetailsHeight(page, 1440, 'Short desktop viewport');
        const shortDetails = page.getByTestId('frame-tree-details');
        assert(await shortDetails.evaluate(element => {
            element.scrollTop = element.scrollHeight;
            const bounds = element.getBoundingClientRect();
            const button = element.querySelector('footer button').getBoundingClientRect();
            return button.top >= bounds.top && button.bottom <= bounds.bottom;
        }), 'Inspector controls must remain reachable on short screens');
        await page.setViewportSize({ width: 1440, height: 900 });
        await page.goto(`http://127.0.0.1:${server.address().port}/?lang=zhCN&theme=dark&deep=1`);
        await page.getByTestId('frame-tree-ready').waitFor();
        await page.getByRole('checkbox', { name: '只展示 Python 调用栈', exact: true }).uncheck();
        await page.getByRole('checkbox', { name: '合并单子链', exact: true }).uncheck();
        const deepIcicle = page.getByTestId('frame-tree-icicle');
        const viewport = deepIcicle.locator('.icicle-stage');
        await viewport.evaluate(element => { element.scrollTop = 1000; });
        await page.getByTestId('frame-tree-icicle-deep-50').dblclick({ position: { x: 100, y: 8 } });
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-50');
        assert.equal(await viewport.evaluate(element => element.scrollTop), 0, 'Drill-down must show the new focused root, not preserve the old scroll offset');
        await viewport.evaluate(element => { element.scrollTop = 500; });
        await page.getByTestId('frame-tree-icicle-deep-80').press('Enter');
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-80');
        assert.equal(await viewport.evaluate(element => element.scrollTop), 0, 'Keyboard drill-down must show the new focused root');
        await viewport.evaluate(element => { element.scrollTop = 500; });
        await page.locator('.frame-tree-toolbar').getByRole('button', { name: '上一级', exact: true }).click();
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-79');
        assert.equal(await viewport.evaluate(element => element.scrollTop), 0, 'Parent navigation must show the new focused root');
        await viewport.evaluate(element => { element.scrollTop = 500; });
        await page.locator('.frame-tree-path').first().getByRole('button').first().click();
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-0');
        assert.equal(await viewport.evaluate(element => element.scrollTop), 0, 'Ancestor navigation must show the new focused root');
        await viewport.evaluate(element => { element.scrollTop = 3000; });
        await page.getByTestId('frame-tree-icicle-deep-180').hover();
        await page.waitForFunction(() => document.querySelector('.frame-tree-inspector')?.getAttribute('data-node-id') === 'deep-180');
        assert.equal(await deepIcicle.getByRole('tooltip').count(), 0);
        assert.equal(await page.getByTestId('frame-tree-ancestors').getByRole('button').count(), 181,
            'All original frames remain in the inspector path');
        if (process.env.FRAME_TREE_SCREENSHOT_DIR) {
            await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, 'deep-path.png'), fullPage: true });
        }
        await page.mouse.move(0, 0);
        await page.setViewportSize({ width: 1440, height: 1600 });
        await page.getByTestId('frame-tree-icicle-deep-180').hover();
        await page.setViewportSize({ width: 1440, height: 800 });
        await page.waitForFunction(() => {
            const list = document.querySelector('[data-testid="frame-tree-ancestors"]');
            const current = list.querySelector('[aria-current="true"]').getBoundingClientRect();
            const bounds = list.getBoundingClientRect();
            return current.top >= bounds.top && current.bottom <= bounds.bottom + 1;
        });
        await page.getByTestId('frame-tree-icicle-deep-180').focus();
        await page.keyboard.press('Escape');
        assert.equal(await page.getByRole('tooltip').count(), 0, 'Icicle popup stays removed');
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-0');
        const deepMerge = page.getByRole('checkbox', { name: '合并单子链', exact: true });
        await deepMerge.check();
        const mergedChain = page.getByTestId('frame-tree-icicle-deep-1');
        assert.equal(await mergedChain.getAttribute('data-merged-count'), '200');
        await mergedChain.click();
        assert.equal(await page.getByTestId('frame-tree-ancestors').getByRole('button').count(), 201);
        assert.equal(await page.locator('.frame-tree-ancestors [data-merged="true"]').count(), 200);
        assert.equal(Number(await mergedChain.locator('rect').getAttribute('height')), 19);
        assert.equal(await mergedChain.locator('foreignObject').count(), 0);
        const deepSearch = page.getByRole('textbox');
        await deepSearch.fill('deep-180');
        await page.getByTestId('frame-tree-search-deep-180').click();
        assert.equal(await deepIcicle.getAttribute('data-focus'), 'deep-179');
        for (const navigation of [page.getByRole('navigation', { name: '调用路径', exact: true }),
            page.getByRole('navigation', { name: '平铺图调用路径', exact: true })]) {
            assert.equal(await navigation.getByRole('button').count(), 2);
            assert.equal(await navigation.getByRole('button').last().innerText(), 'call_179 \u00d7179');
        }
        if (process.env.FRAME_TREE_SCREENSHOT_DIR) {
            await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, 'compact-navigation.png'), fullPage: true });
        }
        await deepSearch.fill('');
        await page.locator('.frame-tree-toolbar').getByRole('button', { name: '返回根', exact: true }).click();
        await deepMerge.uncheck();
        assert.equal(await deepIcicle.locator('[data-node-id][data-testid]').count(), 201);
        for (const width of [1440, 390]) {
          for (const language of ['zhCN', 'enUS']) {
            for (const theme of ['light', 'dark']) {
                await page.setViewportSize({ width, height: 1000 });
                await page.goto(`http://127.0.0.1:${server.address().port}/?lang=${language}&theme=${theme}&presentation=1`);
                await page.getByTestId('frame-tree-ready').waitFor();
                const filter = page.getByRole('checkbox', { name: language === 'zhCN' ? '只展示 Python 调用栈' : 'Python frames only', exact: true });
                assert(await filter.isChecked(), 'Python-only must default to enabled');
                const icicle = page.getByTestId('frame-tree-icicle');
                const treemap = page.getByTestId('frame-tree-chart');
                assert.equal(await icicle.locator('[data-node-id="native"]').count(), 0);
                assert.equal(await treemap.locator('[data-node-id="native"]').count(), 0);
                for (const chart of [icicle, treemap]) {
                    const geometry = await chart.locator('g[data-node-id^="tiny-"] > rect').evaluateAll(rects => rects.map(rect => {
                        const bounds = rect.getBoundingClientRect();
                        return { width: bounds.width, height: bounds.height };
                    }));
                    assert.equal(geometry.length, 100, 'Every positive tiny allocation must be rendered');
                    assert(geometry.every(box => box.width >= 6 - 0.01 && box.height >= 6 - 0.01), 'Tiny cells must retain their actual 6px hit dimensions');
                    const smallest = chart.locator('g[data-node-id="tiny-99"] > rect');
                    await smallest.click();
                    assert.equal(await page.locator('.frame-tree-inspector').getAttribute('data-node-id'), 'tiny-99');
                    assert((await page.locator('.frame-tree-inspector-size').innerText()).includes('1 B'));
                    assert.equal(await page.getByRole('tooltip').count(), 0);
                }
                assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Overflow must stay within the charts');
                await page.getByRole('checkbox', { name: language === 'zhCN' ? '合并单子链' : 'Merge single-child chains', exact: true }).uncheck();
                await filter.uncheck();
                assert.equal(await page.getByTestId('frame-tree-icicle-native').count(), 1);
                assert.equal(await treemap.locator('[data-node-id="native"][data-child-count="100"]').count(), 1);
                await page.getByTestId('frame-tree-icicle-native').locator('> rect').click();
                assert.equal(await icicle.getAttribute('data-locked'), 'true');
                await filter.check();
                assert.equal(await icicle.getAttribute('data-locked'), 'false');
                assert.equal(await icicle.getAttribute('data-focus'), 'root');
                assert.equal(await icicle.locator('[data-node-id="native"]').count(), 0);
                const includeUnknown = page.getByRole('checkbox', { name: language === 'zhCN' ? '包含未知' : 'Include unknown', exact: true });
                assert(await includeUnknown.isChecked());
                for (const chart of [icicle, treemap]) {
                    assert.equal(await chart.locator('g[data-node-id="unknown"]').count(), 1);
                    assert.equal(await chart.locator('g[data-node-id="static"], g[data-node-id="empty"], g[data-node-id="preexisting"], g[data-node-id="missing"]').count(), 0);
                }
                await page.getByTestId('frame-tree-icicle-unknown').click();
                const unknownName = language === 'zhCN' ? '未知' : 'Unknown';
                assert.equal(await page.locator('.frame-tree-inspector h4').innerText(), unknownName);
                await treemap.locator('g[data-node-id="tiny-99"] > rect').hover();
                assert.equal(await page.locator('.frame-tree-inspector h4').innerText(), unknownName);
                await includeUnknown.uncheck();
                await page.waitForFunction(() => document.querySelector('[data-testid="frame-tree-icicle-root"]')
                    ?.getAttribute('aria-label')?.endsWith(': 101 B'));
                assert.equal(await icicle.locator('g[data-node-id="unknown"]').count(), 0);
                assert.equal(await treemap.locator('g[data-node-id="unknown"]').count(), 0);
                assert.equal(await icicle.getAttribute('data-locked'), 'false');
                await includeUnknown.check();
                await page.getByTestId('frame-tree-icicle-unknown').waitFor();
                assert.equal(await treemap.locator('g[data-node-id="unknown"]').count(), 1);
                await treemap.locator('g[data-node-id="tiny-99"] > rect').click();
                if (process.env.FRAME_TREE_SCREENSHOT_DIR && language === 'zhCN') {
                    fs.mkdirSync(process.env.FRAME_TREE_SCREENSHOT_DIR, { recursive: true });
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `python-minimum-${width}-${theme}.png`), fullPage: true });
                }
            }
          }
        }
        for (const width of [1440, 390]) {
          for (const language of ['enUS', 'zhCN']) {
            for (const theme of ['light', 'dark']) {
                await page.setViewportSize({ width, height: 500 });
                await page.goto(`http://127.0.0.1:${server.address().port}/?event-axis=1&lang=${language}&theme=${theme}`);
                const axis = page.getByTestId('snapshot-event-marker-axis');
                await axis.waitFor();
                const surface = axis.getByRole('button', { name: language === 'zhCN'
                    ? '单击时间标记栏添加事件，通过标签切换不同时间点的内存拆解'
                    : 'Click the time marker strip to add an event; switch between memory breakdown tabs', exact: true });
                const box = await surface.boundingBox();
                assert(box && box.height > 20);
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 0);
                await surface.hover({ position: { x: box.width / 4, y: box.height / 2 } });
                assert.equal(await page.getByTestId('snapshot-axis-commits').innerText(), '0');
                assert.equal(await page.getByTestId('snapshot-event-preview').count(), 1);
                await surface.click({ position: { x: box.width / 4, y: box.height / 2 } });
                const first = Number(await page.getByTestId('snapshot-axis-event').innerText());
                assert(Math.abs(first - 25) <= 1);
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 1);
                await surface.hover({ position: { x: box.width * 3 / 4, y: box.height / 2 } });
                assert.equal(await axis.locator('[data-time-marker-select] svg').count(), 1, 'Hover preview does not create a second flag');
                await surface.click({ position: { x: box.width * 3 / 4, y: box.height / 2 } });
                const second = Number(await page.getByTestId('snapshot-axis-event').innerText());
                assert(Math.abs(second - 75) <= 1 && second > first);
                assert.equal(await page.getByTestId('snapshot-axis-commits').innerText(), '2');
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 2, 'Both event markers remain visible');
                const plotWidth = Number(await page.getByTestId('snapshot-axis-plot').getAttribute('data-width'));
                await page.getByTestId('snapshot-axis-pan').click();
                const panned = await page.locator('[data-testid="snapshot-event-flag"][data-marker-id="2"]').evaluate(element => parseFloat(element.style.left));
                assert(Math.abs(panned - (second / 100 - 0.25) * plotWidth) < 0.1);
                await page.getByTestId('snapshot-axis-zoom').click();
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 1);
                assert.equal(await axis.count(), 1, 'The marker strip remains available after zooming off the selected point');
                await surface.click({ position: { x: box.width / 2, y: box.height / 2 } });
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 2);
                assert.equal(await page.getByTestId('snapshot-axis-commits').innerText(), '3');
                await page.getByTestId('snapshot-axis-reset').click();
                await surface.press('Enter');
                assert.equal(await page.getByTestId('snapshot-axis-event').innerText(), '50');
                assert.equal(await page.getByTestId('snapshot-axis-commits').innerText(), '4');
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 4);
                await page.getByTestId('snapshot-axis-manager').click();
                const manager = page.getByTestId('snapshot-event-marker-manager');
                assert.equal(await manager.getByTestId('snapshot-event-marker-row').count(), 4);
                const firstRow = manager.getByTestId('snapshot-event-marker-row').first();
                assert.equal(await firstRow.getByRole('button').count(), 2, 'Each breakdown row offers visibility and deletion, without a locate action');
                await firstRow.locator('input:not([type="color"])').fill('Checkpoint');
                await firstRow.locator('input:not([type="color"])').press('Enter');
                assert.equal(await axis.getByRole('button', { name: /Checkpoint/ }).count(), 1);
                await firstRow.getByRole('button', { name: language === 'zhCN' ? '移除拆解：Checkpoint' : 'Remove breakdown: Checkpoint', exact: true }).click();
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 3);
                assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), true);
                if (process.env.FRAME_TREE_SCREENSHOT_DIR) {
                    await page.screenshot({ path: path.join(process.env.FRAME_TREE_SCREENSHOT_DIR, `event-axis-${width}-${language}-${theme}.png`), fullPage: true });
                }
                await manager.getByRole('button', { name: language === 'zhCN' ? '关闭内存拆解管理' : 'Close memory breakdown management', exact: true }).click();
                const marker = axis.locator('[data-time-marker]').first();
                const inlineDelete = marker.locator('[data-marker-delete]');
                await page.mouse.move(0, 0);
                assert.equal(await inlineDelete.isVisible(), false);
                await marker.locator('[data-time-marker-select]').focus();
                assert.equal(await inlineDelete.isVisible(), true, 'Keyboard focus reveals the delete action');
                await surface.focus();
                assert.equal(await inlineDelete.isVisible(), false);
                await marker.hover();
                assert.equal(await inlineDelete.isVisible(), true);
                await inlineDelete.click();
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 2);
                assert.equal(await page.getByTestId('snapshot-axis-commits').innerText(), '4', 'Delete must not create another marker');
                await page.getByTestId('snapshot-axis-manager').click();
                const clear = manager.getByRole('button', { name: language === 'zhCN' ? '移除全部拆解' : 'Remove all breakdowns', exact: true });
                await clear.click();
                const confirmation = page.getByRole('dialog').filter({ hasText: language === 'zhCN'
                    ? '移除所有拆解标记和标签？此操作不会删除快照数据。' : 'Remove all breakdown markers and tabs? Snapshot data will not be deleted.' });
                await confirmation.getByRole('button', { name: language === 'zhCN' ? /取\s*消/ : 'Cancel', exact: true }).click();
                await confirmation.waitFor({ state: 'hidden' });
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 2, 'Cancel must retain all time markers');
                await clear.click();
                await confirmation.getByRole('button', { name: language === 'zhCN' ? /确\s*定/ : 'OK', exact: true }).click();
                await confirmation.waitFor({ state: 'hidden' });
                assert.equal(await page.getByTestId('snapshot-event-flag').count(), 0);
                assert.equal(await manager.getByTestId('snapshot-event-marker-row').count(), 0);
            }
          }
        }
        assert.deepEqual(errors, []);
        console.log('PASS: unified unknown grouping/toggle, default Python filtering, 6px minimum cells, shared locking/navigation, popup-free charts and time marker selection; desktop/mobile, English/Chinese, light/dark.');
    } finally {
        if (browser) await browser.close();
        await new Promise(resolve => server.close(resolve));
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });

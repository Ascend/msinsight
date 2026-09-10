/* Browser regression for #542. Run from modules/leaks: node tests/tooltip-layout/run.cjs
 * Uses existing workspace dependencies. BROWSER_CHANNEL=msedge selects installed Edge.
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
const output = fs.mkdtempSync(path.join(os.tmpdir(), 'insight-tooltip-'));
const aliases = { '@': path.join(modulesRoot, 'leaks/src') };
for (const [key, value] of Object.entries(req('./lib/package.json').exports)) {
    aliases[`@insight/lib${key === '.' ? '' : key.slice(1)}$`] = path.join(modulesRoot, 'lib', value);
}
process.env.NODE_ENV = 'development';

async function buildFixture() {
    const compiler = webpack({
        mode: 'development', devtool: false, context: __dirname,
        entry: path.join(__dirname, 'entry.tsx'), output: { path: output, filename: 'app.js' },
        resolve: { extensions: ['.tsx', '.ts', '.js', '.json'], alias: aliases,
            modules: [path.join(modulesRoot, 'node_modules'), 'node_modules'] },
        module: { rules: [
            { test: /\.tsx?$/, exclude: /node_modules/, use: { loader: cra.resolve('babel-loader'),
                options: { babelrc: false, configFile: false, presets: [cra.resolve('babel-preset-react-app')] } } },
            { test: /\.glsl$/, type: 'asset/source' },
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
    fs.writeFileSync(path.join(output, 'index.html'), '<!doctype html><meta charset="utf-8"><div id="root"></div><script src="app.js"></script>');
}

async function startFixtureServer() {
    const server = http.createServer((request, response) => {
        const name = path.basename(new URL(request.url, 'http://localhost').pathname) || 'index.html';
        const file = path.join(output, name);
        if (!fs.existsSync(file)) { response.writeHead(404).end(); return; }
        response.setHeader('Content-Type', name.endsWith('.js') ? 'application/javascript' : name.endsWith('.html') ? 'text/html' : 'application/octet-stream');
        fs.createReadStream(file).pipe(response);
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    return server;
}

function* layoutScenarios() {
    for (const width of [320, 768, 1440]) {
        for (const lang of ['enUS', 'zhCN']) {
            for (const mode of ['horizontal', 'proportional']) {
                // left: toolbar; topRight: MemoryBlockDiagram and MemoryStateDiagram.
                for (const placement of ['left', 'topRight']) {
                    yield { width, lang, mode, placement };
                }
            }
        }
    }
}

// Serialized by Playwright: keep DOM inspection self-contained in browser scope.
function inspectTooltipLayout(element) {
    const outer = element.getBoundingClientRect();
    const issues = [];
    if (outer.left < -1 || outer.right > innerWidth + 1) issues.push('outside viewport');
    const walker = document.createTreeWalker(element, NodeFilter.SHOW_TEXT);
    while (walker.nextNode()) {
        const node = walker.currentNode;
        if (!node.textContent.trim()) continue;
        const range = document.createRange();
        range.selectNodeContents(node);
        for (const rect of range.getClientRects()) {
            if (rect.left < outer.left - 1 || rect.right > outer.right + 1 ||
                rect.top < outer.top - 1 || rect.bottom > outer.bottom + 1) issues.push(node.textContent);
        }
    }
    for (const child of element.querySelectorAll('*')) {
        const style = getComputedStyle(child);
        if (['hidden', 'clip'].includes(style.overflowX) && child.scrollWidth > child.clientWidth + 1) {
            issues.push('clipped content');
        }
    }
    return issues;
}

async function checkScenario(page, baseURL, { width, lang, mode, placement }) {
    const scenario = `${width}/${lang}/${mode}/${placement}`;
    await page.setViewportSize({ width, height: 700 });
    await page.goto(`${baseURL}/?lang=${lang}&mode=${mode}&placement=${placement}`);
    await page.getByRole('button', { name: 'Zoom mode' }).hover();
    const tooltip = page.getByRole('tooltip');
    await tooltip.waitFor({ state: 'visible' });
    // Allow rc-trigger positioning and its entry animation to settle.
    await page.waitForTimeout(300);
    assert.match(await tooltip.innerText(), /H/);
    if (lang === 'enUS') assert.match(await tooltip.innerText(), /Current wheel mode/);
    assert.deepEqual(await tooltip.evaluate(inspectTooltipLayout), [], scenario);
}

async function checkScenarios(browser, baseURL) {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    let cases = 0;
    for (const scenario of layoutScenarios()) {
        await checkScenario(page, baseURL, scenario);
        cases++;
    }
    assert.equal(cases, 24);
    assert.deepEqual(errors, []);
    console.log(`PASS: ${cases} real tooltip layout cases; no clipping or page errors.`);
}

async function runBrowserChecks(baseURL) {
    const browser = await chromium.launch({ headless: true, channel: process.env.BROWSER_CHANNEL || undefined });
    try {
        await checkScenarios(browser, baseURL);
    } finally {
        await browser.close();
    }
}

async function main() {
    await buildFixture();
    const server = await startFixtureServer();
    try {
        await runBrowserChecks(`http://127.0.0.1:${server.address().port}`);
    } finally {
        await new Promise(resolve => server.close(resolve));
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });

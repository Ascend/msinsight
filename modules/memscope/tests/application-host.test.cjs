/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
// Integration test against real production bundles using a synthetic host and dataset.
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { chromium } = require('@playwright/test');
const modulesRoot = path.resolve(__dirname, '../..');
const coldMode = process.argv.includes('--cold-snapshot');
const buildRootFor = kind => process.env.MEMORY_TEST_BUILDS_ROOT
    ? path.resolve(process.env.MEMORY_TEST_BUILDS_ROOT, 'memory-browser-' + kind, 'build')
    : path.join(modulesRoot, kind === 'scope' ? 'memscope' : 'memsnapshot', 'build');
const host = `<!doctype html><html><body><iframe id="memory" name="MemScope" style="width:1400px;height:1000px;border:0"></iframe><script>
window.requests = []; window.initCount = 0; window.kind = 'scope';
window.loadApplication = async kind => {
  window.kind = kind;
  const previous = document.getElementById('memory');
  const application = kind === 'snapshot' ? 'MemSnapshot' : 'MemScope';
  const frame = document.createElement('iframe');
  frame.id = 'memory'; frame.name = application; frame.style.cssText = previous.style.cssText;
  previous.replaceWith(frame);
  frame.src = '/' + kind + '/';
};
window.addEventListener('message', event => {
  let message; try { message = typeof event.data === 'string' ? JSON.parse(event.data) : event.data; } catch { return; }
  if (!message || event.source !== document.getElementById('memory').contentWindow) return;
  const send = body => event.source.postMessage(JSON.stringify(body), '*');
  const snapshot = window.kind === 'snapshot';
  if (message.from !== (snapshot ? 'MemSnapshot' : 'MemScope')) throw new Error('Unexpected application identity: ' + message.from);
  if (message.event === 'getParseStatus') {
    window.initCount++;
    const slices = { '0': { eventCount: 2, sliceCount: 2, readySlices: [0,1], slices: [
      { index: 0, startEventId: 0, endEventId: 0, ready: true }, { index: 1, startEventId: 1, endEventId: 1, ready: true }
    ] } };
    const session = { language: 'enUS', module: snapshot ? 'memsnapshot' : 'leaks', deviceIds: { '0': [snapshot ? 'BLOCK' : 'PTA'] },
      threadIds: [42], fileHash: snapshot ? 'synthetic-snapshot' : 'synthetic-scope', dbPath: snapshot ? '/fixture/snapshot.pickle' : '/fixture/scope.db',
      memSnapshotParseFileId: '/fixture/snapshot.pickle', memSnapshotParseLoading: false, memSnapshotParseProgress: 100,
      snapshotParsingComplete: true, snapshotSlices: slices };
    window.snapshotProgress = progress => send({ event: 'parse/memSnapshotProgress', body: { fileId: session.dbPath, progress } });
    window.finishSnapshot = () => send({ event: 'parse/leaksMemoryCompleted', body: session });
    // Include the real directory reply, which initializes and clears prior application state.
    for (const key of message.body.requests) {
      if (key === 'theme') send({ event: 'setTheme', body: { isDark: false } });
      else if (key === 'directory') send({ event: 'switchDirectory', body: { selectedFilePath: session.dbPath } });
      else if (key === 'memSnapshotSession') send({ event: 'updateSession', body: ${JSON.stringify(coldMode)} ? { ...session, fileHash: '', deviceIds: {}, snapshotSlices: {}, snapshotParsingComplete: false, memSnapshotParseLoading: true, memSnapshotParseProgress: 0 } : session });
      else if (key in session) send({ event: 'updateSession', body: { [key]: session[key] } });
    }
  }
  if (message.event === 'request') {
    window.requests.push({ kind: window.kind, id: message.id, module: message.module, command: message.args.command, params: message.args.params });
    const command = message.args.command;
    let body = {};
    if (command.endsWith('/blocks')) body = { blocks: [], headers: [], total: 0, minTimestamp: 0, maxTimestamp: 100, minSize: 0, maxSize: 1024 };
    else if (command.endsWith('/allocations')) body = { allocations: [{ timestamp: 0, totalSize: 0 }, { timestamp: 100, totalSize: 1024 }], minTimestamp: 0, maxTimestamp: 100 };
    else if (command.endsWith('/traces')) body = { traces: [{ depth: 0, startTimestamp: 0, endTimestamp: 100, func: 'synthetic_function' }], minTimestamp: 0, maxTimestamp: 100, maxDepth: 1 };
    else if (command.endsWith('/details')) body = { size: 0, name: 'synthetic', subNodes: [] };
    else if (command.endsWith('/events')) body = { events: [], headers: [], total: 0 };
    else if (command.endsWith('/allocationLines')) body = { reservedLine: [], processUsedLine: [], deviceUsedLine: [] };
    else if (command.endsWith('/leakStats')) body = { totalSize: 0, maxSize: 0, minSize: 0 };
    else if (command.endsWith('/state')) body = { segments: [] };
    send({ event: 'request', id: message.id, body });
  }
});
window.loadApplication(${JSON.stringify(coldMode ? 'snapshot' : 'scope')});
</script></body></html>`;
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.json': 'application/json', '.png': 'image/png' };
const server = http.createServer((request, response) => {
    const pathname = new URL(request.url, 'http://localhost').pathname;
    if (pathname === '/favicon.ico') { response.writeHead(204); response.end(); return; }
    if (pathname === '/') { response.setHeader('Content-Type', 'text/html'); response.end(host); return; }
    const match = pathname.match(/^\/(scope|snapshot)\/(.*)$/);
    if (!match) { response.writeHead(404); response.end(); return; }
    const buildRoot = buildRootFor(match[1]);
    const file = path.resolve(buildRoot, match[2] || 'index.html');
    if (!file.startsWith(buildRoot + path.sep) || !fs.existsSync(file)) { response.writeHead(404); response.end(); return; }
    response.setHeader('Content-Type', mime[path.extname(file)] || 'application/octet-stream');
    fs.createReadStream(file).pipe(response);
});
(async () => {
    let browser;
    try {
        await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
        browser = await chromium.launch({ channel: process.env.MEMORY_TEST_BROWSER || 'msedge', headless: true });
        const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
        const errors = [];
        page.on('response', response => { if (response.status() >= 400) errors.push('Resource failed: ' + response.url()); });
        page.on('pageerror', error => { errors.push(error.message); console.error('Application error:', error.message); });
        page.on('console', message => { if (message.type() === 'error') console.error('Browser console:', message.text()); });
        await page.goto(`http://127.0.0.1:${server.address().port}/`);
        if (coldMode) {
            const frame = page.frameLocator('#memory');
            const overlay = frame.getByTestId('snapshot-parse-overlay');
            await overlay.waitFor({ state: 'visible' });
            assert.equal(await page.evaluate(() => window.requests.length), 0, 'No data queries before parsing metadata arrives');
            await page.evaluate(() => window.snapshotProgress(37));
            await overlay.getByText('37%', { exact: true }).waitFor();
            await page.evaluate(() => window.finishSnapshot());
            await overlay.waitFor({ state: 'hidden' });
            await frame.locator('[data-testid="blockDiagramSection"][data-loading-blocks="false"]').waitFor();
            await page.waitForFunction(() => window.requests.some(request => request.command === 'Memory/snapshot/blocks'));
            assert.deepEqual(errors, [], 'No errors during cold parse');
            console.log(JSON.stringify({ passed: true, scenarios: ['cold snapshot parse overlay', 'progress update', 'completion triggers loading without reimport'] }));
            return;
        }

        await page.waitForFunction(() => window.requests.some(request => request.command === 'Memory/leaks/traces'));
        let frame = page.frameLocator('#memory');
        await frame.locator('[data-testid="blockDiagramSection"][data-loading-blocks="false"]').waitFor();
        const firstScopeIds = await page.evaluate(() => window.requests.filter(request => request.kind === 'scope').map(request => request.id));
        await page.evaluate(() => window.loadApplication('snapshot'));
        await frame.locator('#select-snapshot-window').waitFor();
        await page.waitForFunction(() => window.requests.some(request => request.command === 'Memory/snapshot/blocks'));
        await frame.locator('[data-testid="blockDiagramSection"][data-loading-blocks="false"]').waitFor();
        await frame.locator('#select-snapshot-window').press('ArrowUp');
        await frame.locator('.ant-select-dropdown:visible .ant-select-item-option').first().click();
        await page.waitForFunction(() => window.requests.some(request => request.command === 'Memory/snapshot/blocks' && request.params.sliceIndex === 0));
        const snapshotIds = await page.evaluate(() => window.requests.filter(request => request.kind === 'snapshot').map(request => request.id));
        assert.ok(snapshotIds.every(id => !firstScopeIds.includes(id)), 'Reloaded application must use a different request namespace');
        await page.evaluate(() => window.loadApplication('scope'));
        await page.waitForFunction(count => window.initCount >= count, 3);
        await frame.locator('#select-threadId').waitFor();
        await frame.locator('[data-testid="blockDiagramSection"][data-loading-blocks="false"]').waitFor();
        const requests = await page.evaluate(() => window.requests);
        assert.ok(requests.filter(request => request.kind === 'scope').every(request => !request.command.startsWith('Memory/snapshot/')));
        assert.ok(requests.filter(request => request.kind === 'snapshot').every(request => !request.command.startsWith('Memory/leaks/')));
        assert.ok(requests.every(request => request.module === 'leaks'), 'Backend routing remains compatible');
        assert.deepEqual(errors, [], 'No application or Worker runtime errors');
        console.log(JSON.stringify({ passed: true, scenarios: ['Scope initial load', 'Snapshot initial load', 'Snapshot window switch', 'Return to Scope', 'request namespace isolation', 'feature API isolation'], requests: requests.length }, null, 2));
    } finally {
        await browser?.close();
        await new Promise(resolve => server.close(resolve));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });

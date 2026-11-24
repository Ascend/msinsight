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
import { CommunicationStore } from '../store/CommunicationStore';
import { HostBridge } from '../bridge/HostBridge';
function host(event: string, body: object, extra: object = {}): void {
    window.dispatchEvent(new MessageEvent('message', { source: window.parent, origin: window.location.origin, data: JSON.stringify({ event, body, ...extra }) }));
}
let bridge: HostBridge;
let store: CommunicationStore;
beforeEach(() => { store = new CommunicationStore(); bridge = new HostBridge(store); jest.spyOn(window.parent, 'postMessage').mockImplementation(() => {}); bridge.start(); });
afterEach(() => bridge.dispose());
it('uses the real request envelope and accepts only its parent replies', async () => {
    const promise = bridge.request('communication/matrix/group', { clusterPath: 'c' });
    const request = JSON.parse((window.parent.postMessage as jest.Mock).mock.calls.at(-1)[0]);
    expect(request).toMatchObject({ event: 'request', module: 'communication', args: { command: 'communication/matrix/group', params: { clusterPath: 'c' } } });
    window.dispatchEvent(new MessageEvent('message', { source: null, origin: window.location.origin, data: JSON.stringify({ event: 'request', id: request.id, body: { wrong: true } }) }));
    window.dispatchEvent(new MessageEvent('message', { source: window.parent, origin: 'https://other.example', data: { event: 'request', id: request.id, body: { wrong: true } } }));
    host('request', { data: ['correct'] }, { id: request.id });
    await expect(promise).resolves.toEqual({ data: ['correct'] });
});
it('ignores a reply for the previous page instance and still routes Timeline requests', async () => {
    const old = bridge.request('communication/advisor', {});
    const oldId = JSON.parse((window.parent.postMessage as jest.Mock).mock.calls.at(-1)[0]).id;
    bridge.dispose();
    await expect(old).rejects.toHaveProperty('name', 'AbortError');
    bridge = new HostBridge(store);
    bridge.start();
    const current = bridge.request('unit/kernelDetail', {}, undefined, 'timeline');
    const message = JSON.parse((window.parent.postMessage as jest.Mock).mock.calls.at(-1)[0]);
    expect(message.module).toBe('timeline');
    expect(message.id).not.toBe(oldId);
    host('request', { stale: true }, { id: oldId });
    host('request', { current: true }, { id: message.id });
    await expect(current).resolves.toEqual({ current: true });
});
it('aborts outstanding requests on same-project reset and ignores late replies', async () => {
    const promise = bridge.request('communication/operatorLists', {});
    host('module.reset', {});
    await expect(promise).rejects.toHaveProperty('name', 'AbortError');
    expect(store.generation).toBe(1);
});
it('preserves context on theme changes and parses host lifecycle fields', () => {
    host('frame/loaded', { selectedProjectName: 'p', pageInfo: { cluster: { selectedClusterPath: 'c', clusterList: [{ name: 'c', path: 'c', parsed: true, durationParsed: false }] }, timeline: { unitCount: 3 } } });
    const generation = store.generation;
    host('setTheme', { isDark: false }); host('switchLanguage', { lang: 'zhCN' });
    expect(store).toMatchObject({ project: 'p', clusterPath: 'c', unitCount: 3, dark: false, language: 'zh', generation });
    host('clusterBaselineToggle', { status: true }); expect(store.compare).toBe(true);
});
it('does not interpret a rank comparison as cluster comparison', () => {
    host('switchDirectory', { isCompare: true }); expect(store.compare).toBe(false);
});
it('retains an external locate intent before the page mounts and receives initial context', () => {
    host('locateCommunication', { iterationId: 2, operatorName: 'op', stage: '(0, 1)' });
    host('frame/loaded', { selectedProjectName: 'p', selectedClusterPath: 'c' });
    expect(store.locationIntent).toEqual({ iterationId: '2', operatorName: 'op', stage: '(0, 1)' });
    expect(store.iterationId).toBe('2');
});
it('cleans aborted requests and accepts responses after several minutes without timing out', async () => {
    jest.useFakeTimers();
    try {
        const controller = new AbortController(); const aborted = bridge.request('command', {}, controller.signal); controller.abort();
        await expect(aborted).rejects.toHaveProperty('name', 'AbortError');
        const delayed = bridge.request('command', {});
        const request = JSON.parse((window.parent.postMessage as jest.Mock).mock.calls.at(-1)[0]);
        const settled = jest.fn();
        void delayed.then(settled, settled);
        jest.advanceTimersByTime(10 * 60 * 1000);
        await Promise.resolve();
        expect(settled).not.toHaveBeenCalled();
        host('request', { data: [] }, { id: request.id });
        await expect(delayed).resolves.toEqual({ data: [] });
    } finally { jest.useRealTimers(); }
});
it('supports file-based desktop hosts with an opaque origin', () => {
    jest.spyOn(document, 'referrer', 'get').mockReturnValue('file:///opt/insight/framework/index.html');
    bridge.send('getLanguage', {});
    expect(window.parent.postMessage).toHaveBeenLastCalledWith(expect.any(String), '*');
});
it('rejects synchronous transport failures', async () => {
    (window.parent.postMessage as jest.Mock).mockImplementation(() => { throw new Error('closed host'); });
    await expect(bridge.request('command', {})).rejects.toThrow('closed host');
});

it('cancels requests when the user changes cluster through the store', async () => {
    store.updateHost({ project: 'p', clusterPath: 'first' });
    const promise = bridge.request('communication/operatorLists', {});
    store.selectCluster('second');
    await expect(promise).rejects.toHaveProperty('name', 'AbortError');
});
it('keeps requests alive during metadata updates and supports restarting the bridge', async () => {
    const promise = bridge.request('command', {});
    const request = JSON.parse((window.parent.postMessage as jest.Mock).mock.calls.at(-1)[0]);
    store.updateHost({ dark: false, unitCount: 5 });
    host('request', { data: [] }, { id: request.id });
    await expect(promise).resolves.toEqual({ data: [] });
    const active = bridge.request('command', {});
    bridge.dispose();
    await expect(active).rejects.toHaveProperty('name', 'AbortError');
    bridge.start();
    const restarted = bridge.request('command', {});
    store.resetHost();
    await expect(restarted).rejects.toHaveProperty('name', 'AbortError');
});

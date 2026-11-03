/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import connector from '@/connection';
import TabPane from '../../components/TabPane/Index';
import type { Session } from '../../entity/session';
import { modulesConfig, type ModuleConfig } from '../../moduleConfig';
import { updateSession } from '@/connection/notificationHandler';
import { getAvailableModules, updateDataScene } from '../../components/TabPane/Index';

jest.mock('@/moduleConfig', () => jest.requireActual('../../moduleConfig'), { virtual: true });
jest.mock('@/utils/enum', () => ({ SessionAction: { SWITCH_ACTIVE_MODULE: 'switch active module' } }), { virtual: true });
jest.mock('@/utils/Request', () => ({ getModuleConfig: jest.fn() }), { virtual: true });
jest.mock('@/connection/notificationHandler', () => ({ updateSession: jest.fn() }), { virtual: true });
jest.mock('@/connection', () => ({ __esModule: true, default: { send: jest.fn() } }), { virtual: true });
jest.mock('@/vscode-adapter', () => ({
    onDivLoad: jest.fn(), isVscodePluginEnvironment: jest.fn(() => false), isVscodeEnv: jest.fn(() => false),
}), { virtual: true });
jest.mock('@insight/lib/utils', () => ({ safeJSONParse: jest.fn() }), { virtual: true });

const config = (name: string, flags: Partial<ModuleConfig> = {}): ModuleConfig => ({
    name, requestName: 'timeline', attributes: {}, ...flags,
});
const modules = [
    config('Timeline', { isDefault: true }),
    config('NUMA', { hasNumaData: true }),
    config('PluginA', { isDefault: true }),
    config('PluginB', { isDefault: true }),
];

describe('NUMA module availability', () => {
    it.each([
        [{ hasNumaData: false }, ['Timeline', 'PluginA', 'PluginB']],
        [{ hasNumaData: true, isFullDb: false }, ['Timeline', 'PluginA', 'PluginB', 'NUMA']],
    ])('filters and orders modules for session %o', (session, expected) => {
        expect(getAvailableModules(modules, 'Default', session).map(item => item.name)).toEqual(expected);
    });

    it.each([[{ hasNumaData: true }, true], [{}, false]])(
        'publishes NUMA availability from import data', (result, hasNumaData) => {
            updateDataScene(result);
            expect(updateSession).toHaveBeenCalledWith(expect.objectContaining({ hasNumaData }));
        },
    );
});


describe('independent memory entry availability', () => {
    it.each([
        ['leaks', 'D:/data/scope.db', 'MemScope'],
        ['leaks', 'D:/data/snapshot.PICKLE', 'MemSnapshot'],
        ['memsnapshot', 'D:/data/scope.db', 'MemScope'],
        ['memsnapshot', '', 'MemSnapshot'],
    ])('selects %s / %s as %s without mounting the other application', (module, file, expected) => {
        const entries = getAvailableModules(modulesConfig, 'Leaks', {}, module, file);
        expect(entries.filter(config => config.isLeaks && config.name !== 'Timeline').map(config => config.name)).toEqual([expected]);
    });
    it('does not show memory entries for ordinary profiling data', () => {
        expect(getAvailableModules(modulesConfig, 'Default', {}).some(config => config.name === 'MemScope' || config.name === 'MemSnapshot')).toBe(false);
    });
});


it('mounts, activates and initializes the matching registered entry on dataset switching', async () => {
    const makeSession = (selectedFilePath: string): Session => ({
        scene: 'Leaks', isLeaks: true, isBinary: false, isCluster: false,
        actionListener: {}, activeDataSource: { selectedFilePath, selectedFileType: 'file', projectName: 'fixture' },
    } as unknown as Session);
    const view = render(React.createElement(TabPane, { session: makeSession('/fixture/scope.db') }));
    const scope = view.container.querySelector('iframe[name="MemScope"]');
    expect(scope).not.toBeNull();
    expect(view.container.querySelector('iframe[name="MemSnapshot"]')).toBeNull();
    fireEvent.load(scope!);
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'frame/loaded', to: 'MemScope' }));
    view.rerender(React.createElement(TabPane, { session: makeSession('/fixture/snapshot.pickle') }));
    const snapshot = view.container.querySelector('iframe[name="MemSnapshot"]');
    expect(snapshot).not.toBeNull();
    expect(scope?.isConnected).toBe(false);
    expect(snapshot?.getAttribute('src')).toBe('./plugins/MemSnapshot/index.html');
    fireEvent.load(snapshot!);
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'frame/loaded', to: 'MemSnapshot' }));
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'moduleActive', to: 'MemSnapshot' }));
    await act(async () => { await Promise.resolve(); });
});


it('mounts the first snapshot before isLeaks becomes available', async () => {
    const session = {
        scene: 'Default', isLeaks: false, isBinary: false, isCluster: false, actionListener: {},
        activeDataSource: { selectedFilePath: '/first.pickle', selectedFileType: 'file', projectName: 'first' },
    } as unknown as Session;
    const view = render(React.createElement(TabPane, { session }));
    expect(view.container.querySelector('iframe[name="MemSnapshot"]')).not.toBeNull();
    expect(view.container.querySelector('iframe[name="MemScope"]')).toBeNull();
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'moduleActive', to: 'MemSnapshot' }));
    await act(async () => { await Promise.resolve(); });
});

it.each([
    ['/scope-a.db', '/scope-b.db', 'MemScope'],
    ['/snapshot-a.pickle', '/snapshot-b.pickle', 'MemSnapshot'],
])('keeps %s -> %s on %s despite a Timeline activation request', async (first, second, application) => {
    const makeSession = (file: string, actionListener: object = {}): Session => ({
        scene: 'Leaks', isLeaks: true, isBinary: false, isCluster: false,
        actionListener, activeDataSource: { selectedFilePath: file, selectedFileType: 'file', projectName: file },
    } as unknown as Session);
    const view = render(React.createElement(TabPane, { session: makeSession(first) }));
    expect((view.container.querySelector(`iframe[name="${application}"]`) as HTMLIFrameElement).style.display).toBe('block');
    (connector.send as jest.Mock).mockClear();
    view.rerender(React.createElement(TabPane, { session: makeSession(second, { type: 'switch active module', value: 'Timeline' }) }));
    expect((view.container.querySelector(`iframe[name="${application}"]`) as HTMLIFrameElement).style.display).toBe('block');
    expect((view.container.querySelector('iframe[name="Timeline"]') as HTMLIFrameElement).style.display).toBe('none');
    expect(connector.send).not.toHaveBeenCalledWith(expect.objectContaining({ event: 'moduleActive', to: 'Timeline' }));
    await act(async () => { await Promise.resolve(); });
});

it('allows Timeline again after switching memory data to ordinary profiling', async () => {
    const makeSession = (memory: boolean): Session => ({
        scene: memory ? 'Leaks' : 'Default', isLeaks: memory, isBinary: false, isCluster: false,
        actionListener: { type: 'switch active module', value: 'Timeline' },
        activeDataSource: { selectedFilePath: memory ? '/scope.db' : '/profiling', selectedFileType: 'file', projectName: 'fixture' },
    } as unknown as Session);
    const view = render(React.createElement(TabPane, { session: makeSession(true) }));
    view.rerender(React.createElement(TabPane, { session: makeSession(false) }));
    expect(view.container.querySelector('iframe[name="MemScope"]')).toBeNull();
    expect((view.container.querySelector('iframe[name="Timeline"]') as HTMLIFrameElement).style.display).toBe('block');
    await act(async () => { await Promise.resolve(); });
});

it('keeps MemScope visible after returning from Snapshot and receiving a late Timeline activation', async () => {
    const makeSession = (selectedFilePath: string, actionListener: object = {}): Session => ({
        scene: 'Leaks', isLeaks: true, isBinary: false, isCluster: false, actionListener,
        activeDataSource: { selectedFilePath, selectedFileType: 'file', projectName: 'fixture' },
    } as unknown as Session);
    const view = render(React.createElement(TabPane, { session: makeSession('/snapshot.pickle') }));
    const snapshot = view.container.querySelector('iframe[name="MemSnapshot"]');
    view.rerender(React.createElement(TabPane, { session: makeSession('/scope.db') }));
    expect(snapshot?.isConnected).toBe(false);
    (connector.send as jest.Mock).mockClear();
    view.rerender(React.createElement(TabPane, { session: makeSession('/scope.db', { type: 'switch active module', value: 'Timeline' }) }));
    const scope = view.container.querySelector('iframe[name="MemScope"]') as HTMLIFrameElement;
    expect(scope.style.display).toBe('block');
    expect((view.container.querySelector('iframe[name="Timeline"]') as HTMLIFrameElement).style.display).toBe('none');
    expect(connector.send).not.toHaveBeenCalledWith(expect.objectContaining({ event: 'moduleActive', to: 'Timeline' }));
    fireEvent.load(scope);
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'frame/loaded', to: 'MemScope', body: expect.objectContaining({ selectedFilePath: '/scope.db' }) }));
    await act(async () => { await Promise.resolve(); });
});

describe('other module impact audit', () => {
    const sessionFor = (scene: string, value: string, hasNumaData = false): Session => ({
        scene, isLeaks: false, isBinary: scene === 'Compute', isCluster: scene === 'Cluster', isIE: scene === 'IE',
        hasNumaData, actionListener: { type: 'switch active module', value },
        activeDataSource: { selectedFilePath: '/profiling', selectedFileType: 'file', projectName: 'fixture' },
    } as unknown as Session);
    it.each([
        ['Default', 'Memory'], ['Default', 'Operator'], ['Default', 'Timeline'],
        ['Cluster', 'Communication'], ['Cluster', 'Summary'], ['Cluster', 'Timeline'],
        ['Compute', 'Source'], ['Compute', 'Details'], ['IE', 'Statistic'],
    ])('keeps %s -> %s navigation available', async (scene, target) => {
        const view = render(React.createElement(TabPane, { session: sessionFor(scene, target) }));
        const frame = view.container.querySelector(`iframe[name="${target}"]`) as HTMLIFrameElement;
        expect(frame).not.toBeNull();
        expect(frame.style.display).toBe('block');
        expect(view.container.querySelector('iframe[name="MemScope"]')).toBeNull();
        expect(view.container.querySelector('iframe[name="MemSnapshot"]')).toBeNull();
        await act(async () => { await Promise.resolve(); });
    });
    it('does not replay old navigation after the user selects another tab and NUMA becomes available', async () => {
        const first = sessionFor('Default', 'Memory');
        const view = render(React.createElement(TabPane, { session: first }));
        fireEvent.click(view.getByRole('menuitem', { name: /^Operator$/ }));
        expect((view.container.querySelector('iframe[name="Operator"]') as HTMLIFrameElement).style.display).toBe('block');
        const next = sessionFor('Default', 'Memory', true);
        next.actionListener = first.actionListener;
        view.rerender(React.createElement(TabPane, { session: next }));
        expect((view.container.querySelector('iframe[name="Operator"]') as HTMLIFrameElement).style.display).toBe('block');
        await act(async () => { await Promise.resolve(); });
    });
    it('reinitializes a reloaded ordinary iframe with current file data and removes its listener on unmount', async () => {
        const first = sessionFor('Default', 'Timeline');
        const view = render(React.createElement(TabPane, { session: first }));
        const frame = view.container.querySelector('iframe[name="Timeline"]') as HTMLIFrameElement;
        const next = sessionFor('Default', 'Timeline');
        next.activeDataSource.selectedFilePath = '/another-profile';
        view.rerender(React.createElement(TabPane, { session: next }));
        (connector.send as jest.Mock).mockClear();
        fireEvent.load(frame);
        expect(connector.send).toHaveBeenCalledTimes(1);
        expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ to: 'Timeline', event: 'frame/loaded', body: expect.objectContaining({ selectedFilePath: '/another-profile' }) }));
        view.unmount();
        (connector.send as jest.Mock).mockClear();
        fireEvent.load(frame);
        expect(connector.send).not.toHaveBeenCalled();
        await act(async () => { await Promise.resolve(); });
    });
});

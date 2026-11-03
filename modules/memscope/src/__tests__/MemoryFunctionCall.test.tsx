/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 */
import React, { forwardRef as mockForwardRef } from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import { message } from 'antd';
import MemoryStack from '../components/MemoryStack';
import { createSession as createFeatureSession, type MemScopeSession as Session } from '../application/session';

jest.mock('@insight/lib/utils', () => ({ safeStr: (value: string) => value }), { virtual: true });
jest.mock('@insight/lib/components', () => ({
    MIChart: mockForwardRef(() => null),
    Select: () => null,
    Checkbox: ({ checked, onChange }: any) => <input aria-label="trim" type="checkbox" checked={checked} onChange={onChange} />,
    CollapsiblePanel: ({ children }: { children: React.ReactNode }) => <div>{children}</div>,
}), { virtual: true });
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('../components/MemorySliceChart', () => () => null);
jest.mock('../common/components/MemoryDataZoom', () => () => null);
jest.mock('../../../memsnapshot/src/components/MemSnapshotSliceOverview', () => () => null);
jest.mock('../../../memsnapshot/src/components/PotentialLeakStats', () => () => null);
jest.mock('../common/components/leaks/MemoryBlockDiagram', () => ({ MemoryBlockDiagram: () => null }));
jest.mock('../../../memsnapshot/src/components/MemoryStateDiagram', () => ({ MemoryStateDiagram: () => null }));
jest.mock('../loaders/data', () => ({
    ...jest.requireActual('../loaders/data'), getBarNewData: jest.fn(),
}));
jest.mock('../../../memsnapshot/src/loaders/data', () => ({
    getBarNewData: jest.fn(), getPotentialLeakStats: jest.fn(), preloadSnapshotSliceOverviews: jest.fn(),
}));
jest.mock('../../../memsnapshot/src/rendering/stateWorker/worker', () => { const api = { workerSelectItem: jest.fn() }; return { ...api, getStateRenderer: (session: any) => ({ selectItem: (...args: any[]) => api.workerSelectItem(...args, session) }) }; }, { virtual: true });
jest.mock('@emotion/react', () => ({ useTheme: () => ({ mode: 'light' }) }));
jest.mock('../common/utils/utils', () => ({ chartResize: jest.fn() }));
jest.mock('../common/components/leaks/tools', () => ({
    Loading: ({ loading }: { loading: boolean }) => loading ? <div role="status">Loading</div> : null,
    MarkLineStack: () => null,
}));
jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('../common/leaksWorker/blockWorker/worker', () => { const api = { workerSetBlockGraphGlobalMaxSize: jest.fn() }; return { ...api, getBlockRenderer: (session: any) => ({ setBlockGraphGlobalMaxSize: (...args: any[]) => api.workerSetBlockGraphGlobalMaxSize(...args, session) }) }; });
jest.mock('../../../memsnapshot/src/components/opfsFallback', () => ({}));

const pending: Array<{ resolve: (value: any) => void; reject: (error: Error) => void }> = [];
const traceData = (func: string): any => ({
    traces: [{ func, depth: 0, startTimestamp: 10, endTimestamp: 20 }],
    minTimestamp: 10,
    maxTimestamp: 20,
    maxDepth: 0,
});
const createSession = (): Session => {
    const session = createFeatureSession();
    runInAction(() => {
        session.module = 'leaks';
        session.deviceIds = { 1: ['PTA'] };
        session.deviceId = '1';
        session.eventType = 'PTA';
        session.threadId = 2621226;
        session.threadIds = [2621226];
    });
    return session;
};
beforeEach(() => {
    pending.length = 0;
    jest.clearAllMocks();
    window.request = jest.fn().mockImplementation(() => new Promise((resolve, reject) => {
        pending.push({ resolve, reject });
    }));
});

describe('MemScope initial trace loading', () => {
    it.each(['resolve', 'reject'])('replaces an invalidated initial request and ignores its late %s', async outcome => {
        const session = createSession();
        const view = render(<MemoryStack session={session} />);
        expect(pending).toHaveLength(1);
        // Parsing publishes a new device map even when device and thread selection stay the same.
        act(() => { runInAction(() => { session.deviceIds = { 1: ['PTA'] }; }); });
        expect(pending).toHaveLength(2);
        await act(async () => {
            if (outcome === 'resolve') pending[0].resolve(traceData('stale'));
            else pending[0].reject(new Error('stale failure'));
        });
        expect(view.queryByRole('status')).not.toBeNull();
        expect(session.funcData.traces).toEqual([]);
        expect(message.error).not.toHaveBeenCalled();
        await act(async () => { pending[1].resolve(traceData('current')); });
        expect(session.funcData.traces[0].func).toBe('current');
        expect(session.funcOptions).toEqual([{ label: 'current', value: 'current' }]);
        expect(view.queryByRole('status')).toBeNull();
    });

    it('reloads on project identity changes with the same selections and finishes a failed current request', async () => {
        const session = createSession();
        const view = render(<MemoryStack session={session} />);
        act(() => { runInAction(() => { session.fileHash = 'next-project'; }); });
        expect(pending).toHaveLength(2);
        await act(async () => { pending[1].reject(new Error('current failure')); });
        expect(view.queryByRole('status')).toBeNull();
        expect(message.error).toHaveBeenCalledWith('current failure');
        await act(async () => { pending[0].resolve(traceData('old-project')); });
        expect(session.funcData.traces).toEqual([]);
    });

    it('does not request traces in snapshot mode and loads when returning to MemScope', async () => {
        const session = createSession();
        runInAction(() => { session.module = 'memsnapshot'; });
        render(<MemoryStack session={session} />);
        expect(pending).toHaveLength(0);
        act(() => { runInAction(() => { session.module = 'leaks'; }); });
        expect(pending).toHaveLength(1);
        await act(async () => { pending[0].resolve(traceData('memscope')); });
        expect(session.loadingFunc).toBe(false);
    });

    it('replaces zoom work invalidated by a device map refresh without leaving loading active', async () => {
        const session = createSession();
        const view = render(<MemoryStack session={session} />);
        await act(async () => { pending[0].resolve(traceData('initial')); });
        fireEvent.click(view.getByRole('checkbox', { name: 'trim' }));
        expect(pending).toHaveLength(2);
        act(() => { runInAction(() => { session.deviceIds = { 1: ['PTA'] }; }); });
        expect(pending).toHaveLength(3);
        await act(async () => { pending[2].resolve(traceData('current')); });
        await act(async () => { pending[1].resolve(traceData('cancelled-range')); });
        expect(session.funcData.traces[0].func).toBe('current');
        expect(view.queryByRole('status')).toBeNull();
    });
});

jest.mock('../../../memsnapshot/src/common/components/MemoryDataZoom', () => jest.requireMock('../common/components/MemoryDataZoom'));

jest.mock('../../../memsnapshot/src/common/components/leaks/MemoryBlockDiagram', () => jest.requireMock('../common/components/leaks/MemoryBlockDiagram'));

jest.mock('../../../memsnapshot/src/common/utils/utils', () => jest.requireMock('../common/utils/utils'));

jest.mock('../../../memsnapshot/src/common/components/leaks/tools', () => jest.requireMock('../common/components/leaks/tools'));

jest.mock('../../../memsnapshot/src/common/leaksWorker/blockWorker/worker', () => jest.requireMock('../common/leaksWorker/blockWorker/worker'));

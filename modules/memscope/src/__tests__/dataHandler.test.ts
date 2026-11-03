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
import { message } from 'antd';
import * as scope from '../loaders/data';
import * as snapshot from '../../../memsnapshot/src/loaders/data';
import { workerLoadMemoryBlockCache, workerSetMemoryBlockData } from '../../../memsnapshot/src/common/leaksWorker/blockWorker/worker';
import { createMemoryBlockContextKey } from '../../../memsnapshot/src/common/components/blockLoadState';
const { getFuncNewData, getNewDetailData } = scope;
// Exercise each app's loader directly using the same dataset choice as the host.
const getBarNewData = (session: any): Promise<void> => (session.module === 'leaks' ? scope : snapshot).getBarNewData(session);
const getBlockTableData = (session: any): Promise<void> => (session.module === 'leaks' ? scope : snapshot).getBlockTableData(session);
const getEventTableData = (session: any): Promise<void> => (session.module === 'leaks' ? scope : snapshot).getEventTableData(session);

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('../../../memsnapshot/src/common/leaksWorker/blockWorker/worker', () => {
    const api = {
        workerLoadMemoryBlockCache: jest.fn().mockResolvedValue('miss'),
        workerSetMemoryBlockData: jest.fn().mockResolvedValue(undefined),
        workerSetAllocationLines: jest.fn(),
        workerDestroy: jest.fn(),
        workerTransform: jest.fn(),
    }; return { ...api, getBlockRenderer: (session: any) => ({ loadMemoryBlockCache: (...args: any[]) => api.workerLoadMemoryBlockCache(...args, session), setMemoryBlockData: (...args: any[]) => api.workerSetMemoryBlockData(...args, session), setAllocationLines: (...args: any[]) => api.workerSetAllocationLines(...args, session), destroy: (...args: any[]) => api.workerDestroy(...args, session), transform: (...args: any[]) => api.workerTransform(...args, session) }) };
}, { virtual: true });
jest.mock('../../../memsnapshot/src/components/opfsFallback', () => ({
    ensureOpfsOrWaitForFallbackApproval: jest.fn().mockResolvedValue(undefined),
    ensureOpfsFallbackApproval: jest.fn().mockResolvedValue(undefined),
}));

const createSession = (module: string, deviceId: string): any => ({
    module,
    deviceId,
    fileHash: 'project-a',
    eventType: 'MALLOC',
    threadId: '1',
    searchFunc: [],
    allowTrim: true,
    minTime: 0,
    maxTime: 100,
    memoryStamp: 50,
    selectedSliceIndex: 0,
    snapshotSlices: { 0: { readySlices: [0], slices: [{ index: 0, ready: true }] } },
    sliceOverviewData: {},
    snapshotParsingComplete: false,
    loadedMemoryBlockContextKey: '',
    leaksWorkerInfo: { renderOptions: {} },
    blocksCurrentPage: 1,
    blocksPageSize: 10,
    blocksOrder: '',
    blocksFilters: {},
    blocksRangeFilters: {},
    eventsCurrentPage: 1,
    eventsPageSize: 10,
    eventsOrder: '',
    eventsFilters: {},
    eventsRangeFilters: {},
    lazyUsedThreshold: { valueT: null, perT: null },
    delayedFreeThreshold: { valueT: null, perT: null },
    longIdleThreshold: { valueT: null, perT: null },
});

beforeEach(() => {
    jest.clearAllMocks();
    window.request = jest.fn().mockImplementation(async ({ command }) => ({
        traces: [],
        allocations: [],
        blocks: [],
        events: [],
        headers: [],
        total: /blocks|events/.test(command) ? 0 : undefined,
        minTimestamp: 0,
        maxTimestamp: 100,
        maxDepth: 0,
    }));
});

describe('memory project switching', () => {
    it.each([
        ['leaks', 'blocks', 'resolve'], ['leaks', 'blocks', 'reject'],
        ['leaks', 'events', 'resolve'], ['leaks', 'events', 'reject'],
        ['memsnapshot', 'blocks', 'resolve'], ['memsnapshot', 'blocks', 'reject'],
        ['memsnapshot', 'events', 'resolve'], ['memsnapshot', 'events', 'reject'],
    ])('ignores a stale %s %s table %s after switching features', async (module, table, outcome) => {
        let resolveRequest: (value: unknown) => void = () => undefined;
        let rejectRequest: (reason: Error) => void = () => undefined;
        window.request = jest.fn().mockReturnValue(new Promise((resolve, reject) => {
            resolveRequest = resolve;
            rejectRequest = reject;
        }));
        const session = createSession(module, '0');
        session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        const pending = table === 'blocks' ? getBlockTableData(session) : getEventTableData(session);
        expect(window.request).toHaveBeenCalledTimes(1);
        session.module = module === 'leaks' ? 'memsnapshot' : 'leaks';
        session[`${table}TableData`] = [{ id: 'current-project' }];
        if (outcome === 'resolve') {
            resolveRequest({ [table]: [{ id: 'stale' }], headers: [], total: 1 });
        } else {
            rejectRequest(new Error('Previous feature failed'));
        }
        await pending;
        expect(session[`${table}TableData`]).toEqual([{ id: 'current-project' }]);
        expect(message.error).not.toHaveBeenCalled();
    });
    it.each(['leaks', 'memsnapshot'])('does not send empty-device requests during %s reset', async module => {
        const session = createSession(module, '');
        await getFuncNewData(session, 0, 100);
        await getBarNewData(session);
        await getNewDetailData(session);
        await getBlockTableData(session);
        await getEventTableData(session);
        expect(window.request).not.toHaveBeenCalled();
        expect(message.error).not.toHaveBeenCalled();
    });

    it('does not request memscope-only data for snapshot zoom callbacks', async () => {
        const session = createSession('memsnapshot', '0');
        await getFuncNewData(session, 0, 100);
        await getNewDetailData(session);
        expect(window.request).not.toHaveBeenCalled();
    });

    it.each(['leaks', 'memsnapshot'])('loads device zero through the correct %s endpoints', async module => {
        const session = createSession(module, '0');
        await getBarNewData(session);
        await getBlockTableData(session);
        await getEventTableData(session);
        const prefix = module === 'leaks' ? 'Memory/leaks' : 'Memory/snapshot';
        for (const suffix of ['allocations', 'blocks', 'events']) {
            expect(window.request).toHaveBeenCalledWith(expect.objectContaining({
                command: `${prefix}/${suffix}`, params: expect.objectContaining({ deviceId: '0' }),
            }));
        }
    });

    it('resumes memscope traces when the device becomes ready', async () => {
        const session = createSession('leaks', '');
        await getFuncNewData(session);
        session.deviceId = '0';
        await getFuncNewData(session);
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(session.funcData.maxTimestamp).toBe(100);
    });

    it('does not start the block request if the project changes during cache lookup', async () => {
        let resolveCache: (value: string) => void = () => undefined;
        (workerLoadMemoryBlockCache as jest.Mock).mockReturnValueOnce(new Promise(resolve => { resolveCache = resolve; }));
        const session = createSession('memsnapshot', '0');
        const pending = getBarNewData(session);
        await Promise.resolve();
        session.deviceId = '';
        session.module = 'leaks';
        resolveCache('miss');
        await pending;
        expect(window.request).not.toHaveBeenCalled();
        expect(workerSetMemoryBlockData).not.toHaveBeenCalled();
    });

    it.each(['resolve', 'reject'])('ignores a stale trace %s after the project changes', async outcome => {
        let resolveRequest: (value: unknown) => void = () => undefined;
        let rejectRequest: (reason: Error) => void = () => undefined;
        window.request = jest.fn().mockReturnValue(new Promise((resolve, reject) => {
            resolveRequest = resolve;
            rejectRequest = reject;
        }));
        const session = createSession('leaks', '0');
        const pending = getFuncNewData(session);
        session.fileHash = 'project-b';
        if (outcome === 'resolve') {
            resolveRequest({ traces: [], minTimestamp: 5, maxTimestamp: 10 });
        } else {
            rejectRequest(new Error('Previous project request failed'));
        }
        await pending;
        expect(session.funcData).toBeUndefined();
        expect(message.error).not.toHaveBeenCalled();
    });
});

jest.mock('../common/leaksWorker/blockWorker/worker', () => jest.requireMock('../../../memsnapshot/src/common/leaksWorker/blockWorker/worker'));

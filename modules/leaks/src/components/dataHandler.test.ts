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
import { getBarNewData, getBlockTableData, getEventTableData, getFuncNewData, getNewDetailData } from './dataHandler';
import { workerLoadMemoryBlockCache, workerSetMemoryBlockData } from '@/leaksWorker/blockWorker/worker';
import { createMemoryBlockContextKey } from './blockLoadState';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerLoadMemoryBlockCache: jest.fn().mockResolvedValue('miss'),
    workerSetMemoryBlockData: jest.fn().mockResolvedValue(undefined),
    workerSetAllocationLines: jest.fn(),
    workerDestroy: jest.fn(),
    workerTransform: jest.fn(),
}), { virtual: true });
jest.mock('./opfsFallback', () => ({
    ensureOpfsOrWaitForFallbackApproval: jest.fn().mockResolvedValue(undefined),
    ensureOpfsFallbackApproval: jest.fn().mockResolvedValue(undefined),
}));

const createSession = (module: string, deviceId: string): any => ({
    module,
    deviceId,
    deviceIds: deviceId === '' ? {} : { [deviceId]: ['MALLOC'] },
    fileHash: 'project-a',
    eventType: 'MALLOC',
    threadId: '1',
    searchFunc: [],
    allowTrim: true,
    minTime: 0,
    maxTime: 100,
    memoryStamp: 50,
    selectedSliceIndex: 0,
    snapshotSlices: { [deviceId]: { readySlices: [0], slices: [{ index: 0, ready: true }] } },
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

const createReadyTableSession = (module = 'memsnapshot'): any => {
    const session = createSession(module, '0');
    session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
    session.blocksTableData = [{ id: -1 }];
    session.eventsTableData = [{ id: -2 }];
    session.blocksTableHeader = [{ key: 'old-block', name: 'Old block' }];
    session.eventsTableHeader = [{ key: 'old-event', name: 'Old event' }];
    session.blocksTotal = 7;
    session.eventsTotal = 8;
    return session;
};

const tableState = (session: any): unknown => ({
    blocks: session.blocksTableData,
    events: session.eventsTableData,
    blocksHeader: session.blocksTableHeader,
    eventsHeader: session.eventsTableHeader,
    blocksTotal: session.blocksTotal,
    eventsTotal: session.eventsTotal,
    blocksPage: session.blocksCurrentPage,
    eventsPage: session.eventsCurrentPage,
});

const tableResponse = () => ({
    headers: [{ key: 'id', name: 'ID' }],
    blocks: [{ id: 11 }],
    events: [{ id: 22 }],
    total: 1,
});

const deferred = () => {
    let resolve: (value: unknown) => void = () => undefined;
    let reject: (reason: Error) => void = () => undefined;
    const promise = new Promise<unknown>((resolvePromise, rejectPromise) => {
        resolve = resolvePromise;
        reject = rejectPromise;
    });
    return { promise, resolve, reject };
};

beforeEach(() => {
    jest.clearAllMocks();
    (workerLoadMemoryBlockCache as jest.Mock).mockReset().mockResolvedValue('miss');
    (workerSetMemoryBlockData as jest.Mock).mockReset().mockResolvedValue(undefined);
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

// TARGET project-switching and graph scheduling intent is retained.
describe('memory project switching', () => {
    it.each(['leaks', 'memsnapshot'])('does not send empty-device requests during %s reset', async module => {
        const session = createSession(module, '');
        await getFuncNewData(session, 0, 100);
        await getBarNewData(session);
        await getNewDetailData(session);
        expect(await getBlockTableData(session)).toBeUndefined();
        expect(await getEventTableData(session)).toBeUndefined();
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
        const request = deferred();
        window.request = jest.fn().mockReturnValue(request.promise);
        const session = createSession('leaks', '0');
        const pending = getFuncNewData(session);
        session.fileHash = 'project-b';
        if (outcome === 'resolve') {
            request.resolve({ traces: [], minTimestamp: 5, maxTimestamp: 10 });
        } else {
            request.reject(new Error('Previous project request failed'));
        }
        await pending;
        expect(session.funcData).toBeUndefined();
        expect(message.error).not.toHaveBeenCalled();
    });

    it('invalidates traces before an unready-snapshot early return even after restoring the context', async () => {
        const request = deferred();
        window.request = jest.fn().mockReturnValue(request.promise);
        const session = createSession('leaks', '0');
        const pending = getFuncNewData(session);
        session.module = 'memsnapshot';
        session.snapshotSlices[0].slices[0].ready = false;
        await getFuncNewData(session);
        session.module = 'leaks';
        session.snapshotSlices[0].slices[0].ready = true;
        request.resolve({ traces: [], minTimestamp: 5, maxTimestamp: 10 });
        await pending;
        expect(session.funcData).toBeUndefined();
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(message.error).not.toHaveBeenCalled();
    });
});

// SOURCE query-building intent is retained with TARGET-ready snapshot fixtures.
describe('MemScope table data handlers', () => {
    test('builds the Snapshot block request from the current query state', async () => {
        const session = createReadyTableSession();
        session.minTime = 10;
        session.blocksCurrentPage = 2;
        session.blocksPageSize = 30;
        session.blocksOrder = true;
        session.blocksOrderBy = 'size';
        session.blocksFilters = { name: 'tensor' };
        session.blocksRangeFilters = { size: [10, 20] };
        await getBlockTableData(session);
        expect(window.request).toHaveBeenCalledWith({
            command: 'Memory/snapshot/blocks',
            params: expect.objectContaining({
                deviceId: '0',
                currentPage: 2,
                pageSize: 30,
                desc: true,
                orderBy: 'size',
                filters: { name: 'tensor' },
                rangeFilters: { size: [10, 20] },
                isTable: true,
                startTimestamp: 10,
                endTimestamp: 100,
                sliceIndex: 0,
            }),
        });
    });

    test('uses the Leaks event endpoint and preserves ascending sort', async () => {
        const session = createReadyTableSession('leaks');
        session.eventsCurrentPage = 3;
        session.eventsPageSize = 20;
        session.eventsOrder = false;
        session.eventsOrderBy = 'timestamp';
        session.eventsFilters = { event: 'alloc' };
        await getEventTableData(session);
        expect(window.request).toHaveBeenCalledWith({
            command: 'Memory/leaks/events',
            params: expect.objectContaining({
                currentPage: 3,
                pageSize: 20,
                desc: false,
                orderBy: 'timestamp',
                filters: { event: 'alloc' },
                isTable: true,
            }),
        });
    });

    test('does not query snapshot events until the current graph is loaded', async () => {
        const session = createReadyTableSession();
        session.loadedMemoryBlockContextKey = '';
        const before = tableState(session);
        expect(await getEventTableData(session)).toBeUndefined();
        expect(window.request).not.toHaveBeenCalled();
        expect(tableState(session)).toEqual(before);
        expect(message.error).not.toHaveBeenCalled();
    });
});

describe.each([
    { kind: 'blocks', fetchData: getBlockTableData },
    { kind: 'events', fetchData: getEventTableData },
])('$kind table return and invalidation contract', ({ kind, fetchData }) => {
    test('returns rows, headers and total without committing session data or correcting a page', async () => {
        const session = createReadyTableSession();
        session.blocksCurrentPage = 9;
        session.eventsCurrentPage = 9;
        const before = tableState(session);
        const response = tableResponse();
        (window.request as jest.Mock).mockResolvedValue(response);
        expect(await fetchData(session)).toBe(response);
        expect(tableState(session)).toEqual(before);
        expect(window.request).toHaveBeenCalledWith(expect.objectContaining({
            command: `Memory/snapshot/${kind}`,
            params: expect.objectContaining({ deviceId: '0', sliceIndex: 0, currentPage: 9 }),
        }));
    });

    test('preserves a successful empty table instead of returning unavailable', async () => {
        const session = createReadyTableSession();
        const before = tableState(session);
        const response = { headers: [], blocks: [], events: [], total: 0 };
        (window.request as jest.Mock).mockResolvedValue(response);
        expect(await fetchData(session)).toBe(response);
        expect(tableState(session)).toEqual(before);
    });

    test('returns undefined without requesting an unready snapshot slice', async () => {
        const session = createReadyTableSession();
        session.snapshotSlices[0].slices[0].ready = false;
        const before = tableState(session);
        expect(await fetchData(session)).toBeUndefined();
        expect(window.request).not.toHaveBeenCalled();
        expect(tableState(session)).toEqual(before);
        expect(message.error).not.toHaveBeenCalled();
    });

    test.each(['resolve', 'reject'])('rechecks slice readiness before accepting a late %s', async outcome => {
        const session = createReadyTableSession();
        const before = tableState(session);
        const request = deferred();
        window.request = jest.fn().mockReturnValue(request.promise);
        const pending = fetchData(session);
        session.snapshotSlices[0].slices[0].ready = false;
        if (outcome === 'resolve') request.resolve(tableResponse());
        else request.reject(new Error('No longer ready'));
        expect(await pending).toBeUndefined();
        expect(tableState(session)).toEqual(before);
        expect(message.error).not.toHaveBeenCalled();
    });

    test('resumes normally after the slice and graph become ready', async () => {
        const session = createReadyTableSession();
        session.snapshotSlices[0].slices[0].ready = false;
        session.loadedMemoryBlockContextKey = '';
        expect(await fetchData(session)).toBeUndefined();
        session.snapshotSlices[0].slices[0].ready = true;
        session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        const response = tableResponse();
        (window.request as jest.Mock).mockResolvedValue(response);
        expect(await fetchData(session)).toBe(response);
        expect(window.request).toHaveBeenCalledTimes(1);
    });

    test('toasts and rethrows a current request error without changing table state', async () => {
        const session = createReadyTableSession();
        const before = tableState(session);
        const error = new Error('Current table request failed');
        (window.request as jest.Mock).mockRejectedValue(error);
        await expect(fetchData(session)).rejects.toBe(error);
        expect(message.error).toHaveBeenCalledWith(error.message);
        expect(tableState(session)).toEqual(before);
    });

    test.each(['fileHash', 'deviceIds', 'deviceId', 'selectedSliceIndex', 'eventType', 'module'])(
        'ignores late resolve and reject after %s changes without a new request', async field => {
            for (const outcome of ['resolve', 'reject']) {
                const session = createReadyTableSession();
                const before = tableState(session);
                const request = deferred();
                window.request = jest.fn().mockReturnValue(request.promise);
                const pending = fetchData(session);
                if (field === 'fileHash') session.fileHash = 'project-b';
                if (field === 'deviceIds') session.deviceIds = { 0: ['MALLOC'] };
                if (field === 'deviceId') session.deviceId = '1';
                if (field === 'selectedSliceIndex') session.selectedSliceIndex = 1;
                if (field === 'eventType') session.eventType = 'FREE';
                if (field === 'module') session.module = 'leaks';
                if (outcome === 'resolve') request.resolve(tableResponse());
                else request.reject(new Error('Stale table request failed'));
                expect(await pending).toBeUndefined();
                expect(tableState(session)).toEqual(before);
                expect(message.error).not.toHaveBeenCalled();
            }
        },
    );

    test.each(['device', 'slice'])(
        'invalidates a pending request before the %s early return even when the same context is restored', async reason => {
            for (const outcome of ['resolve', 'reject']) {
                const session = createReadyTableSession();
                const before = tableState(session);
                const request = deferred();
                window.request = jest.fn().mockReturnValue(request.promise);
                const pending = fetchData(session);
                if (reason === 'device') session.deviceId = '';
                else session.snapshotSlices[0].slices[0].ready = false;
                expect(await fetchData(session)).toBeUndefined();
                session.deviceId = '0';
                session.snapshotSlices[0].slices[0].ready = true;
                if (outcome === 'resolve') request.resolve(tableResponse());
                else request.reject(new Error('Superseded table request failed'));
                expect(await pending).toBeUndefined();
                expect(window.request).toHaveBeenCalledTimes(1);
                expect(tableState(session)).toEqual(before);
                expect(message.error).not.toHaveBeenCalled();
            }
        },
    );

    test('returns only the newer overlapping response', async () => {
        const session = createReadyTableSession();
        const first = deferred();
        const second = deferred();
        window.request = jest.fn().mockReturnValueOnce(first.promise).mockReturnValueOnce(second.promise);
        const oldRequest = fetchData(session);
        const newRequest = fetchData(session);
        const response = tableResponse();
        second.resolve(response);
        expect(await newRequest).toBe(response);
        first.resolve(tableResponse());
        expect(await oldRequest).toBeUndefined();
    });
});

it.each(['resolve', 'reject'])('rechecks graph readiness before accepting a late event %s', async outcome => {
    const session = createReadyTableSession();
    const before = tableState(session);
    const request = deferred();
    window.request = jest.fn().mockReturnValue(request.promise);
    const pending = getEventTableData(session);
    session.loadedMemoryBlockContextKey = '';
    if (outcome === 'resolve') request.resolve(tableResponse());
    else request.reject(new Error('Graph is no longer ready'));
    expect(await pending).toBeUndefined();
    expect(tableState(session)).toEqual(before);
    expect(message.error).not.toHaveBeenCalled();
});

it.each(['resolve', 'reject'])('invalidates events before graph-unready return and ignores a late %s after recovery', async outcome => {
    const session = createReadyTableSession();
    const before = tableState(session);
    const request = deferred();
    window.request = jest.fn().mockReturnValue(request.promise);
    const pending = getEventTableData(session);
    session.loadedMemoryBlockContextKey = '';
    expect(await getEventTableData(session)).toBeUndefined();
    session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
    if (outcome === 'resolve') request.resolve(tableResponse());
    else request.reject(new Error('Old graph request failed'));
    expect(await pending).toBeUndefined();
    expect(window.request).toHaveBeenCalledTimes(1);
    expect(tableState(session)).toEqual(before);
    expect(message.error).not.toHaveBeenCalled();
});

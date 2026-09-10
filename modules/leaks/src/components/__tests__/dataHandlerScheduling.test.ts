/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { getBarNewData, preloadSnapshotSliceOverviews } from '../dataHandler';
import {
    getBlocksGraphData,
    getLeaksAllocationsData,
    getSnapshotAllocationLines,
    getSnapshotAllocations,
    getSnapshotBlocks,
} from '../../utils/RequestUtils';
import {
    workerLoadMemoryBlockCache,
    workerSetAllocationLines,
    workerSetMemoryBlockData,
    workerDestroy,
} from '@/leaksWorker/blockWorker/worker';
import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from '../blockLoadState';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../../utils/RequestUtils', () => ({
    getBlocksGraphData: jest.fn(),
    getLeaksAllocationsData: jest.fn(),
    getSnapshotAllocations: jest.fn(),
    getSnapshotAllocationLines: jest.fn(),
    getSnapshotBlocks: jest.fn(),
}));
jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerLoadMemoryBlockCache: jest.fn(),
    workerSetMemoryBlockData: jest.fn(),
    workerSetAllocationLines: jest.fn(),
    workerTransform: jest.fn(),
    workerDestroy: jest.fn(),
}), { virtual: true });
jest.mock('../opfsFallback', () => ({
    ensureOpfsOrWaitForFallbackApproval: jest.fn(),
    ensureOpfsFallbackApproval: jest.fn(),
}));

const mockedGetBlocksGraphData = getBlocksGraphData as jest.MockedFunction<typeof getBlocksGraphData>;
const mockedGetLeaksAllocationsData = getLeaksAllocationsData as jest.MockedFunction<typeof getLeaksAllocationsData>;
const mockedGetSnapshotBlocks = getSnapshotBlocks as jest.MockedFunction<typeof getSnapshotBlocks>;
const mockedGetSnapshotAllocations = getSnapshotAllocations as jest.MockedFunction<typeof getSnapshotAllocations>;
const mockedGetSnapshotAllocationLines = getSnapshotAllocationLines as jest.MockedFunction<
    typeof getSnapshotAllocationLines
>;
const mockedLoadCache = workerLoadMemoryBlockCache as jest.MockedFunction<typeof workerLoadMemoryBlockCache>;
const mockedSetMemoryBlockData = workerSetMemoryBlockData as jest.MockedFunction<typeof workerSetMemoryBlockData>;
const mockedSetAllocationLines = workerSetAllocationLines as jest.MockedFunction<typeof workerSetAllocationLines>;
const mockedWorkerDestroy = workerDestroy as jest.MockedFunction<typeof workerDestroy>;

const createSession = (): any => ({
    module: 'memsnapshot',
    deviceId: '0',
    eventType: 'BLOCK',
    fileHash: 'a'.repeat(64),
    snapshotParsingComplete: false,
    memSnapshotCacheRefreshPending: false,
    selectedSliceIndex: 0,
    snapshotSlices: {
        0: {
            eventCount: 100,
            sliceCount: 1,
            readySlices: [0],
            slices: [{ index: 0, startEventId: 0, endEventId: 99, ready: true }],
        },
    },
    sliceOverviewData: {},
    loadedMemoryBlockContextKey: '',
    leaksWorkerInfo: { renderOptions: {} },
});

const createLeaksSession = (): any => ({
    module: 'leaks',
    deviceId: '0',
    eventType: 'HOST',
    fileHash: '',
    snapshotParsingComplete: true,
    memSnapshotCacheRefreshPending: false,
    selectedSliceIndex: -1,
    snapshotSlices: {},
    sliceOverviewData: {},
    loadedMemoryBlockContextKey: '',
    leaksWorkerInfo: { renderOptions: {} },
    allocationData: { allocations: [] },
});

describe('memory block data request scheduling', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        mockedSetMemoryBlockData.mockResolvedValue(undefined);
        mockedGetSnapshotAllocations.mockResolvedValue({ allocations: [] } as any);
        mockedGetSnapshotAllocationLines.mockResolvedValue({ reservedLine: [] } as any);
        mockedGetLeaksAllocationsData.mockResolvedValue({ allocations: [] } as any);
        mockedGetBlocksGraphData.mockResolvedValue({ blocks: [] } as any);
    });

    it('loads allocations and a cache miss block response in parallel', async () => {
        let resolveBlocks: (data: any) => void = () => undefined;
        mockedLoadCache.mockResolvedValue('miss');
        mockedGetSnapshotBlocks.mockImplementation(() => new Promise(resolve => {
            resolveBlocks = resolve;
        }));

        const session = createSession();
        const loading = getBarNewData(session);
        await new Promise(resolve => setTimeout(resolve, 0));
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocations).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocationLines).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledWith(expect.objectContaining({
            sliceIndex: 0,
            currentPage: 1,
            pageSize: 1000,
        }));
        expect(mockedGetSnapshotAllocations).toHaveBeenCalledWith(expect.objectContaining({
            sliceIndex: 0,
            currentPage: 1,
            pageSize: 30000,
        }));
        expect(mockedGetSnapshotAllocationLines).toHaveBeenCalledWith(expect.objectContaining({
            sliceIndex: 0,
            currentPage: 1,
            pageSize: 30000,
        }));

        resolveBlocks({ blocks: [] });
        await loading;

        expect(mockedSetMemoryBlockData).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocations).toHaveBeenCalledTimes(1);
        expect(session.loadedMemoryBlockContextKey).toBe(createMemoryBlockContextKey(session));
        expect(isMemoryBlockLoadReady(session)).toBe(true);
    });

    it('requests allocations after a cache hit without requesting blocks', async () => {
        mockedLoadCache.mockResolvedValue('hit');
        const reservedLine = [{ timestamp: 1, reservedSize: 10 }];
        const processUsedLine = [{ timestamp: 1, processUsed: 20 }];
        const deviceUsedLine = [{ timestamp: 1, deviceUsed: 30 }];
        mockedGetSnapshotAllocations.mockResolvedValue({ allocations: [] } as any);
        mockedGetSnapshotAllocationLines.mockResolvedValue({
            reservedLine,
            processUsedLine,
            deviceUsedLine,
        } as any);

        const session = createSession();
        await getBarNewData(session);

        expect(mockedGetSnapshotBlocks).not.toHaveBeenCalled();
        expect(mockedGetSnapshotAllocations).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocationLines).toHaveBeenCalledTimes(1);
        expect(mockedSetAllocationLines).toHaveBeenCalledWith({
            reservedLine,
            processUsedLine,
            deviceUsedLine,
        });
        expect(session.allocationData.allocationLineAvailability).toEqual({
            reservedLine: true,
            processUsedLine: true,
            deviceUsedLine: true,
        });
        expect(isMemoryBlockLoadReady(session)).toBe(true);
    });

    it('reuses a prefetched slice overview and requests only allocation lines when switching slices', async () => {
        mockedLoadCache.mockResolvedValue('hit');
        const session = createSession();
        session.snapshotParsingComplete = true;
        session.sliceOverviewData = {
            0: {
                0: {
                    minTimestamp: 0,
                    maxTimestamp: 99,
                    allocations: [{ timestamp: 1, totalSize: 10 }],
                },
            },
        };

        await getBarNewData(session);

        expect(mockedGetSnapshotAllocations).not.toHaveBeenCalled();
        expect(mockedGetSnapshotAllocationLines).toHaveBeenCalledTimes(1);
        expect(session.allocationData.allocations).toEqual([{ timestamp: 1, totalSize: 10 }]);
    });

    it('hides the block loading mask as soon as a cached graph is rendered', async () => {
        let resolveAllocations: (data: any) => void = () => undefined;
        mockedLoadCache.mockResolvedValue('hit');
        mockedGetSnapshotAllocations.mockImplementation(() => new Promise(resolve => {
            resolveAllocations = resolve;
        }));
        const session = createSession();

        const loading = getBarNewData(session);
        await new Promise(resolve => setTimeout(resolve, 0));

        expect(mockedGetSnapshotAllocations).toHaveBeenCalledTimes(1);
        expect(session.loadingBlocks).toBe(false);
        expect(isMemoryBlockLoadReady(session)).toBe(true);

        resolveAllocations({ allocations: [] });
        await loading;
    });

    it('preloads non-selected slice overviews in reverse order', async () => {
        const session = createSession();
        let activeRequests = 0;
        let maxActiveRequests = 0;
        session.selectedSliceIndex = -1;
        session.deviceIds = { 0: ['BLOCK'] };
        session.snapshotGlobalMaxSizes = {};
        session.snapshotSlices[0] = {
            eventCount: 400,
            sliceCount: 4,
            readySlices: [0, 1, 2, 3],
            slices: [],
        };
        mockedGetSnapshotAllocations.mockImplementation(async () => {
            activeRequests++;
            maxActiveRequests = Math.max(maxActiveRequests, activeRequests);
            await new Promise(resolve => setTimeout(resolve, 20));
            activeRequests--;
            return { allocations: [], reservedLine: [] } as any;
        });

        await preloadSnapshotSliceOverviews(session);

        expect(mockedGetSnapshotAllocations.mock.calls.map(([param]) => param.sliceIndex)).toEqual([2, 1, 0]);
        expect(maxActiveRequests).toBeGreaterThanOrEqual(1);
        expect(maxActiveRequests).toBeLessThanOrEqual(2);
    });

    it('commits the global maximum once after overview preloading completes', async () => {
        const session = createSession();
        session.selectedSliceIndex = -1;
        session.deviceIds = { 0: ['BLOCK'] };
        session.snapshotSlices[0] = {
            eventCount: 300,
            sliceCount: 3,
            readySlices: [0, 1, 2],
            slices: [],
        };
        let globalMaxSizes = {};
        let updateCount = 0;
        Object.defineProperty(session, 'snapshotGlobalMaxSizes', {
            configurable: true,
            get: () => globalMaxSizes,
            set: value => {
                updateCount++;
                globalMaxSizes = value;
            },
        });
        mockedGetSnapshotAllocations.mockImplementation(async param => ({
            allocations: [{ timestamp: param.sliceIndex ?? 0, totalSize: ((param.sliceIndex ?? 0) + 1) * 10 }],
        } as any));

        await preloadSnapshotSliceOverviews(session);

        expect(updateCount).toBe(1);
        expect(globalMaxSizes).toEqual({ 0: { BLOCK: 20 } });
    });

    it('renders a finalized snapshot cache before preloading the remaining overviews', async () => {
        const session = createSession();
        session.snapshotParsingComplete = true;
        session.deviceIds = { 0: ['BLOCK'] };
        session.snapshotGlobalMaxSizes = {};
        session.snapshotSlices[0] = {
            eventCount: 200,
            sliceCount: 2,
            readySlices: [0, 1],
            slices: [
                { index: 0, startEventId: 0, endEventId: 99, ready: true },
                { index: 1, startEventId: 100, endEventId: 199, ready: true },
            ],
        };
        const requestOrder: string[] = [];
        mockedGetSnapshotAllocations.mockImplementation(async param => {
            requestOrder.push(`allocation-${param.sliceIndex}`);
            return {
                allocations: [{ timestamp: param.sliceIndex ?? 0, totalSize: (param.sliceIndex ?? 0) + 1 }],
                reservedLine: [],
            } as any;
        });
        mockedGetSnapshotAllocationLines.mockImplementation(async param => {
            requestOrder.push(`lines-${param.sliceIndex}`);
            return { reservedLine: [] } as any;
        });
        mockedLoadCache.mockImplementation(async () => {
            requestOrder.push('cache');
            return 'hit';
        });

        await getBarNewData(session);
        await preloadSnapshotSliceOverviews(session);

        expect(requestOrder).toEqual(['cache', 'allocation-0', 'lines-0', 'allocation-1']);
        expect(session.snapshotGlobalMaxSizes[0].BLOCK).toBe(2);
    });

    it('bypasses a stale persistent cache until finalization cleanup succeeds', async () => {
        const session = createSession();
        session.snapshotParsingComplete = true;
        session.memSnapshotCacheRefreshPending = true;
        mockedLoadCache.mockResolvedValue('miss');
        mockedGetSnapshotBlocks.mockResolvedValue({ blocks: [] } as any);

        await getBarNewData(session);

        expect(mockedLoadCache).toHaveBeenCalledWith({ fileHash: '' });
        expect(mockedSetMemoryBlockData).toHaveBeenCalledWith({ data: { blocks: [] }, fileHash: '' });
    });

    it('does not preload blocks for unselected slices after rendering the selected slice', async () => {
        const session = createSession();
        session.snapshotParsingComplete = true;
        session.deviceIds = { 0: ['BLOCK'] };
        session.snapshotSlices[0] = {
            eventCount: 200,
            sliceCount: 2,
            readySlices: [0, 1],
            slices: [
                { index: 0, startEventId: 0, endEventId: 99, ready: true },
                { index: 1, startEventId: 100, endEventId: 199, ready: true },
            ],
        };
        mockedLoadCache.mockResolvedValue('miss');
        mockedGetSnapshotBlocks.mockResolvedValue({ blocks: [] } as any);

        await getBarNewData(session);
        await new Promise(resolve => setTimeout(resolve, 0));

        expect(mockedGetSnapshotBlocks).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledWith(expect.objectContaining({ sliceIndex: 0 }));
    });

    it('does not mark block data ready before rendering completes', async () => {
        let resolveRender: () => void = () => undefined;
        mockedLoadCache.mockResolvedValue('miss');
        mockedGetSnapshotBlocks.mockResolvedValue({ blocks: [] } as any);
        mockedSetMemoryBlockData.mockImplementation(() => new Promise(resolve => {
            resolveRender = resolve;
        }));
        const session = createSession();

        const loading = getBarNewData(session);
        await new Promise(resolve => setTimeout(resolve, 0));

        expect(isMemoryBlockLoadReady(session)).toBe(false);
        resolveRender();
        await loading;
        expect(isMemoryBlockLoadReady(session)).toBe(true);
    });

    it('reads memscope polylines from leaks/allocations instead of snapshot allocationLines', async () => {
        const reservedLine = [{ timestamp: 1, reservedSize: 10 }];
        const processUsedLine = [{ timestamp: 1, processUsed: 20 }];
        const deviceUsedLine = [{ timestamp: 1, deviceUsed: 30 }];
        mockedGetLeaksAllocationsData.mockResolvedValue({
            allocations: [{ timestamp: 1, totalSize: 40 }],
            reservedLine,
            processUsedLine,
            deviceUsedLine,
        } as any);

        const session = createLeaksSession();
        await getBarNewData(session);

        expect(mockedGetLeaksAllocationsData).toHaveBeenCalledTimes(1);
        expect(mockedGetBlocksGraphData).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocationLines).not.toHaveBeenCalled();
        expect(mockedGetSnapshotAllocations).not.toHaveBeenCalled();
        expect(mockedLoadCache).not.toHaveBeenCalled();
        expect(mockedSetAllocationLines).toHaveBeenCalledWith({
            reservedLine,
            processUsedLine,
            deviceUsedLine,
        });
        expect(session.allocationData.allocationLineAvailability).toEqual({
            reservedLine: true,
            processUsedLine: true,
            deviceUsedLine: true,
        });
        expect(mockedSetMemoryBlockData.mock.invocationCallOrder[0])
            .toBeLessThan(mockedSetAllocationLines.mock.invocationCallOrder[0]);
    });

    it('holds memscope polylines until block data raises the worker generation', async () => {
        const reservedLine = [{ timestamp: 2, reservedSize: 8 }];
        let resolveBlocks: (data: any) => void = () => undefined;
        mockedGetLeaksAllocationsData.mockResolvedValue({
            allocations: [],
            reservedLine,
            processUsedLine: [],
            deviceUsedLine: [],
        } as any);
        mockedGetBlocksGraphData.mockImplementation(() => new Promise(resolve => {
            resolveBlocks = resolve;
        }));

        const loading = getBarNewData(createLeaksSession());
        await new Promise(resolve => setTimeout(resolve, 0));

        expect(mockedGetLeaksAllocationsData).toHaveBeenCalledTimes(1);
        expect(mockedSetAllocationLines).not.toHaveBeenCalled();
        expect(mockedSetMemoryBlockData).not.toHaveBeenCalled();

        resolveBlocks({ blocks: [] });
        await loading;

        expect(mockedSetMemoryBlockData).toHaveBeenCalledTimes(1);
        expect(mockedSetAllocationLines).toHaveBeenCalledWith({
            reservedLine,
            processUsedLine: [],
            deviceUsedLine: [],
        });
        expect(mockedSetMemoryBlockData.mock.invocationCallOrder[0])
            .toBeLessThan(mockedSetAllocationLines.mock.invocationCallOrder[0]);
    });

    it('cancels the previous render and keeps an unready slice empty', async () => {
        const session = createSession();
        session.selectedSliceIndex = 1;
        session.snapshotSlices[0] = {
            eventCount: 200,
            sliceCount: 2,
            readySlices: [0],
            slices: [
                { index: 0, startEventId: 0, endEventId: 99, ready: true },
                { index: 1, startEventId: 100, endEventId: 199, ready: false },
            ],
        };

        await getBarNewData(session);

        expect(mockedWorkerDestroy).toHaveBeenCalledTimes(1);
        expect(mockedLoadCache).not.toHaveBeenCalled();
        expect(mockedGetSnapshotBlocks).not.toHaveBeenCalled();
        expect(session.blockData.blocks).toEqual([]);
        expect(session.allocationData.allocations).toEqual([]);
    });
});

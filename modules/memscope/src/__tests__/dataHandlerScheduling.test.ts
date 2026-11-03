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

import { getBarNewData } from '../loaders/data';
import { getBlocksGraphData, getLeaksAllocationsData } from '../api/requests';
import { getSnapshotAllocationLines, getSnapshotAllocations } from '../../../memsnapshot/src/api/requests';
import {
    workerLoadMemoryBlockCache,
    workerSetAllocationLines,
    workerSetMemoryBlockData,
} from '../common/leaksWorker/blockWorker/worker';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../api/requests', () => ({
    getBlocksGraphData: jest.fn(),
    getLeaksAllocationsData: jest.fn(),
}));
jest.mock('../../../memsnapshot/src/api/requests', () => ({
    getSnapshotAllocations: jest.fn(),
    getSnapshotAllocationLines: jest.fn(),
    getSnapshotBlocks: jest.fn(),
}));
jest.mock('../common/leaksWorker/blockWorker/worker', () => {
    const api = {
        workerLoadMemoryBlockCache: jest.fn(),
        workerSetMemoryBlockData: jest.fn(),
        workerSetAllocationLines: jest.fn(),
        workerTransform: jest.fn(),
        workerDestroy: jest.fn(),
    }; return { ...api, getBlockRenderer: (session: any) => ({ loadMemoryBlockCache: (...args: any[]) => api.workerLoadMemoryBlockCache(...args, session), setMemoryBlockData: (...args: any[]) => api.workerSetMemoryBlockData(...args, session), setAllocationLines: (...args: any[]) => api.workerSetAllocationLines(...args, session), transform: (...args: any[]) => api.workerTransform(...args, session), destroy: (...args: any[]) => api.workerDestroy(...args, session) }) };
});
jest.mock('../../../memsnapshot/src/components/opfsFallback', () => ({
    ensureOpfsOrWaitForFallbackApproval: jest.fn(),
    ensureOpfsFallbackApproval: jest.fn(),
}));

const mockedGetBlocksGraphData = getBlocksGraphData as jest.MockedFunction<typeof getBlocksGraphData>;
const mockedGetLeaksAllocationsData = getLeaksAllocationsData as jest.MockedFunction<typeof getLeaksAllocationsData>;
const mockedGetSnapshotAllocations = getSnapshotAllocations as jest.MockedFunction<typeof getSnapshotAllocations>;
const mockedGetSnapshotAllocationLines = getSnapshotAllocationLines as jest.MockedFunction<
    typeof getSnapshotAllocationLines
>;
const mockedLoadCache = workerLoadMemoryBlockCache as jest.MockedFunction<typeof workerLoadMemoryBlockCache>;
const mockedSetMemoryBlockData = workerSetMemoryBlockData as jest.MockedFunction<typeof workerSetMemoryBlockData>;
const mockedSetAllocationLines = workerSetAllocationLines as jest.MockedFunction<typeof workerSetAllocationLines>;

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
        }, expect.any(Object));
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
        }, expect.any(Object));
        expect(mockedSetMemoryBlockData.mock.invocationCallOrder[0])
            .toBeLessThan(mockedSetAllocationLines.mock.invocationCallOrder[0]);
    });
});

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

import { getBarNewData } from '../dataHandler';
import { getSnapshotAllocations, getSnapshotBlocks } from '../../utils/RequestUtils';
import {
    workerLoadMemoryBlockCache,
    workerSetAllocationLines,
    workerSetMemoryBlockData,
} from '@/leaksWorker/blockWorker/worker';
import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from '../blockLoadState';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../../utils/RequestUtils', () => ({
    getSnapshotAllocations: jest.fn(),
    getSnapshotBlocks: jest.fn(),
}));
jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerLoadMemoryBlockCache: jest.fn(),
    workerSetMemoryBlockData: jest.fn(),
    workerSetAllocationLines: jest.fn(),
    workerTransform: jest.fn(),
}), { virtual: true });
jest.mock('../opfsFallback', () => ({
    ensureOpfsOrWaitForFallbackApproval: jest.fn(),
    ensureOpfsFallbackApproval: jest.fn(),
}));

const mockedGetSnapshotBlocks = getSnapshotBlocks as jest.MockedFunction<typeof getSnapshotBlocks>;
const mockedGetSnapshotAllocations = getSnapshotAllocations as jest.MockedFunction<typeof getSnapshotAllocations>;
const mockedLoadCache = workerLoadMemoryBlockCache as jest.MockedFunction<typeof workerLoadMemoryBlockCache>;
const mockedSetMemoryBlockData = workerSetMemoryBlockData as jest.MockedFunction<typeof workerSetMemoryBlockData>;
const mockedSetAllocationLines = workerSetAllocationLines as jest.MockedFunction<typeof workerSetAllocationLines>;

const createSession = (): any => ({
    module: 'memsnapshot',
    deviceId: '0',
    eventType: 'BLOCK',
    fileHash: 'a'.repeat(64),
    loadedMemoryBlockContextKey: '',
    leaksWorkerInfo: { renderOptions: {} },
});

describe('memory block data request scheduling', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        mockedSetMemoryBlockData.mockResolvedValue(undefined);
        mockedGetSnapshotAllocations.mockResolvedValue({ allocations: [], reservedLine: [] } as any);
    });

    it('starts the allocation request after a cache miss block response', async () => {
        let resolveBlocks: (data: any) => void = () => undefined;
        mockedLoadCache.mockResolvedValue('miss');
        mockedGetSnapshotBlocks.mockImplementation(() => new Promise(resolve => {
            resolveBlocks = resolve;
        }));

        const session = createSession();
        const loading = getBarNewData(session);
        await new Promise(resolve => setTimeout(resolve, 0));
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotAllocations).not.toHaveBeenCalled();

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
        mockedGetSnapshotAllocations.mockResolvedValue({
            allocations: [],
            reservedLine,
            processUsedLine,
            deviceUsedLine,
        } as any);

        const session = createSession();
        await getBarNewData(session);

        expect(mockedGetSnapshotBlocks).not.toHaveBeenCalled();
        expect(mockedGetSnapshotAllocations).toHaveBeenCalledTimes(1);
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
});

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

import { getBarNewData } from '../dataHandler';
import { getSnapshotAllocations, getSnapshotBlocks } from '../../utils/RequestUtils';
import {
    workerLoadMemoryBlockCache,
    workerSetMemoryBlockData,
} from '@/leaksWorker/blockWorker/worker';
import { ensureOpfsFallbackApproval } from '../opfsFallback';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../../utils/RequestUtils', () => ({
    getSnapshotAllocations: jest.fn(),
    getSnapshotBlocks: jest.fn(),
}));
jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerLoadMemoryBlockCache: jest.fn(),
    workerSetMemoryBlockData: jest.fn(),
    workerSetReservedLine: jest.fn(),
    workerTransform: jest.fn(),
}), { virtual: true });
jest.mock('../opfsFallback', () => ({
    ensureOpfsFallbackApproval: jest.fn(),
    ensureOpfsOrWaitForFallbackApproval: jest.fn(),
}));

const mockedGetSnapshotBlocks = getSnapshotBlocks as jest.MockedFunction<typeof getSnapshotBlocks>;
const mockedGetSnapshotAllocations = getSnapshotAllocations as jest.MockedFunction<typeof getSnapshotAllocations>;
const mockedLoadCache = workerLoadMemoryBlockCache as jest.MockedFunction<typeof workerLoadMemoryBlockCache>;
const mockedSetMemoryBlockData = workerSetMemoryBlockData as jest.MockedFunction<typeof workerSetMemoryBlockData>;
const mockedEnsureFallback = ensureOpfsFallbackApproval as jest.MockedFunction<typeof ensureOpfsFallbackApproval>;

const createSession = (): any => ({
    module: 'memsnapshot',
    deviceId: '0',
    eventType: 'BLOCK',
    fileHash: 'a'.repeat(64),
    leaksWorkerInfo: { renderOptions: {} },
});

const createAllocationData = (): any => ({ allocations: [], reservedLine: [] });

describe('memory block OPFS fallback', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        mockedSetMemoryBlockData.mockResolvedValue(undefined);
        mockedGetSnapshotAllocations.mockResolvedValue(createAllocationData());
        mockedEnsureFallback.mockResolvedValue(undefined);
    });

    it('waits for fallback approval when OPFS becomes unavailable during cache loading', async () => {
        let approveFallback: () => void = () => undefined;
        mockedLoadCache.mockResolvedValue('unavailable');
        mockedEnsureFallback.mockImplementation(() => new Promise(resolve => {
            approveFallback = resolve;
        }));
        mockedGetSnapshotBlocks.mockResolvedValue({ blocks: [] } as any);

        const loading = getBarNewData(createSession());
        await new Promise(resolve => setTimeout(resolve, 0));
        expect(mockedEnsureFallback).toHaveBeenCalledTimes(1);
        expect(mockedGetSnapshotBlocks).not.toHaveBeenCalled();

        approveFallback();
        await loading;
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledTimes(1);
    });

    it('uses the temporary-cache path without approval after a transient cache failure', async () => {
        mockedLoadCache.mockResolvedValue('transient');
        mockedGetSnapshotBlocks.mockResolvedValue({ blocks: [] } as any);

        await getBarNewData(createSession());

        expect(mockedEnsureFallback).not.toHaveBeenCalled();
        expect(mockedGetSnapshotBlocks).toHaveBeenCalledTimes(1);
        expect(mockedSetMemoryBlockData).toHaveBeenCalledTimes(1);
    });
});

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

import { getBarNewData } from '../loaders/data';
import { getSnapshotAllocations, getSnapshotBlocks } from '../api/requests';
import {
    workerLoadMemoryBlockCache,
    workerSetMemoryBlockData,
} from '../common/leaksWorker/blockWorker/worker';
import { ensureOpfsFallbackApproval } from '../components/opfsFallback';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../api/requests', () => ({
    getSnapshotAllocations: jest.fn(),
    getSnapshotBlocks: jest.fn(),
}));
jest.mock('../common/leaksWorker/blockWorker/worker', () => {
    const api = {
        workerLoadMemoryBlockCache: jest.fn(),
        workerSetMemoryBlockData: jest.fn(),
        workerSetAllocationLines: jest.fn(),
        workerTransform: jest.fn(),
        workerDestroy: jest.fn(),
        isWorkerBlockRenderer: true,
        workerCheckOpfsAvailability: jest.fn(),
    }; return { ...api, getBlockRenderer: (session: any) => ({ loadMemoryBlockCache: (...args: any[]) => api.workerLoadMemoryBlockCache(...args, session), setMemoryBlockData: (...args: any[]) => api.workerSetMemoryBlockData(...args, session), setAllocationLines: (...args: any[]) => api.workerSetAllocationLines(...args, session), transform: (...args: any[]) => api.workerTransform(...args, session), destroy: (...args: any[]) => api.workerDestroy(...args, session), checkOpfsAvailability: (...args: any[]) => api.workerCheckOpfsAvailability(...args, session) }) };
}, { virtual: true });
jest.mock('../components/opfsFallback', () => ({
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

jest.mock('../../../memscope/src/common/leaksWorker/blockWorker/worker', () => jest.requireMock('../common/leaksWorker/blockWorker/worker'));

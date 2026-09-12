/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { getPotentialLeakStats } from '../dataHandler';
import { getSnapshotLeakStats } from '../../utils/RequestUtils';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('mobx', () => ({ runInAction: (action: () => void) => action() }));
jest.mock('../../utils/RequestUtils', () => ({
    getSnapshotLeakStats: jest.fn(),
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

const mockedGetSnapshotLeakStats = getSnapshotLeakStats as jest.MockedFunction<typeof getSnapshotLeakStats>;

const createSession = (): any => ({
    module: 'memsnapshot',
    deviceId: '0',
    minTime: 10,
    maxTime: 20,
    selectedSliceIndex: -1,
    leakStats: {
        totalSize: 0,
        maxSize: 0,
        minSize: 0,
        loading: false,
        error: false,
        requestId: 0,
    },
    snapshotSlices: {},
});

describe('getPotentialLeakStats', () => {
    beforeEach(() => {
        mockedGetSnapshotLeakStats.mockReset();
        mockedGetSnapshotLeakStats.mockResolvedValue({ totalSize: 1, maxSize: 1, minSize: 1 });
    });

    it('does not query leak stats before a snapshot slice is ready', async () => {
        await getPotentialLeakStats(createSession());
        expect(mockedGetSnapshotLeakStats).not.toHaveBeenCalled();
    });

    it('queries leak stats after the selected slice becomes ready', async () => {
        const session = createSession();
        session.selectedSliceIndex = 0;
        session.snapshotSlices = {
            0: {
                slices: [{ ready: true }],
            },
        };

        await getPotentialLeakStats(session);

        expect(mockedGetSnapshotLeakStats).toHaveBeenCalledWith({
            deviceId: '0',
            startTimestamp: 10,
            endTimestamp: 20,
            sliceIndex: 0,
        });
        expect(session.leakStats.totalSize).toBe(1);
        expect(session.leakStats.loading).toBe(false);
        expect(session.leakStats.error).toBe(false);
    });
});

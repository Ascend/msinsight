/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { store } from '@/store';
import { updateSession } from '@/connection/notificationHandler';
import {
    parseLeaksSuccessHandler,
    parseMemSnapshotProgressHandler,
    parseMemSnapshotSliceReadyHandler,
} from './interceptorHandler';

jest.mock('@insight/lib/utils', () => ({
    getIndexByRankNameAndDeviceId: jest.fn(),
    getRankInfoKey: jest.fn((rankInfo: { rankId?: string }) => rankInfo.rankId ?? ''),
}), { virtual: true });

jest.mock('@/store', () => {
    const activeSession = {
        activeDataSource: {
            selectedFilePath: 'C:\\data\\snapshot.pickle',
            projectPath: ['C:\\data\\snapshot.pickle'],
        },
        toBeActivedProject: undefined as { selectedFilePath?: string; projectPath?: string[] } | undefined,
        memSnapshotParseFileId: '',
        memSnapshotParseLoading: false,
        memSnapshotParseProgress: 0,
        fileHash: '',
        snapshotSlices: {} as Record<string, any>,
        snapshotParsingComplete: true,
        deviceIds: {} as Record<string, unknown>,
    };
    return {
        store: {
            sessionStore: {
                activeSession,
            },
        },
    };
}, { virtual: true });

jest.mock('@/connection/notificationHandler', () => ({
    updateSession: jest.fn(),
}), { virtual: true });

const mockedUpdateSession = updateSession as jest.MockedFunction<typeof updateSession>;

const completeSlices = {
    0: {
        sliceCount: 1,
        slices: [{ index: 0, startEventId: 0, endEventId: 99, ready: true }],
        readySlices: [0],
    },
};

describe('framework memsnapshot interceptors', () => {
    beforeEach(() => {
        const session = store.sessionStore.activeSession;
        session.activeDataSource.selectedFilePath = 'C:\\data\\snapshot.pickle';
        session.activeDataSource.projectPath = ['C:\\data\\snapshot.pickle'];
        session.toBeActivedProject = undefined;
        session.memSnapshotParseFileId = '';
        session.memSnapshotParseLoading = false;
        session.memSnapshotParseProgress = 0;
        session.fileHash = '';
        session.snapshotSlices = {};
        session.snapshotParsingComplete = true;
        session.deviceIds = {};
        mockedUpdateSession.mockClear();
        mockedUpdateSession.mockImplementation((data: Record<string, unknown>) => {
            Object.assign(session, data);
            return data;
        });
    });

    it('ignores completion events from another data source', () => {
        parseLeaksSuccessHandler({
            module: 'memsnapshot',
            dbPath: 'D:\\other.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            snapshotParsingComplete: true,
            snapshotSlices: completeSlices,
        } as any);

        expect(mockedUpdateSession).not.toHaveBeenCalled();
    });

    it('accepts a cache-hit completion while a folder import is still pending', () => {
        store.sessionStore.activeSession.toBeActivedProject = {
            projectName: 'data',
            selectedFilePath: 'C:\\data',
            projectPath: ['C:\\data'],
            children: [],
        };

        parseLeaksSuccessHandler({
            module: 'memsnapshot',
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: completeSlices,
        } as any);

        expect(store.sessionStore.activeSession.memSnapshotParseLoading).toBe(false);
        expect(store.sessionStore.activeSession.memSnapshotParseProgress).toBe(100);
        expect(store.sessionStore.activeSession.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
    });

    it('ignores stale progress from a previous snapshot file', () => {
        parseMemSnapshotProgressHandler({ fileId: 'D:\\old.pickle', progress: 80 });

        expect(mockedUpdateSession).not.toHaveBeenCalled();
        expect(store.sessionStore.activeSession.memSnapshotParseProgress).toBe(0);
    });

    it('keeps overall parsing visible until every slice is ready', () => {
        parseMemSnapshotProgressHandler({ fileId: 'C:\\data\\snapshot.pickle', progress: 40 });
        expect(store.sessionStore.activeSession.memSnapshotParseLoading).toBe(true);
        expect(store.sessionStore.activeSession.memSnapshotParseProgress).toBe(40);

        const session = store.sessionStore.activeSession;
        session.fileHash = 'hash';
        session.snapshotSlices = {
            0: {
                eventCount: 200,
                sliceCount: 2,
                readySlices: [],
                slices: [
                    { index: 0, startEventId: 0, endEventId: 99, ready: false },
                    { index: 1, startEventId: 100, endEventId: 199, ready: false },
                ],
            },
        };
        parseMemSnapshotSliceReadyHandler({
            fileId: 'C:\\data\\snapshot.pickle',
            fileHash: 'hash',
            deviceId: '0',
            slice: { index: 0, startEventId: 0, endEventId: 99, ready: true },
        });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.snapshotSlices[0].readySlices).toEqual([0]);
    });
});

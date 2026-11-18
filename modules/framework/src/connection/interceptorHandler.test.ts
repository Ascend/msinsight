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
    parseFailHandler,
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
        memSnapshotSourcePaths: [] as string[],
        memSnapshotParseFileId: '',
        memSnapshotParseLoading: false,
        memSnapshotParseProgress: 0,
        memSnapshotParseFailure: null as { fileId: string; error: string } | null,
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
        session.memSnapshotSourcePaths = [];
        session.memSnapshotParseFileId = '';
        session.memSnapshotParseLoading = false;
        session.memSnapshotParseProgress = 0;
        session.memSnapshotParseFailure = null;
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

    it('caches snapshot failures and prevents later progress from restarting loading', () => {
        parseFailHandler({ rankId: 'C:\\data\\snapshot.pickle', error: 'Python spawn failed' });
        parseMemSnapshotProgressHandler({ fileId: 'C:\\data\\snapshot.pickle', progress: 5 });
        expect(store.sessionStore.activeSession.memSnapshotParseLoading).toBe(false);
        expect(store.sessionStore.activeSession.memSnapshotParseFailure).toEqual({
            fileId: 'C:\\data\\snapshot.pickle', error: 'Python spawn failed',
        });
    });

    it('does not cache a snapshot failure belonging to another data source', () => {
        parseFailHandler({ rankId: 'D:\\other.pickle', error: 'old failure' });
        expect(mockedUpdateSession).not.toHaveBeenCalled();
    });

    it('prioritizes a pending ADD_FILE target over the previous snapshot', () => {
        const session = store.sessionStore.activeSession;
        session.memSnapshotSourcePaths = ['/new/snapshot.pkl'];
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        parseFailHandler({ rankId: 'C:\\data\\snapshot.pickle', error: 'old failure' });
        expect(mockedUpdateSession).not.toHaveBeenCalled();
        parseFailHandler({ rankId: '/new/snapshot.pkl', error: 'new failure' });
        expect(session.memSnapshotParseFailure?.fileId).toBe('/new/snapshot.pkl');
    });

    it.each([undefined, '', '   '])('uses a nonempty error for %p', error => {
        parseFailHandler({ rankId: 'C:\\data\\snapshot.pickle', error });
        expect(store.sessionStore.activeSession.memSnapshotParseFailure?.error).toBe('Failed to parse snapshot data.');
    });

    it('does not block another file in the same imported folder after a snapshot failure', () => {
        const session = store.sessionStore.activeSession;
        session.activeDataSource.projectPath = ['C:\\data'];
        parseFailHandler({ rankId: 'C:\\data\\snapshot.pickle', error: 'Python spawn failed' });
        parseMemSnapshotProgressHandler({ fileId: 'C:\\data\\another.pkl', progress: 10 });
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\another.pkl');
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

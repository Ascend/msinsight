/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { Session } from '../entity/session';
import { store } from '../store';
import {
    importRemoteHandler,
    parseCompletedHandler,
    parseProgressHandler,
    parseSliceReadyHandler,
    switchDirectoryHandler,
    updateSessionHandler,
    hasMultipleMemSnapshotSlices,
} from './handler';

jest.mock('@insight/lib/i18n', () => ({
    __esModule: true,
    default: { changeLanguage: jest.fn() },
}), { virtual: true });
jest.mock('@insight/lib', () => ({
    errorCenter: { handleError: jest.fn() },
    ErrorCode: { PARSE_FAIL: 'PARSE_FAIL' },
    WsError: class extends Error {},
}), { virtual: true });
jest.mock('@/leaksWorker/blockWorker/worker', () => ({ workerDestroy: jest.fn() }), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({ workerDestroy: jest.fn() }), { virtual: true });
jest.mock('@/entity/session', () => ({
    LEAKS_WORKER_INFO_DEFAULT: {},
    MARK_LINE_POSITION_DEFAULT: {},
    STATE_WORKER_INFO_DEFAULT: {},
}), { virtual: true });

const incompleteSlices = {
    0: {
        eventCount: 300,
        sliceCount: 3,
        readySlices: [1, 2],
        slices: [
            { index: 0, startEventId: 0, endEventId: 99, ready: false },
            { index: 1, startEventId: 100, endEventId: 199, ready: true },
            { index: 2, startEventId: 200, endEventId: 299, ready: true },
        ],
    },
};

const completeSlices = {
    0: {
        eventCount: 300,
        sliceCount: 3,
        readySlices: [0, 1, 2],
        slices: [
            { index: 0, startEventId: 0, endEventId: 99, ready: true },
            { index: 1, startEventId: 100, endEventId: 199, ready: true },
            { index: 2, startEventId: 200, endEventId: 299, ready: true },
        ],
    },
};

describe('memsnapshot parse progress handlers', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        store.sessionStore.activeSession = new Session();
    });

    it('keeps showing progress after the first slice becomes available', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 37;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotSlices: incompleteSlices,
        });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(37);
        expect(session.selectedSliceIndex).toBe(2);

        updateSessionHandler({ deviceIds: { 0: ['BLOCK'] } });
        expect(session.memSnapshotParseLoading).toBe(true);
    });

    it('does not hide 100 percent progress before every slice is ready', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;

        parseProgressHandler({ fileId: 'C:\\data\\snapshot.pickle', progress: 100 });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(100);
    });

    it('finishes cached parsing when the last slice-ready event arrives after 100 percent progress', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'hash';
        session.snapshotParsingComplete = true;
        session.deviceIds = { 0: ['BLOCK'] };
        session.memSnapshotParseLoading = true;
        // 缓存命中时 100% 事件可能早于 parseCompleted 到达并被忽略。
        session.memSnapshotParseProgress = 0;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;

        parseSliceReadyHandler({
            fileId: 'C:\\data\\snapshot.pickle',
            fileHash: 'hash',
            deviceId: '0',
            slice: { index: 0, startEventId: 0, endEventId: 99, ready: true },
        });

        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(100);
    });

    it('restores the active parse file from framework session state', () => {
        const session = store.sessionStore.activeSession as Session;

        updateSessionHandler({ module: 'memsnapshot' });
        updateSessionHandler({ snapshotParsingComplete: false });
        updateSessionHandler({ dbPath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
    });

    it('restores the parse file while cached slice-ready events are still arriving', () => {
        const session = store.sessionStore.activeSession as Session;

        updateSessionHandler({ module: 'memsnapshot' });
        updateSessionHandler({ snapshotParsingComplete: true });
        updateSessionHandler({ snapshotSlices: incompleteSlices });
        updateSessionHandler({ dbPath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
    });

    it('preserves snapshot fields that arrive before the module field', () => {
        const session = store.sessionStore.activeSession as Session;

        updateSessionHandler({ snapshotParsingComplete: false });
        updateSessionHandler({ snapshotSlices: incompleteSlices });
        updateSessionHandler({ dbPath: 'C:\\data\\snapshot.pickle' });
        updateSessionHandler({ module: 'memsnapshot' });

        expect(session.snapshotParsingComplete).toBe(false);
        expect(session.snapshotSlices).toEqual(incompleteSlices);
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.selectedSliceIndex).toBe(2);
    });

    it('restores the active snapshot parse from the current framework directory', () => {
        const session = store.sessionStore.activeSession as Session;

        switchDirectoryHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.selectedSliceIndex).toBe(-1);
        expect(session.snapshotSlices).toEqual({});
        expect(session.snapshotParsingComplete).toBe(false);
    });

    it('resets stale window state when switching to another snapshot', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.memSnapshotParseLoading = false;
        session.memSnapshotParseProgress = 100;
        session.memSnapshotParseFileId = 'C:\\data\\old.pickle';
        session.snapshotSlices = incompleteSlices;
        session.selectedSliceIndex = 2;
        session.fileHash = 'old-hash';
        session.snapshotParsingComplete = true;

        session.minTime = 50;
        session.maxTime = 80;
        session.deviceId = '0';

        switchDirectoryHandler({ selectedFilePath: 'C:\\data\\new.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\new.pickle');
        expect(session.selectedSliceIndex).toBe(-1);
        expect(session.snapshotSlices).toEqual({});
        expect(session.snapshotParsingComplete).toBe(false);
        expect(session.fileHash).toBe('');
        expect(session.minTime).toBe(0);
        expect(session.maxTime).toBe(0);
        expect(session.deviceId).toBe('');
    });

    it('keeps overall parse progress when switching to the same snapshot file', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 62;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;
        session.selectedSliceIndex = 2;

        switchDirectoryHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(62);
        expect(session.selectedSliceIndex).toBe(2);
        expect(session.snapshotSlices).toEqual(incompleteSlices);
    });

    it('resets stale window state when importing a new snapshot', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.memSnapshotParseProgress = 80;
        session.memSnapshotParseFileId = 'C:\\data\\old.pickle';
        session.snapshotSlices = incompleteSlices;
        session.selectedSliceIndex = 2;
        session.fileHash = 'old-hash';
        session.snapshotParsingComplete = true;

        session.minTime = 100;
        session.maxTime = 200;
        session.deviceId = '0';
        session.clickEventItem = { id: 1 } as any;

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.selectedSliceIndex).toBe(-1);
        expect(session.snapshotSlices).toEqual({});
        expect(session.snapshotParsingComplete).toBe(false);
        expect(session.fileHash).toBe('');
        expect(session.minTime).toBe(0);
        expect(session.maxTime).toBe(0);
        expect(session.deviceId).toBe('');
        expect(session.clickEventItem).toBeNull();
        expect(session.leakStats.error).toBe(false);
        expect(session.leakStats.loading).toBe(false);
    });

    it('hydrates overall parse progress from the framework session', () => {
        const session = store.sessionStore.activeSession as Session;

        updateSessionHandler({
            module: 'memsnapshot',
            snapshotParsingComplete: false,
            dbPath: 'C:\\data\\snapshot.pickle',
            memSnapshotParseFileId: 'C:\\data\\snapshot.pickle',
            memSnapshotParseLoading: true,
            memSnapshotParseProgress: 41,
            snapshotSlices: incompleteSlices,
        });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(41);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.selectedSliceIndex).toBe(2);
        expect(session.snapshotSlices).toEqual(incompleteSlices);
    });

    it('does not clear an active snapshot parse when framework still reports the previous module', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'leaks';
        session.snapshotParsingComplete = false;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 62;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;
        session.selectedSliceIndex = 2;

        updateSessionHandler({ module: 'leaks' });

        expect(session.snapshotParsingComplete).toBe(false);
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(62);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.snapshotSlices).toEqual(incompleteSlices);
        expect(session.selectedSliceIndex).toBe(2);
    });

    it('clears snapshot parsing state when importing a non-snapshot data source', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.snapshotParsingComplete = false;
        session.memSnapshotCacheRefreshPending = true;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 62;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;
        session.selectedSliceIndex = 2;

        importRemoteHandler({ selectedFilePath: 'C:\\data\\leaks_dump.db' });

        expect(session.snapshotParsingComplete).toBe(true);
        expect(session.memSnapshotCacheRefreshPending).toBe(false);
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('');
        expect(session.snapshotSlices).toEqual({});
        expect(session.selectedSliceIndex).toBe(-1);
    });

    it('accepts progress and the first ready slice after the stale module status arrives', () => {
        const session = store.sessionStore.activeSession as Session;

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });
        updateSessionHandler({ module: 'leaks' });
        parseProgressHandler({ fileId: 'C:\\data\\snapshot.pickle', progress: 12 });
        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: false,
            snapshotSlices: incompleteSlices,
        });

        expect(session.module).toBe('memsnapshot');
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(12);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.selectedSliceIndex).toBe(2);
    });

    it('does not enter snapshot loading state after memscope parsing completes', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 41;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;

        parseCompletedHandler({
            dbPath: 'C:\\data\\leaks_dump.db',
            deviceIds: { 0: ['PTA'] },
            threadIds: [1],
            module: 'leaks',
            fileHash: '',
        });

        expect(session.module).toBe('leaks');
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('');
        expect(session.snapshotSlices).toEqual({});
        expect(session.selectedSliceIndex).toBe(-1);
    });

    it('waits for final completion after the last pending slice becomes ready', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'hash';
        session.snapshotParsingComplete = false;
        session.deviceIds = { 0: ['BLOCK'] };
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 99;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = incompleteSlices;

        parseSliceReadyHandler({
            fileId: 'C:\\data\\snapshot.pickle',
            fileHash: 'hash',
            deviceId: '0',
            slice: { index: 0, startEventId: 0, endEventId: 99, ready: true },
        });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(99);
        expect(session.snapshotSlices['0'].readySlices).toEqual([0, 1, 2]);

        parseProgressHandler({ fileId: 'C:\\data\\snapshot.pickle', progress: 100 });
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(100);

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: session.snapshotSlices,
        });

        expect(session.memSnapshotParseLoading).toBe(false);
    });

    it('ignores late events from a previously imported snapshot', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'current-hash';
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseProgress = 12;
        session.memSnapshotParseFileId = 'C:\\data\\current.pickle';
        session.snapshotSlices = incompleteSlices;
        session.deviceIds = { 0: ['BLOCK'] };

        parseProgressHandler({ fileId: 'C:\\data\\previous.pickle', progress: 90 });
        parseSliceReadyHandler({
            fileId: 'C:\\data\\previous.pickle',
            fileHash: 'previous-hash',
            deviceId: '1',
            slice: { index: 0, startEventId: 0, endEventId: 99, ready: true },
        });
        parseCompletedHandler({
            dbPath: 'C:\\data\\previous.pickle',
            deviceIds: { 1: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'previous-hash',
            snapshotSlices: {},
        });

        expect(session.memSnapshotParseProgress).toBe(12);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\current.pickle');
        expect(session.fileHash).toBe('current-hash');
        expect(session.deviceIds).toEqual({ 0: ['BLOCK'] });
    });

    it('matches Windows snapshot paths without case sensitivity', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseFileId = 'C:\\Data\\Snapshot.PICKLE';

        parseProgressHandler({ fileId: 'c:/data/snapshot.pickle', progress: 25 });

        expect(session.memSnapshotParseProgress).toBe(25);
    });

    it('preserves the selected window and marks caches stale when parsing is finalized', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'hash';
        session.snapshotParsingComplete = false;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.selectedSliceIndex = 1;
        session.loadedMemoryBlockContextKey = 'stale-context';
        session.snapshotSlices = incompleteSlices;

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: {
                0: {
                    ...incompleteSlices[0],
                    readySlices: [0, 1, 2],
                    slices: incompleteSlices[0].slices.map(slice => ({ ...slice, ready: true })),
                },
            },
        });

        expect(session.selectedSliceIndex).toBe(1);
        expect(session.snapshotParsingComplete).toBe(true);
        expect(session.memSnapshotCacheRefreshPending).toBe(true);
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.loadedMemoryBlockContextKey).toBe('');
        expect(session.loadingBlocks).toBe(true);
        const blockWorker = jest.requireMock('@/leaksWorker/blockWorker/worker') as { workerDestroy: jest.Mock };
        expect(blockWorker.workerDestroy).toHaveBeenCalledTimes(1);
    });

    it('skips cache finalization refresh when the snapshot has only one slice', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'hash';
        session.snapshotParsingComplete = false;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.selectedSliceIndex = 0;
        session.deviceId = '0';
        session.allocationData = { allocations: [{ timestamp: 1, totalSize: 8 }], maxTimestamp: 1, minTimestamp: 0 };
        session.loadedMemoryBlockContextKey = 'current-context';
        const singleSlice = {
            0: {
                eventCount: 100,
                sliceCount: 1,
                readySlices: [0],
                slices: [{ index: 0, startEventId: 0, endEventId: 99, ready: true }],
            },
        };
        session.snapshotSlices = singleSlice;

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: singleSlice,
        });

        expect(session.memSnapshotCacheRefreshPending).toBe(false);
        expect(session.loadedMemoryBlockContextKey).toBe('current-context');
        expect(session.snapshotParsingComplete).toBe(true);
        expect(session.deviceId).toBe('0');
        expect(session.selectedSliceIndex).toBe(0);
        expect(session.allocationData.allocations).toHaveLength(1);
        const blockWorker = jest.requireMock('@/leaksWorker/blockWorker/worker') as { workerDestroy: jest.Mock };
        expect(blockWorker.workerDestroy).not.toHaveBeenCalled();
        expect(hasMultipleMemSnapshotSlices(singleSlice)).toBe(false);
        expect(hasMultipleMemSnapshotSlices(incompleteSlices)).toBe(true);
    });

    it('does not reopen overall parsing when re-importing a fully parsed snapshot', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.fileHash = 'hash';
        session.snapshotParsingComplete = true;
        session.memSnapshotParseLoading = false;
        session.memSnapshotParseProgress = 100;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = completeSlices;
        session.selectedSliceIndex = 2;

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(100);
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.selectedSliceIndex).toBe(2);
        expect(session.fileHash).toBe('hash');
    });

    it('hides overall parsing when switching back to a fully parsed snapshot', () => {
        const session = store.sessionStore.activeSession as Session;
        session.module = 'memsnapshot';
        session.snapshotParsingComplete = true;
        session.memSnapshotParseLoading = false;
        session.memSnapshotParseProgress = 100;
        session.memSnapshotParseFileId = 'C:\\data\\snapshot.pickle';
        session.snapshotSlices = completeSlices;
        session.selectedSliceIndex = 2;

        switchDirectoryHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.selectedSliceIndex).toBe(2);
    });

    it('applies a cache-hit completion that arrived before remote/import', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        session.memSnapshotParseFileId = 'C:\\data\\old.pickle';
        session.snapshotParsingComplete = false;

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: completeSlices,
        });

        expect(session.memSnapshotParseFileId).toBe('C:\\data\\old.pickle');

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(100);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.fileHash).toBe('hash');
    });

    it('replays cache-hit completion when import starts after parse finished', () => {
        const session = store.sessionStore.activeSession as Session;

        parseCompletedHandler({
            dbPath: 'C:\\data\\snapshot.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'hash',
            snapshotParsingComplete: true,
            snapshotSlices: completeSlices,
        });

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(100);
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.selectedSliceIndex).toBe(2);
    });

    it('does not replay a previous snapshot completion onto a newly imported file', () => {
        const session = store.sessionStore.activeSession as Session;

        parseCompletedHandler({
            dbPath: 'C:\\data\\old.pickle',
            deviceIds: { 0: ['BLOCK'] },
            threadIds: [],
            module: 'memsnapshot',
            fileHash: 'old-hash',
            snapshotParsingComplete: true,
            snapshotSlices: completeSlices,
        });

        importRemoteHandler({ selectedFilePath: 'C:\\data\\snapshot.pickle' });

        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.memSnapshotParseFileId).toBe('C:\\data\\snapshot.pickle');
        expect(session.snapshotSlices).toEqual({});
        expect(session.snapshotParsingComplete).toBe(false);
    });
});

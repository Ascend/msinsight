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
    getLeaksParseStatusHandler,
    parseFailHandler,
    frameLoadedHandler,
} from './handler';
import connector from '@/connection';
import { errorCenter } from '@insight/lib';

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
jest.mock('@/connection', () => ({
    __esModule: true,
    default: { send: jest.fn() },
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

    it.each(['import', 'frame', 'progress'])('preserves failure before a delayed %s event', (event) => {
        const fileId = '/data/snapshot.pkl';
        const session = store.sessionStore.activeSession as Session;
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: [fileId], memSnapshotParseFailure: null });
        parseFailHandler({ rankId: fileId, error: 'Python spawn failed' });
        if (event === 'import') importRemoteHandler({ selectedFilePath: fileId });
        if (event === 'frame') frameLoadedHandler({ selectedFilePath: fileId });
        if (event === 'progress') parseProgressHandler({ fileId, progress: 10 });
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseFailure?.error).toBe('Python spawn failed');
    });

    it.each(['frame', 'nested-frame', 'import', 'update'])('preserves failure after delayed complete %s metadata containing null', event => {
        const session = store.sessionStore.activeSession as Session;
        const fileId = '/data/snapshot.pkl';
        const imported = {
            dataSource: { selectedFilePath: fileId, projectPath: [fileId] },
            importResult: { children: [{ filePath: fileId }] },
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: [fileId],
            memSnapshotImportPending: false, memSnapshotParseFailure: null,
        };
        importRemoteHandler(imported);
        parseFailHandler({ rankId: fileId, error: 'Python spawn failed' });
        if (event === 'frame') frameLoadedHandler({
            memSnapshotParseAttempt: 1, memSnapshotParseFailure: null,
            selectedFileType: 'RANK', selectedFilePath: fileId, selectedProjectName: 'snapshot',
            pageInfo: { cluster: {}, timeline: {} },
        });
        // Kept as a defensive compatibility shape; the current TabPane producer uses top-level metadata.
        if (event === 'nested-frame') frameLoadedHandler({ remoteImportData: imported });
        if (event === 'import') importRemoteHandler(imported);
        if (event === 'update') updateSessionHandler({
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: [fileId], memSnapshotImportPending: false,
            memSnapshotParseFailure: null, memSnapshotParseLoading: true,
        });
        expect(session.memSnapshotParseAttempt).toBe(1);
        expect(session.memSnapshotParseFailure).toEqual({ fileId, error: 'Python spawn failed' });
        expect(session.memSnapshotParseFileId).toBe(fileId);
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.dbPath).toBe('');
        expect(session.fileHash).toBe('');
        expect(session.snapshotSlices).toEqual({});
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });

    it('restores failure received while the iframe was absent', () => {
        const session = store.sessionStore.activeSession as Session;
        frameLoadedHandler({
            selectedFilePath: '/data/snapshot.pkl',
            memSnapshotParseAttempt: 1,
            memSnapshotParseFailure: { fileId: '/data/snapshot.pkl', error: 'Python spawn failed' },
        });
        updateSessionHandler({ memSnapshotParseLoading: true });
        updateSessionHandler({ memSnapshotParseFileId: '', dbPath: '/old/data.json', fileHash: 'old-hash' });
        expect(session.dbPath).toBe('');
        expect(session.fileHash).toBe('');
        expect(session.memSnapshotParseFileId).toBe('/data/snapshot.pkl');
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseFailure?.fileId).toBe('/data/snapshot.pkl');
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });

    it('allows an explicit new import of the same failed file and ignores old initialization', () => {
        const fileId = '/data/snapshot.pkl';
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({ selectedFilePath: fileId, memSnapshotParseAttempt: 1 });
        parseFailHandler({ rankId: fileId, error: 'Python spawn failed' });
        importRemoteHandler({ selectedFilePath: fileId, memSnapshotParseAttempt: 2 });
        frameLoadedHandler({
            selectedFilePath: fileId,
            memSnapshotParseAttempt: 1,
            memSnapshotParseFailure: { fileId, error: 'old failure' },
        });
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFailure).toBeNull();
    });

    it.each(['empty', 'ordinary'])('ignores an unrelated late snapshot failure with an %s current file', current => {
        const session = store.sessionStore.activeSession as Session;
        session.dbPath = current === 'ordinary' ? '/data/trace.json' : '';
        session.fileHash = 'current-hash';
        session.deviceIds = { 0: ['BLOCK'] };
        session.threadIds = [7];
        parseFailHandler({ rankId: '/old/snapshot.pkl', error: 'late failure' });
        expect(session.dbPath).toBe(current === 'ordinary' ? '/data/trace.json' : '');
        expect(session.fileHash).toBe('current-hash');
        expect(session.deviceIds).toEqual({ 0: ['BLOCK'] });
        expect(session.threadIds).toEqual([7]);
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(errorCenter.handleError).not.toHaveBeenCalled();
    });

    it('accepts a pending directory failure but not the previous source failure', () => {
        const session = store.sessionStore.activeSession as Session;
        session.dbPath = '/old/trace.json';
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/new'], memSnapshotParseFailure: null });
        parseFailHandler({ rankId: '/old/snapshot.pkl', error: 'old failure' });
        expect(session.memSnapshotParseFailure).toBeNull();
        parseFailHandler({ rankId: '/new/snapshot.pkl', error: 'new failure' });
        importRemoteHandler({
            dataSource: { projectPath: ['/new'] },
            importResult: { children: [{ filePath: '/new/snapshot.pkl' }] },
            memSnapshotParseAttempt: 1,
            memSnapshotSourcePaths: ['/new'],
        });
        expect(session.dbPath).toBe('');
        expect(session.memSnapshotParseFileId).toBe('/new/snapshot.pkl');
        expect(session.memSnapshotParseFailure?.error).toBe('new failure');
        expect(session.memSnapshotParseLoading).toBe(false);
    });

    it.each([undefined, '', '   '])('uses a nonempty error for %p', error => {
        const session = store.sessionStore.activeSession as Session;
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/snapshot.pkl'] });
        parseFailHandler({ rankId: '/data/snapshot.pkl', error });
        expect(session.memSnapshotParseFailure?.error).toBe('Failed to parse snapshot data.');
    });

    it('restores failed A after successful B without starting a new attempt', () => {
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({ selectedFilePath: '/data/a.pkl', memSnapshotParseAttempt: 1 });
        parseFailHandler({ rankId: '/data/a.pkl', error: 'A failed' });
        switchDirectoryHandler({ selectedFilePath: '/data/b.pkl' });
        parseCompletedHandler({
            module: 'memsnapshot', dbPath: '/data/b.pkl', fileHash: 'b-hash',
            deviceIds: { 0: ['BLOCK'] }, threadIds: [7], snapshotParsingComplete: true, snapshotSlices: completeSlices,
        });
        expect(session.dbPath).toBe('/data/b.pkl');
        expect(session.selectedSliceIndex).toBe(2);
        jest.clearAllMocks();
        switchDirectoryHandler({ selectedFilePath: '/data/a.pkl' });
        expect(session.memSnapshotParseAttempt).toBe(1);
        expect(session.memSnapshotParseFileId).toBe('/data/a.pkl');
        expect(session.memSnapshotParseFailure?.error).toBe('A failed');
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.dbPath).toBe('');
        expect(session.fileHash).toBe('');
        expect(session.deviceIds).toEqual({});
        expect(session.threadIds).toEqual([]);
        expect(session.snapshotSlices).toEqual({});
        expect(session.selectedSliceIndex).toBe(-1);
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).toHaveBeenCalledTimes(1);
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).toHaveBeenCalledTimes(1);
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
        parseFailHandler({ rankId: '/data/a.pkl', error: 'A failed' });
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });

    it('rejects stale nested initialization and state hydration after retry', () => {
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({ selectedFilePath: '/data/a.pkl', memSnapshotParseAttempt: 2 });
        frameLoadedHandler({ remoteImportData: {
            selectedFilePath: '/data/a.pkl', memSnapshotParseAttempt: 1,
            memSnapshotParseFailure: { fileId: '/data/a.pkl', error: 'old failure' },
        } });
        updateSessionHandler({ memSnapshotParseAttempt: 1, dbPath: '/data/a.pkl', fileHash: 'old-hash' });
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotParseLoading).toBe(true);
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(session.fileHash).toBe('');
    });

    it('does not let old directory hydration replace pending import ownership', () => {
        const session = store.sessionStore.activeSession as Session;
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/new'] });
        parseFailHandler({ rankId: '/new/snapshot.pkl', error: 'new failure' });
        switchDirectoryHandler({ selectedFilePath: '/old/trace.json', memSnapshotSourcePaths: ['/new'] });
        expect(session.memSnapshotParseFileId).toBe('/new/snapshot.pkl');
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseFailure?.error).toBe('new failure');
    });

    it('does not apply an older ordinary response or directory while a snapshot folder import is pending', () => {
        const session = store.sessionStore.activeSession as Session;
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data'], memSnapshotImportPending: true });
        frameLoadedHandler({ selectedFilePath: '/data/previous.pkl' });
        importRemoteHandler({ dataSource: { projectPath: ['/data/trace.json'] }, memSnapshotImportPending: true });
        switchDirectoryHandler({
            selectedFilePath: '/data/previous.pkl', memSnapshotSourcePaths: ['/data'], memSnapshotImportPending: true,
        });
        expect(session.memSnapshotSourcePaths).toEqual(['/data']);
        expect(session.memSnapshotParseFileId).toBe('');
        parseFailHandler({ rankId: '/data/new.pkl', error: 'new failure' });
        expect(session.memSnapshotParseFailure?.fileId).toBe('/data/new.pkl');
        expect(session.memSnapshotParseLoading).toBe(false);
    });

    it('does not replay a failed snapshot after an ordinary import succeeds', () => {
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({ selectedFilePath: '/failed.pkl', memSnapshotParseAttempt: 1 });
        parseFailHandler({ rankId: '/failed.pkl', error: 'old failure' });
        importRemoteHandler({ dataSource: { projectPath: ['/trace.json'] }, memSnapshotImportPending: false });
        frameLoadedHandler({
            selectedFilePath: '/trace.json', memSnapshotParseAttempt: 1,
            memSnapshotSourcePaths: ['/trace.json'], memSnapshotParseFailure: null,
        });
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(session.memSnapshotParseFileId).toBe('');
        expect(session.memSnapshotParseLoading).toBe(false);
    });

    it('does not clear a resolved directory snapshot when its sibling fails late', () => {
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({
            dataSource: { projectPath: ['/data'] }, importResult: { children: [{ filePath: '/data/b.pkl' }] },
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data'], memSnapshotImportPending: false,
        });
        parseCompletedHandler({
            module: 'memsnapshot', dbPath: '/data/b.pkl', fileHash: 'b-hash',
            deviceIds: { 0: ['BLOCK'] }, threadIds: [7], snapshotParsingComplete: true, snapshotSlices: completeSlices,
        });
        jest.clearAllMocks();
        parseFailHandler({ rankId: '/data/a.pkl', error: 'late sibling failure' });
        expect(session.memSnapshotSourcePaths).toEqual(['/data/b.pkl']);
        expect(session.dbPath).toBe('/data/b.pkl');
        expect(session.fileHash).toBe('b-hash');
        expect(session.deviceIds).toEqual({ 0: ['BLOCK'] });
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(errorCenter.handleError).not.toHaveBeenCalled();
    });

    it('keeps selected B through getInitStatus hydration when failed A is only a cached sibling', () => {
        const session = store.sessionStore.activeSession as Session;
        const failure = { fileId: '/data/a.pkl', error: 'A failed' };
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data'], memSnapshotImportPending: true });
        parseFailHandler({ rankId: failure.fileId, error: failure.error });
        importRemoteHandler({
            dataSource: { projectPath: ['/data'] }, importResult: { children: [{ filePath: '/data/b.pkl' }] },
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/b.pkl'],
            memSnapshotImportPending: false, memSnapshotParseFailure: failure,
        });
        parseCompletedHandler({
            module: 'memsnapshot', dbPath: '/data/b.pkl', fileHash: 'b-hash',
            deviceIds: { 0: ['BLOCK'] }, threadIds: [], snapshotParsingComplete: true, snapshotSlices: completeSlices,
        });
        jest.clearAllMocks();
        const initialization = {
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/b.pkl'],
            memSnapshotImportPending: false, memSnapshotParseFailure: failure,
        };
        const state: Record<string, unknown> = {
            memSnapshotParseFileId: '/data/b.pkl', memSnapshotParseLoading: false, memSnapshotParseProgress: 100,
            fileHash: 'b-hash', module: 'memsnapshot', deviceIds: { 0: ['BLOCK'] }, threadIds: [],
            snapshotParsingComplete: true, snapshotSlices: completeSlices, dbPath: '/data/b.pkl',
        };
        // The same order requested by getInitStatus; the three metadata keys are atomic groups.
        [
            'memSnapshotParseAttempt', 'memSnapshotSourcePaths', 'memSnapshotParseFileId',
            'memSnapshotParseLoading', 'memSnapshotParseProgress', 'memSnapshotParseFailure',
            'fileHash', 'module', 'deviceIds', 'threadIds', 'snapshotParsingComplete', 'snapshotSlices', 'dbPath',
        ].forEach(key => updateSessionHandler(key in initialization ? initialization : {
            memSnapshotParseAttempt: 1, [key]: state[key],
        }));
        expect(session.memSnapshotParseFileId).toBe('/data/b.pkl');
        expect(session.dbPath).toBe('/data/b.pkl');
        expect(session.fileHash).toBe('b-hash');
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(session.memSnapshotParseFailure).toEqual(failure);
        expect(errorCenter.handleError).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).not.toHaveBeenCalled();
    });

    it('retains failed A during cold hydration of successful B and restores A only when selected', () => {
        const session = store.sessionStore.activeSession as Session;
        const failure = { fileId: '/data/a.pkl', error: 'A failed' };
        const initialization = {
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/b.pkl'],
            memSnapshotImportPending: false, memSnapshotParseFailure: failure,
        };
        const state: Record<string, unknown> = {
            memSnapshotParseFileId: '/data/b.pkl', memSnapshotParseLoading: false, memSnapshotParseProgress: 100,
            fileHash: 'b-hash', module: 'memsnapshot', deviceIds: { 0: ['BLOCK'] }, threadIds: [],
            snapshotParsingComplete: true, snapshotSlices: completeSlices, dbPath: '/data/b.pkl',
        };
        [
            'memSnapshotParseAttempt', 'memSnapshotSourcePaths', 'memSnapshotParseFileId',
            'memSnapshotParseLoading', 'memSnapshotParseProgress', 'memSnapshotParseFailure',
            'fileHash', 'module', 'deviceIds', 'threadIds', 'snapshotParsingComplete', 'snapshotSlices', 'dbPath',
        ].forEach(key => updateSessionHandler(key in initialization ? initialization : {
            memSnapshotParseAttempt: 1, [key]: state[key],
        }));
        expect(session.memSnapshotParseFailure).toEqual(failure);
        expect(session.memSnapshotParseFileId).toBe('/data/b.pkl');
        expect(session.dbPath).toBe('/data/b.pkl');
        expect(session.fileHash).toBe('b-hash');
        expect(session.snapshotSlices).toEqual(completeSlices);
        expect(errorCenter.handleError).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).not.toHaveBeenCalled();
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).not.toHaveBeenCalled();
        // An older generation must not replace the inactive cache either.
        updateSessionHandler({ ...initialization, memSnapshotParseAttempt: 0, memSnapshotParseFailure: { fileId: '/old.pkl', error: 'old' } });
        expect(session.memSnapshotParseFailure).toEqual(failure);
        switchDirectoryHandler({ selectedFilePath: '/data/a.pkl' });
        expect(session.memSnapshotParseFileId).toBe('/data/a.pkl');
        expect(session.memSnapshotParseFailure).toEqual(failure);
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.dbPath).toBe('');
        expect(session.fileHash).toBe('');
        expect(session.snapshotSlices).toEqual({});
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
        expect(jest.requireMock('@/leaksWorker/blockWorker/worker').workerDestroy).toHaveBeenCalledTimes(1);
        expect(jest.requireMock('@/leaksWorker/stateWorker/worker').workerDestroy).toHaveBeenCalledTimes(1);
    });

    it('preserves cached failure on same-generation null and clears it only on a newer generation', () => {
        const session = store.sessionStore.activeSession as Session;
        const failure = { fileId: '/data/a.pkl', error: 'A failed' };
        updateSessionHandler({ memSnapshotParseAttempt: 2, memSnapshotSourcePaths: ['/data/b.pkl'], memSnapshotParseFailure: failure });
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotParseFailure: null });
        expect(session.memSnapshotParseFailure).toEqual(failure);
        updateSessionHandler({ memSnapshotParseAttempt: 2, memSnapshotParseFailure: null });
        expect(session.memSnapshotParseFailure).toEqual(failure);
        updateSessionHandler({ memSnapshotParseAttempt: 3, memSnapshotParseFailure: null });
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(errorCenter.handleError).not.toHaveBeenCalled();
    });

    it('does not let a late inactive A cache replace current B failure or restart B', () => {
        const session = store.sessionStore.activeSession as Session;
        const fileId = '/data/b.pkl';
        const imported = {
            dataSource: { selectedFilePath: fileId, projectPath: [fileId] },
            memSnapshotParseAttempt: 1, memSnapshotSourcePaths: [fileId],
            memSnapshotImportPending: false, memSnapshotParseFailure: null,
        };
        importRemoteHandler(imported);
        parseFailHandler({ rankId: fileId, error: 'B failed' });
        const staleFailure = { fileId: '/data/a.pkl', error: 'old A failure' };
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: [fileId], memSnapshotParseFailure: staleFailure });
        expect(session.memSnapshotParseFailure).toEqual({ fileId, error: 'B failed' });
        parseProgressHandler({ fileId, progress: 50 });
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseFailure).toEqual({ fileId, error: 'B failed' });
        parseCompletedHandler({
            module: 'memsnapshot', dbPath: fileId, fileHash: 'late-b-hash',
            deviceIds: { 0: ['BLOCK'] }, threadIds: [], snapshotParsingComplete: true, snapshotSlices: completeSlices,
        });
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.fileHash).toBe('');
        expect(session.snapshotSlices).toEqual({});
        frameLoadedHandler({
            selectedFilePath: fileId, memSnapshotParseAttempt: 1,
            memSnapshotSourcePaths: [fileId], memSnapshotParseFailure: staleFailure,
        });
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseFailure).toEqual({ fileId, error: 'B failed' });
        importRemoteHandler({ ...imported, memSnapshotParseFailure: staleFailure });
        expect(session.memSnapshotParseAttempt).toBe(1);
        expect(session.memSnapshotParseFailure).toEqual({ fileId, error: 'B failed' });
        expect(session.memSnapshotParseFileId).toBe(fileId);
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(session.memSnapshotParseProgress).toBe(0);
        expect(session.dbPath).toBe('');
        expect(session.fileHash).toBe('');
        expect(session.snapshotSlices).toEqual({});
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });

    it('allows a fresh inactive failure cache after a newer attempt clears the active failure', () => {
        const session = store.sessionStore.activeSession as Session;
        importRemoteHandler({ selectedFilePath: '/data/b.pkl', memSnapshotParseAttempt: 1 });
        parseFailHandler({ rankId: '/data/b.pkl', error: 'B failed' });
        const failure = { fileId: '/data/a.pkl', error: 'new A cache' };
        updateSessionHandler({
            memSnapshotParseAttempt: 2, memSnapshotSourcePaths: ['/data/b.pkl'], memSnapshotParseFailure: failure,
        });
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotParseFailure).toEqual(failure);
        expect(session.memSnapshotParseFileId).toBe('');
        expect(session.memSnapshotParseLoading).toBe(false);
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });

    it('reports the first hydrated failure of the already-selected file exactly once', () => {
        const failure = { fileId: '/data/a.pkl', error: 'A failed' };
        updateSessionHandler({ memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/a.pkl'], memSnapshotParseFileId: '/data/a.pkl' });
        const metadata = { memSnapshotParseAttempt: 1, memSnapshotSourcePaths: ['/data/a.pkl'], memSnapshotParseFailure: failure };
        updateSessionHandler(metadata);
        updateSessionHandler(metadata);
        expect(store.sessionStore.activeSession!.memSnapshotParseFailure).toEqual(failure);
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
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

describe('getLeaksParseStatusHandler', () => {
    const mockedSend = connector.send as jest.MockedFunction<typeof connector.send>;

    beforeEach(() => {
        jest.clearAllMocks();
        store.sessionStore.activeSession = new Session();
    });

    it('reports incomplete while snapshot parse is loading', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = true;
        getLeaksParseStatusHandler({});
        expect(mockedSend).toHaveBeenCalledWith({
            event: 'leaksParseStatus',
            body: { complete: false },
        });
    });

    it('reports complete after snapshot parse loading ends', () => {
        const session = store.sessionStore.activeSession as Session;
        session.memSnapshotParseLoading = false;
        getLeaksParseStatusHandler({});
        expect(mockedSend).toHaveBeenCalledWith({
            event: 'leaksParseStatus',
            body: { complete: true },
        });
    });
});

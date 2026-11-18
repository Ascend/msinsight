/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { sendDirectory, sendSessionKey } from './sendNotification';
import connector from '@/connection/index';
import { store } from '@/store';

jest.mock('@/connection/index', () => ({ __esModule: true, default: { send: jest.fn() } }), { virtual: true });
jest.mock('@/store', () => ({ store: { sessionStore: { activeSession: {} } } }), { virtual: true });
jest.mock('@insight/lib/theme', () => ({ themeInstance: {} }), { virtual: true });
jest.mock('@insight/lib', () => ({}), { virtual: true });
jest.mock('@/utils/enum', () => ({}), { virtual: true });
jest.mock('./agentPanelNotification', () => ({ sendAgentPanelNotification: jest.fn() }));

describe('snapshot initialization and directory ownership', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        store.sessionStore.activeSession = {
            isCompareStatus: false,
            activeDataSource: {
                projectName: 'previous', selectedFileType: 'RANK', selectedFilePath: '/data/previous.pkl',
            },
            memSnapshotParseAttempt: 3,
            memSnapshotSourcePaths: ['/data/new.pkl'],
            memSnapshotImportPending: true,
            memSnapshotParseFailure: { fileId: '/data/new.pkl', error: 'parse failed' },
            fileHash: '',
        } as any;
    });

    it.each([undefined, 'Leaks'])('does not overwrite pending ownership when sending a directory to %p', to => {
        sendDirectory(to);
        expect(store.sessionStore.activeSession.memSnapshotSourcePaths).toEqual(['/data/new.pkl']);
        expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
            event: 'switchDirectory',
            body: expect.objectContaining({ memSnapshotSourcePaths: ['/data/new.pkl'], memSnapshotImportPending: true }),
        }));
    });

    it('changes source ownership on explicit directory selection without incrementing the attempt', () => {
        const session = store.sessionStore.activeSession;
        session.memSnapshotImportPending = false;
        sendDirectory();
        expect(session.memSnapshotSourcePaths).toEqual(['/data/previous.pkl']);
        expect(session.memSnapshotParseAttempt).toBe(3);
    });

    it.each(['memSnapshotParseAttempt', 'memSnapshotSourcePaths', 'memSnapshotParseFailure'])
    ('replays %s together with its generation, source and failure', key => {
        sendSessionKey(key, 'Leaks');
        expect(connector.send).toHaveBeenCalledWith({
            event: 'updateSession', to: 'Leaks',
            body: {
                memSnapshotParseAttempt: 3,
                memSnapshotSourcePaths: ['/data/new.pkl'],
                memSnapshotImportPending: true,
                memSnapshotParseFailure: { fileId: '/data/new.pkl', error: 'parse failed' },
            },
        });
    });

    it('tags individual snapshot state keys with their initialization generation', () => {
        sendSessionKey('fileHash', 'Leaks');
        expect(connector.send).toHaveBeenCalledWith({
            event: 'updateSession', to: 'Leaks', body: { memSnapshotParseAttempt: 3, fileHash: '' },
        });
    });
});

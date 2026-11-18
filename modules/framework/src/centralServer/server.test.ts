/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { addDataPath, CONNECTION_MAP } from './server';
import { importProject } from '@/utils/Request';
import { parseFailHandler, parseLeaksSuccessHandler } from '../connection/interceptorHandler';
import { store } from '@/store';
import connector from '@/connection';
import { updateProject } from '@/utils/Project';
import { errorCenter } from '@insight/lib';
import { ProjectAction } from '@/utils/enum';

// Load Leaks sources at runtime instead of importing them statically: their Leaks-local `@/` aliases
// do not resolve inside the Framework production type check.
const { Session: LeaksSession } = jest.requireActual<any>('../../../leaks/src/entity/session');
const { store: leaksStore } = jest.requireMock<any>('../../../leaks/src/store');
const {
    updateSessionHandler: updateLeaksSessionHandler,
    importRemoteHandler: importLeaksRemoteHandler,
    parseFailHandler: parseLeaksFailHandler,
} = jest.requireActual<any>('../../../leaks/src/connection/handler');

jest.mock('./websocket/defs', () => ({ GLOBAL_HOST: { remote: 'localhost', port: 9000 } }));
jest.mock('./websocket/connection', () => ({ Connection: jest.fn() }));
jest.mock('@/utils/Request', () => ({ importProject: jest.fn() }), { virtual: true });
jest.mock('@/utils/Rank', () => ({ updateRankMap: jest.fn() }), { virtual: true });
jest.mock('@/utils/Project', () => ({ transformFile: jest.fn(), updateProject: jest.fn() }), { virtual: true });
jest.mock('@/utils/useLoading', () => ({ closeLoading: jest.fn() }), { virtual: true });
jest.mock('@/components/TabPane/Index', () => ({ updateDataScene: jest.fn() }), { virtual: true });
jest.mock('@/vscode-adapter', () => ({ isVscodeEnv: () => false }), { virtual: true });
jest.mock('@/utils/enum', () => ({
    ProjectAction: { ADD_FILE: 'ADD_FILE' },
    SessionAction: { ADD_DATA_UNDER_PROJECT: 'ADD_DATA_UNDER_PROJECT' },
    ProjectType: { IE: 'IE' },
}), { virtual: true });
jest.mock('@insight/lib', () => ({ errorCenter: { handleError: jest.fn() }, ErrorCode: { PARSE_FAIL: 'PARSE_FAIL' }, WsError: Error }), { virtual: true });
jest.mock('@insight/lib/utils', () => ({}), { virtual: true });
jest.mock('@/connection', () => ({ __esModule: true, default: { send: jest.fn() } }), { virtual: true });
jest.mock('@/store', () => ({ store: { sessionStore: { activeSession: {} } } }), { virtual: true });
jest.mock('@/connection/notificationHandler', () => ({
    updateSession: (data: object) => Object.assign(jest.requireMock('@/store').store.sessionStore.activeSession, data),
}), { virtual: true });

// Route framework connector messages to the real Leaks handlers with a separate iframe store.
jest.mock('../../../leaks/src/store', () => ({ store: { sessionStore: { activeSession: {} } } }));
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { changeLanguage: jest.fn() } }), { virtual: true });
jest.mock('@/leaksWorker/blockWorker/worker', () => ({ workerDestroy: jest.fn() }), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({ workerDestroy: jest.fn() }), { virtual: true });
jest.mock('@/entity/session', () => ({
    LEAKS_WORKER_INFO_DEFAULT: { renderOptions: {} }, STATE_WORKER_INFO_DEFAULT: {}, MARK_LINE_POSITION_DEFAULT: {},
}), { virtual: true });

describe('snapshot import failure ordering', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        (connector.send as jest.Mock).mockReset();
        CONNECTION_MAP.clear();
        CONNECTION_MAP.set('localhost:9000', {} as any);
        store.sessionStore.activeSession = {
            memSnapshotParseAttempt: 0,
            memSnapshotParseFailure: null,
            memSnapshotSourcePaths: [],
            memSnapshotParseFileId: '',
            actionListener: { type: '' },
            activeDataSource: { projectName: '', selectedFilePath: '', projectPath: [] },
            reset() {
                this.memSnapshotParseFailure = null;
                this.memSnapshotSourcePaths = [];
                this.memSnapshotImportPending = false;
                this.memSnapshotParseFileId = '';
            },
            resetActionListener() {},
        } as any;
    });

    it.each(['empty', 'previous'])('preserves pre-response failure in an %s session and permits explicit retry', async (source) => {
        const session = store.sessionStore.activeSession;
        if (source === 'previous') {
            session.activeDataSource = { projectName: 'previous', selectedFilePath: '/old/snapshot.pkl', projectPath: ['/old/snapshot.pkl'] } as any;
            session.memSnapshotParseFileId = '/old/snapshot.pkl';
        }
        const project = { projectName: 'snapshot', projectPath: ['/data/snapshot.pkl'] } as any;
        const request = importProject as jest.MockedFunction<typeof importProject>;
        request.mockImplementationOnce(async () => {
            expect(session.memSnapshotSourcePaths).toEqual(['/data/snapshot.pkl']);
            parseFailHandler({ rankId: '/old/snapshot.pkl', error: 'unrelated late failure' });
            expect(session.memSnapshotParseFailure).toBeNull();
            parseFailHandler({ rankId: '/data/snapshot.pkl', error: 'Python spawn failed' });
            return { body: { reset: true, children: [] } } as any;
        });
        await addDataPath(project, ProjectAction.ADD_FILE, false, session);

        const notifications = (connector.send as jest.Mock).mock.calls.map(([message]) => message);
        expect(notifications.find(message => message.event === 'remote/import').body).toMatchObject({
            memSnapshotParseAttempt: 1,
            memSnapshotSourcePaths: ['/data/snapshot.pkl'],
            memSnapshotParseFailure: { fileId: '/data/snapshot.pkl', error: 'Python spawn failed' },
        });

        request.mockResolvedValueOnce({ body: { reset: true, children: [] } } as any);
        await addDataPath(project, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotParseFailure).toBeNull();
    });

    it('registers an unknown directory before its snapshot failure and response', async () => {
        const session = store.sessionStore.activeSession;
        (importProject as jest.Mock).mockImplementationOnce(async () => {
            parseFailHandler({ rankId: '/data/snapshot.pkl', error: '' });
            return { body: { reset: true, children: [
                { filePath: '/data/snapshot.pkl', type: 'RANK', children: [] },
            ] } };
        });
        await addDataPath({ projectName: 'folder', projectPath: ['/data'] } as any, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotParseFailure).toEqual({
            fileId: '/data/snapshot.pkl', error: 'Failed to parse snapshot data.',
        });
        expect(session.memSnapshotSourcePaths).toEqual(['/data/snapshot.pkl']);
    });

    it('processes both ordinary import responses without resetting snapshot initialization', async () => {
        const session = store.sessionStore.activeSession;
        session.memSnapshotParseAttempt = 7;
        session.memSnapshotParseFailure = { fileId: '/snapshot.pkl', error: 'previous failure' };
        session.memSnapshotSourcePaths = ['/snapshot.pkl'];
        let resolveFirst!: (value: any) => void;
        let resolveSecond!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveFirst = resolve; }))
            .mockReturnValueOnce(new Promise(resolve => { resolveSecond = resolve; }));
        const first = addDataPath({ projectName: 'first', projectPath: ['/first.json'] } as any, ProjectAction.ADD_FILE, false, session);
        const second = addDataPath({ projectName: 'second', projectPath: ['/second.json'] } as any, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotParseAttempt).toBe(7);
        expect(session.memSnapshotParseFailure?.error).toBe('previous failure');
        expect(session.memSnapshotSourcePaths).toEqual(['/snapshot.pkl']);
        resolveSecond({ body: { reset: false, children: [] } });
        await second;
        resolveFirst({ body: { reset: false, children: [] }, error: { code: 1, message: 'first warning' } });
        await first;
        const imports = (connector.send as jest.Mock).mock.calls.map(([message]) => message)
            .filter(message => message.event === 'remote/import');
        expect(imports.map(message => message.body.dataSource.projectName)).toEqual(['second', 'first']);
        expect(imports.every(message => message.body.memSnapshotParseAttempt === undefined)).toBe(true);
        expect(updateProject).toHaveBeenCalledTimes(2);
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
    });


    it('retains ordinary directory responses even though their contents were unknown at import start', async () => {
        const session = store.sessionStore.activeSession;
        let resolveFirst!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveFirst = resolve; }))
            .mockResolvedValueOnce({ body: { reset: false, children: [] } });
        const first = addDataPath({ projectName: 'first', projectPath: ['/first-folder'] } as any, ProjectAction.ADD_FILE, false, session);
        await addDataPath({ projectName: 'second', projectPath: ['/second-folder'] } as any, ProjectAction.ADD_FILE, false, session);
        resolveFirst({ body: { reset: false, children: [{ filePath: '/first-folder/trace.json', type: 'RANK', children: [] }] } });
        expect(await first).toBe(true);
        expect(updateProject).toHaveBeenCalledTimes(2);
        expect(session.memSnapshotImportPending).toBe(false);
    });

    it('clears pending ownership protection when import/action rejects', async () => {
        const session = store.sessionStore.activeSession;
        (importProject as jest.Mock).mockRejectedValueOnce(new Error('connection closed'));
        await expect(addDataPath({ projectName: 'snapshot', projectPath: ['/data/snapshot.pkl'] } as any,
            ProjectAction.ADD_FILE, false, session)).rejects.toThrow('connection closed');
        expect(session.memSnapshotImportPending).toBe(false);
    });


    it('preserves pending snapshot ownership across an earlier ordinary response reset', async () => {
        const session = store.sessionStore.activeSession;
        let resolveOrdinary!: (value: any) => void;
        let resolveSnapshot!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveOrdinary = resolve; }))
            .mockReturnValueOnce(new Promise(resolve => { resolveSnapshot = resolve; }));
        const ordinary = addDataPath({ projectName: 'ordinary', projectPath: ['/trace.json'] } as any,
            ProjectAction.ADD_FILE, false, session);
        const snapshot = addDataPath({ projectName: 'snapshot', projectPath: ['/new/snapshot.pkl'] } as any,
            ProjectAction.ADD_FILE, false, session);
        resolveOrdinary({ body: { reset: true, children: [] } });
        await ordinary;
        expect(session.memSnapshotImportPending).toBe(true);
        expect(session.memSnapshotSourcePaths).toEqual(['/new/snapshot.pkl']);
        expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
            event: 'remote/import', body: expect.objectContaining({ memSnapshotImportPending: true }),
        }));
        parseFailHandler({ rankId: '/new/snapshot.pkl', error: 'snapshot failed' });
        resolveSnapshot({ body: { reset: true, children: [] } });
        await snapshot;
        expect(session.memSnapshotParseFailure?.error).toBe('snapshot failed');
        expect(session.memSnapshotImportPending).toBe(false);
    });


    it('clears a previous snapshot failure once an ordinary import succeeds', async () => {
        const session = store.sessionStore.activeSession;
        session.memSnapshotParseAttempt = 1;
        session.memSnapshotParseFailure = { fileId: '/failed.pkl', error: 'old failure' };
        session.memSnapshotParseFileId = '/failed.pkl';
        session.memSnapshotSourcePaths = ['/failed.pkl'];
        (importProject as jest.Mock).mockResolvedValueOnce({ body: { reset: true, children: [] } });
        await addDataPath({ projectName: 'ordinary', projectPath: ['/trace.json'] } as any, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(session.memSnapshotParseFileId).toBe('');
        expect(session.memSnapshotSourcePaths).toEqual([]);
        expect(session.memSnapshotParseAttempt).toBe(1);
    });


    it('reports an older ordinary import error without resetting a later successful snapshot', async () => {
        const session = store.sessionStore.activeSession;
        let resolveOrdinary!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveOrdinary = resolve; }))
            .mockResolvedValueOnce({ body: { reset: true, children: [] } });
        const ordinary = addDataPath({ projectName: 'ordinary', projectPath: ['/trace.json'] } as any,
            ProjectAction.ADD_FILE, false, session);
        await addDataPath({ projectName: 'snapshot', projectPath: ['/snapshot.pkl'] } as any,
            ProjectAction.ADD_FILE, false, session);
        resolveOrdinary({ error: { code: 1, message: 'old import failed' } });
        expect(await ordinary).toBe(false);
        expect(errorCenter.handleError).toHaveBeenCalledTimes(1);
        expect((connector.send as jest.Mock).mock.calls.some(([message]) => message.event === 'remote/reset')).toBe(false);
    });


    it('narrows a resolved directory import before a sibling snapshot can fail', async () => {
        const session = store.sessionStore.activeSession;
        (importProject as jest.Mock).mockResolvedValueOnce({ body: { reset: true, children: [
            { filePath: '/data/b.pkl', type: 'RANK', children: [] },
        ] } });
        await addDataPath({ projectName: 'folder', projectPath: ['/data'] } as any, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotSourcePaths).toEqual(['/data/b.pkl']);
        parseLeaksSuccessHandler({
            module: 'memsnapshot', dbPath: '/data/b.pkl', fileHash: 'b-hash',
            deviceIds: { 0: ['BLOCK'] }, threadIds: [], snapshotParsingComplete: true,
            snapshotSlices: { 0: { sliceCount: 1, slices: [{ ready: true }] } },
        });
        parseFailHandler({ rankId: '/data/a.pkl', error: 'late sibling failure' });
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(session.fileHash).toBe('b-hash');
        expect(session.memSnapshotParseFileId).toBe('/data/b.pkl');
        expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
            event: 'remote/import', body: expect.objectContaining({ memSnapshotSourcePaths: ['/data/b.pkl'] }),
        }));
    });


    it.each([true, false])('keeps selected B current when pending directory failure A precedes reset=%p', async reset => {
        const session = store.sessionStore.activeSession;
        (importProject as jest.Mock).mockImplementationOnce(async () => {
            parseFailHandler({ rankId: '/data/a.pkl', error: 'A failed' });
            return { body: { reset, children: [{ filePath: '/data/b.pkl', type: 'RANK', children: [] }] } };
        });
        await addDataPath({ projectName: 'folder', projectPath: ['/data'] } as any, ProjectAction.ADD_FILE, false, session);
        expect(session.memSnapshotParseFileId).toBe('/data/b.pkl');
        expect(session.memSnapshotSourcePaths).toEqual(['/data/b.pkl']);
        expect(session.memSnapshotParseFailure).toEqual({ fileId: '/data/a.pkl', error: 'A failed' });
    });

});

describe('framework to Leaks import ordering', () => {
    beforeEach(() => {
        jest.clearAllMocks();
        (connector.send as jest.Mock).mockReset();
        (importProject as jest.Mock).mockReset();
        leaksStore.sessionStore.activeSession = new LeaksSession();
        store.sessionStore.activeSession = {
            memSnapshotParseAttempt: 0, memSnapshotParseFailure: null, memSnapshotSourcePaths: [],
            memSnapshotParseFileId: '', memSnapshotImportPending: false,
            actionListener: { type: '' }, activeDataSource: { projectName: '', selectedFilePath: '', projectPath: [] },
            reset() {
                this.memSnapshotParseFailure = null;
                this.memSnapshotSourcePaths = [];
                this.memSnapshotParseFileId = '';
                this.memSnapshotImportPending = false;
            },
            resetActionListener() {},
        } as any;
        CONNECTION_MAP.clear();
        CONNECTION_MAP.set('localhost:9000', {} as any);
        (connector.send as jest.Mock).mockImplementation(message => {
            if (message.event === 'updateSession') updateLeaksSessionHandler(message.body);
            if (message.event === 'remote/import') importLeaksRemoteHandler(message.body);
        });
    });

    afterEach(() => {
        (connector.send as jest.Mock).mockReset();
    });

    it('delivers both ordinary directory responses without stale snapshot metadata and clears failure on ordinary selection', async () => {
        const session = leaksStore.sessionStore.activeSession as any;
        const frameworkSession = store.sessionStore.activeSession;
        let resolveFirst!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveFirst = resolve; }))
            .mockResolvedValueOnce({ body: { reset: true, children: [] } });
        const first = addDataPath({ projectName: 'first', projectPath: ['/first'] } as any,
            ProjectAction.ADD_FILE, false, frameworkSession);
        await addDataPath({ projectName: 'second', projectPath: ['/second'] } as any,
            ProjectAction.ADD_FILE, false, frameworkSession);
        // A current-generation inactive cache must be cleared when the late ordinary response
        // is handled, rather than surviving because its earlier snapshot generation rejected it.
        updateLeaksSessionHandler({
            memSnapshotParseAttempt: 2, memSnapshotSourcePaths: ['/second/trace.json'],
            memSnapshotParseFailure: { fileId: '/second/failed.pkl', error: 'cached failure' },
        });
        expect(session.memSnapshotParseFailure).not.toBeNull();
        resolveFirst({ body: { reset: true, children: [] } });
        await first;
        const imports = (connector.send as jest.Mock).mock.calls.map(([message]) => message)
            .filter(message => message.event === 'remote/import');
        expect(imports.map(message => message.body.dataSource.projectName)).toEqual(['second', 'first']);
        expect(imports.every(message => message.body.memSnapshotParseAttempt === undefined)).toBe(true);
        expect(imports.every(message => message.body.memSnapshotParseFailure === undefined)).toBe(true);
        expect(updateProject).toHaveBeenCalledTimes(2);
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotParseFailure).toBeNull();
        expect(session.memSnapshotParseFileId).toBe('');
    });

    it('does not let an ordinary directory response replace a newer pending snapshot', async () => {
        const session = leaksStore.sessionStore.activeSession as any;
        const frameworkSession = store.sessionStore.activeSession;
        let resolveOrdinary!: (value: any) => void;
        let resolveSnapshot!: (value: any) => void;
        (importProject as jest.Mock)
            .mockReturnValueOnce(new Promise(resolve => { resolveOrdinary = resolve; }))
            .mockReturnValueOnce(new Promise(resolve => { resolveSnapshot = resolve; }));
        const ordinary = addDataPath({ projectName: 'ordinary', projectPath: ['/ordinary'] } as any,
            ProjectAction.ADD_FILE, false, frameworkSession);
        const snapshot = addDataPath({ projectName: 'snapshot', projectPath: ['/new/snapshot.pkl'] } as any,
            ProjectAction.ADD_FILE, false, frameworkSession);
        parseLeaksFailHandler({ rankId: '/new/snapshot.pkl', error: 'new snapshot failed' });
        frameworkSession.memSnapshotParseFailure = { fileId: '/new/snapshot.pkl', error: 'new snapshot failed' };
        resolveOrdinary({ body: { reset: true, children: [] } });
        await ordinary;
        expect(session.memSnapshotParseAttempt).toBe(2);
        expect(session.memSnapshotSourcePaths).toEqual(['/new/snapshot.pkl']);
        expect(session.memSnapshotParseFailure?.error).toBe('new snapshot failed');
        expect(session.memSnapshotImportPending).toBe(true);
        resolveSnapshot({ body: { reset: true, children: [] } });
        await snapshot;
    });
});

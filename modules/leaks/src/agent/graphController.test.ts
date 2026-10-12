/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import {
    COMMAND_ERROR_CODES,
    type CommandDefinition,
    type CommandHandler,
    type JsonObject,
} from '@insight/lib/FrontendAgentCommand';
import { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { Session } from '@/entity/session';
import { createMemoryBlockContextKey } from '@/components/blockLoadState';
import {
    workerHoverItem as workerHoverBlock,
    workerSelectItem as workerSelectBlock,
    workerQueryBlocks,
    workerTransform as workerTransformBlock,
} from '@/leaksWorker/blockWorker/worker';
import {
    workerHoverItem as workerHoverState,
    workerSelectItem as workerSelectState,
    workerTransform as workerTransformState,
} from '@/leaksWorker/stateWorker/worker';
import * as RequestUtils from '@/utils/RequestUtils';
import {
    clearMemoryPoolSummary,
    registerMemScopeGraphCommands,
    setMemScopeAgentSession,
    updateMemoryPoolSummary,
} from './graphController';

jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerHoverItem: jest.fn(),
    workerSelectItem: jest.fn(),
    workerQueryBlocks: jest.fn(),
    workerTransform: jest.fn(),
}));

jest.mock('@/leaksWorker/stateWorker/worker', () => ({
    workerHoverItem: jest.fn(),
    workerSelectItem: jest.fn(),
    workerTransform: jest.fn(),
}));

const detailCommands: Array<{ command: string; args: JsonObject; params: JsonObject }> = [
    { command: 'lifecycleGraph.getBlockDetail', args: { blockId: 7 }, params: { type: 'block', id: 7 } },
    { command: 'memoryPool.getEventDetail', args: { eventId: 7 }, params: { type: 'event', id: 7 } },
    {
        command: 'memoryPool.getSegmentDetail', args: { eventId: 7, address: '0x123', stream: -1 },
        params: { type: 'segment', id: -1, eventId: 7, segmentAddress: '0x123', stream: -1 },
    },
];
const contextChanges: Array<{ label: string; change: (session: Session) => void }> = [
    { label: 'active session', change: () => setMemScopeAgentSession(snapshotSession()) },
    { label: 'detach and reattach', change: session => { setMemScopeAgentSession(undefined); setMemScopeAgentSession(session); } },
    { label: 'project hash', change: session => { session.fileHash = 'project-b'; } },
    { label: 'project database', change: session => { session.dbPath = 'project-b.db'; } },
    { label: 'device', change: session => { session.deviceId = '1'; } },
    { label: 'event type', change: session => { session.eventType = 'free'; } },
    { label: 'module', change: session => { session.module = 'leaks'; } },
    { label: 'slice', change: session => { session.selectedSliceIndex = 1; } },
    { label: 'deviceIds identity', change: session => { session.deviceIds = { ...session.deviceIds }; } },
];

describe('MemScope graph commands at the slice-aware rebase stage', () => {
    let session: Session;
    let commands: Map<string, { definition: CommandDefinition; handler: CommandHandler }>;
    let unregister: () => void;
    let request: jest.Mock;
    let previousRequest: typeof window.request;
    const queryWorker = workerQueryBlocks as jest.MockedFunction<typeof workerQueryBlocks>;
    const invoke = async (name: string, args: JsonObject = {}): Promise<JsonObject> => {
        const command = commands.get(`MemScope.${name}`);
        if (!command) throw new Error(`Missing command ${name}`);
        return await command.handler(args, {
            requestId: 'graph-fixture', deadline: Date.now() + 1000, signal: new AbortController().signal,
        }) as JsonObject;
    };

    beforeEach(() => {
        jest.clearAllMocks();
        session = snapshotSession();
        setMemScopeAgentSession(session);
        commands = new Map();
        // Exercise the real client registration/schema validation without starting transport or services.
        const client = new ModuleAgentCommandClient({ moduleId: 'MemScope', observe: () => ({}) });
        const register = client.registerCommand.bind(client);
        jest.spyOn(client, 'registerCommand').mockImplementation((definition, handler) => {
            commands.set(definition.name, { definition, handler });
            const stop = register(definition, handler);
            return () => { commands.delete(definition.name); stop(); };
        });
        unregister = registerMemScopeGraphCommands(client);
        previousRequest = window.request;
        request = jest.fn().mockResolvedValue({ events: [1, 2, 3] });
        window.request = request;
        queryWorker.mockResolvedValue(queryResult());
    });

    afterEach(() => {
        unregister();
        setMemScopeAgentSession(undefined);
        clearMemoryPoolSummary();
        window.request = previousRequest;
        jest.restoreAllMocks();
    });

    test('retains historical commands alongside visible range, input limits, and unregister ownership', () => {
        expect([...commands.keys()]).toEqual([
            'MemScope.lifecycleGraph.getSummary', 'MemScope.lifecycleGraph.getVisibleRange',
            'MemScope.lifecycleGraph.selectBlock',
            'MemScope.lifecycleGraph.getBlockDetail', 'MemScope.lifecycleGraph.queryBlocks',
            'MemScope.lifecycleGraph.resetView', 'MemScope.memoryPool.getSummary',
            'MemScope.memoryPool.selectEvent', 'MemScope.memoryPool.getEventDetail',
            'MemScope.memoryPool.getSegmentDetail', 'MemScope.memoryPool.clearSelection',
            'MemScope.memoryPool.resetView',
        ]);
        // The eleven historical commands remain, with visible range registered after getSummary.
        expect(commands.size).toBe(12);
        expect(commands.get('MemScope.lifecycleGraph.queryBlocks')?.definition.inputSchema).toMatchObject({
            properties: { limit: { minimum: 1, maximum: 20 } }, additionalProperties: false,
        });
        unregister();
        expect(commands.size).toBe(0);
    });

    describe.each(detailCommands)('$command', ({ command, args, params }) => {
        test.each([0, 1])('sends selected slice %s (including the non-final slice) and bounded detail', async sliceIndex => {
            runInAction(() => { session.selectedSliceIndex = sliceIndex; });
            expect(await invoke(command, { ...args, maxItems: 1 })).toMatchObject({
                detail: { events: [1, '[2 more items]'] }, limits: { maxArrayItems: 1 },
            });
            expect(request).toHaveBeenCalledWith({
                command: 'Memory/snapshot/detail', params: { ...params, deviceId: '0', sliceIndex },
            });
        });

        test.each([-1, 2])('rejects unavailable current slice %s before dispatch', async sliceIndex => {
            runInAction(() => { session.selectedSliceIndex = sliceIndex; });
            await expect(invoke(command, args)).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE, retryable: true });
            expect(request).not.toHaveBeenCalled();
        });

        test.each(contextChanges)('discards detail after late $label change', async ({ change }) => {
            const pending = deferred<unknown>();
            request.mockReturnValueOnce(pending.promise);
            const result = invoke(command, args);
            runInAction(() => change(session));
            pending.resolve({ secretFromOldWindow: true });
            await expect(result).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.CANCELLED, retryable: true, details: { reason: 'context_changed' },
            });
        });

        test('rejects readiness lost while awaiting the detail', async () => {
            const pending = deferred<unknown>();
            request.mockReturnValueOnce(pending.promise);
            const result = invoke(command, args);
            runInAction(() => { session.snapshotSlices['0'].slices[0].ready = false; });
            pending.resolve({ oldDetail: true });
            await expect(result).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
        });

        test('preserves a current backend failure but rejects a stale failure as cancelled', async () => {
            request.mockRejectedValueOnce(new Error('backend unavailable'));
            await expect(invoke(command, args)).rejects.toThrow('backend unavailable');
            const pending = deferred<unknown>();
            request.mockReturnValueOnce(pending.promise);
            const result = invoke(command, args);
            runInAction(() => { session.selectedSliceIndex = 1; });
            pending.reject(new Error('old backend failure'));
            await expect(result).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.CANCELLED });
        });
    });

    test.each([0, 7, 99, 100, 199])('locates ready event %s using the target slice helper', async eventId => {
        runInAction(() => { session.selectedSliceIndex = 1; });
        const sliceIndex = eventId < 100 ? 0 : 1;
        expect(await invoke('memoryPool.selectEvent', { eventId })).toEqual({ accepted: true, eventId, deviceId: '0', sliceIndex });
        expect(session.selectedSliceIndex).toBe(sliceIndex);
        expect(session.pendingEventLocate).toEqual({ eventId, deviceId: '0' });
        // Pending is handed to the existing UI locate consumer, now in the correct ready window.
    });

    test('can locate a ready window while the current window is not ready', async () => {
        runInAction(() => { session.selectedSliceIndex = 2; });
        await invoke('memoryPool.selectEvent', { eventId: 7 });
        expect(session.selectedSliceIndex).toBe(0);
    });

    test.each([
        { eventId: -1, code: COMMAND_ERROR_CODES.INVALID, retryable: false },
        { eventId: 300, code: COMMAND_ERROR_CODES.NOT_FOUND, retryable: false },
        { eventId: 200, code: COMMAND_ERROR_CODES.UNAVAILABLE, retryable: true },
    ])('fails event $eventId explicitly rather than leaving an accepted pending locate', async ({ eventId, code, retryable }) => {
        await expect(invoke('memoryPool.selectEvent', { eventId })).rejects.toMatchObject({ code, retryable });
        expect(session.pendingEventLocate).toBeNull();
        expect(session.selectedSliceIndex).toBe(0);
    });

    test.each(['no device', 'live module', 'no session'])('rejects selectEvent with %s', async context => {
        runInAction(() => {
            if (context === 'no device') session.deviceId = '';
            if (context === 'live module') session.module = 'leaks';
            if (context === 'no session') setMemScopeAgentSession(undefined);
        });
        await expect(invoke('memoryPool.selectEvent', { eventId: 7 })).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
    });

    test('queryBlocks strips worker paths and enforces at most 20 bounded summaries', async () => {
        queryWorker.mockResolvedValueOnce({
            ...queryResult(), blocks: Array.from({ length: 30 }, (_, id) => ({ ...queryResult().blocks[0], id, path: [1, 2, 3] })),
        });
        const result = await invoke('lifecycleGraph.queryBlocks', { sortBy: 'size', limit: 20 });
        expect(result.blocks).toHaveLength(20);
        expect((result.blocks as JsonObject[])[0]).not.toHaveProperty('path');
        expect(queryWorker).toHaveBeenCalledWith({ sortBy: 'size', limit: 20, address: undefined, minSize: undefined, minLifetime: undefined });
        await expect(invoke('lifecycleGraph.queryBlocks', { sortBy: 'size', limit: 21 }))
            .rejects.toMatchObject({ code: COMMAND_ERROR_CODES.INVALID });
        expect(queryWorker).toHaveBeenCalledTimes(1);
    });

    test.each(['old graph context', 'loading graph', 'unready slice', 'no device'])('does not query an %s', async reason => {
        runInAction(() => {
            if (reason === 'old graph context') session.selectedSliceIndex = 1;
            if (reason === 'loading graph') session.loadingBlocks = true;
            if (reason === 'unready slice') session.snapshotSlices['0'].slices[0].ready = false;
            if (reason === 'no device') session.deviceId = '';
        });
        expect(await invoke('lifecycleGraph.getSummary')).toMatchObject({ ready: false });
        await expect(invoke('lifecycleGraph.queryBlocks', { sortBy: 'lifetime' })).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
        expect(queryWorker).not.toHaveBeenCalled();
    });

    test.each(contextChanges)('discards late worker candidates after $label changes even when the new graph is loaded', async ({ change }) => {
        const pending = deferred<BlockQueryResult>();
        queryWorker.mockReturnValueOnce(pending.promise);
        const result = invoke('lifecycleGraph.queryBlocks', { sortBy: 'size' });
        runInAction(() => { change(session); session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session); });
        pending.resolve(queryResult());
        await expect(result).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.CANCELLED, retryable: true });
    });

    test.each(['loading', 'cache context', 'slice readiness'])('rejects queryBlocks when %s is invalidated while awaiting', async reason => {
        const pending = deferred<BlockQueryResult>();
        queryWorker.mockReturnValueOnce(pending.promise);
        const result = invoke('lifecycleGraph.queryBlocks', { sortBy: 'size' });
        runInAction(() => {
            if (reason === 'loading') session.loadingBlocks = true;
            if (reason === 'cache context') session.loadedMemoryBlockContextKey = '';
            if (reason === 'slice readiness') session.snapshotSlices['0'].slices[0].ready = false;
        });
        pending.resolve(queryResult());
        await expect(result).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
    });

    test('live queries also require the graph to belong to the current context', async () => {
        runInAction(() => { session.module = 'leaks'; });
        await expect(invoke('lifecycleGraph.queryBlocks', { sortBy: 'size' })).rejects.toMatchObject({ code: COMMAND_ERROR_CODES.UNAVAILABLE });
        runInAction(() => { session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session); });
        expect(await invoke('lifecycleGraph.queryBlocks', { sortBy: 'size' })).toMatchObject({ limit: 10, blocks: queryResult().blocks });
    });

    test('publishes bounded pool aggregates and clears them explicitly', async () => {
        updateMemoryPoolSummary(7, []);
        runInAction(() => { session.stateWorkerInfo.eventId = 7; });
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: { eventId: 7, segmentCount: 0 } });
        clearMemoryPoolSummary();
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: null });
    });

    test.each(contextChanges)('clears an aggregate on $label change', async ({ change }) => {
        updateMemoryPoolSummary(7, []);
        runInAction(() => { session.stateWorkerInfo.eventId = 7; change(session); });
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: null });
    });

    test.each(['wrong slice event', 'unready', 'selected event changed'])('never displays a pool aggregate with %s', async reason => {
        runInAction(() => { session.stateWorkerInfo.eventId = 7; });
        updateMemoryPoolSummary(reason === 'wrong slice event' ? 100 : 7, []);
        runInAction(() => {
            if (reason === 'unready') session.snapshotSlices['0'].slices[0].ready = false;
            if (reason === 'selected event changed') session.stateWorkerInfo.eventId = 8;
        });
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: null });
    });

    test('an invalidated project aggregate cannot reappear after switching back', async () => {
        updateMemoryPoolSummary(7, []);
        runInAction(() => { session.stateWorkerInfo.eventId = 7; session.fileHash = 'project-b'; });
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: null });
        runInAction(() => { session.fileHash = 'project-a'; });
        expect(await invoke('memoryPool.getSummary')).toMatchObject({ aggregate: null });
    });
});

const snapshotSession = (): Session => {
    const session = new Session();
    runInAction(() => {
        session.module = 'memsnapshot';
        session.deviceId = '0';
        session.deviceIds = { '0': {}, '1': {} };
        session.eventType = 'malloc';
        session.fileHash = 'project-a';
        session.dbPath = 'project-a.db';
        session.selectedSliceIndex = 0;
        session.snapshotSlices = Object.fromEntries(['0', '1'].map(deviceId => [deviceId, {
            eventCount: 300, sliceCount: 3, readySlices: [0, 1],
            slices: [
                { index: 0, startEventId: 0, endEventId: 99, ready: true },
                { index: 1, startEventId: 100, endEventId: 199, ready: true },
                { index: 2, startEventId: 200, endEventId: 299, ready: false },
            ],
        }]));
        session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
    });
    return session;
};

const queryResult = (): BlockQueryResult => ({
    blocks: [{ id: 7, address: '0x123', startTimestamp: 1, endTimestamp: 3, lifetime: 2, sizeBytes: 128 }],
    scannedEntries: 1, matchedEntries: 1, limit: 10, sortBy: 'size',
});
const deferred = <T,>(): { promise: Promise<T>; resolve: (value: T) => void; reject: (error: Error) => void } => {
    let resolve!: (value: T) => void;
    let reject!: (error: Error) => void;
    const promise = new Promise<T>((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
};

describe('MemScope lifecycle graph visible range', () => {
    const handlers = new Map<string, CommandHandler>();
    const definitions = new Map<string, CommandDefinition>();
    const client = {
        registerCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
            definitions.set(definition.name, definition);
            handlers.set(definition.name, handler);
            return () => {
                definitions.delete(definition.name);
                handlers.delete(definition.name);
            };
        },
    } as unknown as ModuleAgentCommandClient;

    const invoke = (name: string, args: JsonObject = {}): unknown => handlers.get(name)?.(args, {
        requestId: 'request-1',
        deadline: Date.now() + 1000,
        signal: new AbortController().signal,
    });

    const snapshotSession = (session: Session): Record<string, unknown> => ({
        module: session.module,
        minTime: session.minTime,
        maxTime: session.maxTime,
        clickItem: session.leaksWorkerInfo.clickItem,
        clickEventItem: session.clickEventItem,
        pendingBlockLocateId: session.pendingBlockLocateId,
        selectionVersion: session.selectionVersion,
        progressiveTotalEventCount: session.progressiveTotalEventCount,
        transform: { ...session.leaksWorkerInfo.renderOptions.transform },
        zoom: { ...session.leaksWorkerInfo.renderOptions.zoom },
        viewport: { ...session.leaksWorkerInfo.renderOptions.viewport },
    });

    const createSession = (module: Session['module']): Session => {
        const session = new Session();
        runInAction(() => {
            session.module = module;
            session.deviceId = 'device-0';
            session.deviceIds = { 'device-0': {} };
            if (module === 'memsnapshot') {
                session.selectedSliceIndex = 0;
                session.snapshotSlices = {
                    'device-0': {
                        eventCount: 101, sliceCount: 1, readySlices: [0],
                        slices: [{ index: 0, startEventId: 0, endEventId: 100, ready: true }],
                    },
                };
            }
            session.minTime = 0;
            session.maxTime = 100;
            session.leaksWorkerInfo.sizeInfo = {
                minTimestamp: 0,
                maxTimestamp: 100,
                minSize: 0,
                maxSize: 10,
            };
            session.allocationData = {
                allocations: [{ id: 1, timestamp: 0, totalSize: 4 }],
                minTimestamp: 0,
                maxTimestamp: 100,
            };
            session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
            session.leaksWorkerInfo.renderOptions = {
                transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
                viewport: { width: 100, height: 40 },
                zoom: { x: 1, y: 1, offset: 0 },
            };
            // getSummary requires a graph loaded for this module/device/slice context.
            session.loadedMemoryBlockContextKey = createMemoryBlockContextKey(session);
        });
        return session;
    };

    const setWindow = (session: Session, min: number, max: number): void => {
        session.leaksWorkerInfo.renderOptions.viewport = { width: max - min, height: 40 };
        session.leaksWorkerInfo.renderOptions.zoom = { x: 1, y: 1, offset: min };
        session.leaksWorkerInfo.renderOptions.transform = {
            ...session.leaksWorkerInfo.renderOptions.transform,
            x: 0,
            scaleX: 1,
        };
    };

    let unregister: (() => void) | undefined;
    let previousRequest: typeof window.request;
    let request: jest.Mock;
    let snapshotDetail: jest.SpyInstance;

    beforeEach(() => {
        jest.clearAllMocks();
        handlers.clear();
        definitions.clear();
        unregister = registerMemScopeGraphCommands(client);
        setMemScopeAgentSession(undefined);
        previousRequest = window.request;
        request = jest.fn().mockResolvedValue({});
        window.request = request;
        // Spy only in this describe; the slice-aware suite exercises real RequestUtils.
        snapshotDetail = jest.spyOn(RequestUtils, 'getSnapshotDetail');
    });

    afterEach(() => {
        unregister?.();
        unregister = undefined;
        setMemScopeAgentSession(undefined);
        window.request = previousRequest;
        jest.restoreAllMocks();
    });

    test('publishes getVisibleRange with an empty object schema and axis semantics', () => {
        const definition = definitions.get('MemScope.lifecycleGraph.getVisibleRange');
        expect(definition).toMatchObject({
            name: 'MemScope.lifecycleGraph.getVisibleRange',
            inputSchema: { type: 'object', properties: {}, required: [], additionalProperties: false },
        });
        expect(definition?.description).toEqual(expect.stringContaining('relative nanoseconds'));
        expect(definition?.description).toEqual(expect.stringContaining('integer event IDs'));
        expect(definition?.description).toEqual(expect.stringContaining('intersecting'));
        expect(definition?.description).toEqual(expect.stringContaining('visibleRange null'));
        expect(definition?.description).toEqual(expect.stringContaining('timestampRange'));
    });

    test('returns available false when no session is active', () => {
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({ available: false });
    });

    test('returns timestamp and eventId ranges for the two modules', () => {
        const leaks = createSession('leaks');
        setWindow(leaks, 1000.25, 2000.75);
        leaks.leaksWorkerInfo.sizeInfo.maxTimestamp = 3000;
        leaks.allocationData.maxTimestamp = 3000;
        setMemScopeAgentSession(leaks);
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'ready',
            visibleRange: { min: 1000.25, max: 2000.75 },
        });

        const snapshot = createSession('memsnapshot');
        setWindow(snapshot, 10.2, 20.8);
        setMemScopeAgentSession(snapshot);
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'eventId',
            status: 'ready',
            visibleRange: { min: 11, max: 20 },
        });
    });

    test('reads the live transform before the debounced session min/max is committed', () => {
        const session = createSession('leaks');
        session.minTime = 0;
        session.maxTime = 100;
        setWindow(session, 10.2, 20.8);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'ready',
            visibleRange: { min: 10.2, max: 20.8 },
        });
        expect(session.minTime).toBe(0);
        expect(session.maxTime).toBe(100);
    });

    test('switches axis and range with the active session and does not cache the previous result', () => {
        const leaks = createSession('leaks');
        setWindow(leaks, 0, 100);
        setMemScopeAgentSession(leaks);
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'ready',
            visibleRange: { min: 0, max: 100 },
        });

        const snapshot = createSession('memsnapshot');
        setWindow(snapshot, 110, 120);
        setMemScopeAgentSession(snapshot);
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'eventId',
            status: 'empty',
            visibleRange: null,
        });
    });

    test('keeps the X range unchanged when only Y transform values change', () => {
        const session = createSession('leaks');
        setWindow(session, 20, 40);
        setMemScopeAgentSession(session);
        const first = invoke('MemScope.lifecycleGraph.getVisibleRange');
        session.leaksWorkerInfo.renderOptions.transform.y = 80;
        session.leaksWorkerInfo.renderOptions.transform.scaleY = 3;
        session.leaksWorkerInfo.renderOptions.zoom.y = 9;
        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual(first);
    });

    test('is read-only and does not call workers or snapshot detail APIs', () => {
        const session = createSession('memsnapshot');
        setWindow(session, 10, 20);
        setMemScopeAgentSession(session);
        const before = snapshotSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'eventId',
            status: 'ready',
            visibleRange: { min: 10, max: 20 },
        });

        expect(snapshotSession(session)).toEqual(before);
        expect(workerHoverBlock).not.toHaveBeenCalled();
        expect(workerSelectBlock).not.toHaveBeenCalled();
        expect(workerQueryBlocks).not.toHaveBeenCalled();
        expect(workerTransformBlock).not.toHaveBeenCalled();
        expect(workerHoverState).not.toHaveBeenCalled();
        expect(workerSelectState).not.toHaveBeenCalled();
        expect(workerTransformState).not.toHaveBeenCalled();
        expect(snapshotDetail).not.toHaveBeenCalled();
        expect(request).not.toHaveBeenCalled();
    });

    test('unregisters getVisibleRange with the other graph commands', () => {
        expect(definitions.has('MemScope.lifecycleGraph.getVisibleRange')).toBe(true);
        unregister?.();
        unregister = undefined;
        expect(definitions.has('MemScope.lifecycleGraph.getVisibleRange')).toBe(false);
        expect(definitions.has('MemScope.lifecycleGraph.getSummary')).toBe(false);
    });

    test('returns ready when only a strict block domain marks the graph loaded', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 0;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 100;
        session.allocationData = { allocations: [], minTimestamp: 0, maxTimestamp: 0 };
        session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
        session.progressiveTotalEventCount = 0;
        setWindow(session, 0, 100);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'ready',
            visibleRange: { min: 0, max: 100 },
        });
    });

    test('returns ready when only progressiveTotalEventCount marks the graph loaded', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 0;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 0;
        session.allocationData = { allocations: [], minTimestamp: 0, maxTimestamp: 0 };
        session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
        session.progressiveTotalEventCount = 1;
        setWindow(session, 0, 100);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'ready',
            visibleRange: { min: 0, max: 0 },
        });
    });

    test('returns unavailable for a single-point sizeInfo with no loaded records', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 42;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 42;
        session.allocationData = { allocations: [], minTimestamp: 0, maxTimestamp: 0 };
        session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
        session.progressiveTotalEventCount = 0;
        setWindow(session, 40, 50);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'unavailable',
            visibleRange: null,
        });
    });

    test('returns unavailable when only allocations exist before worker sizeInfo is initialized', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 0;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 0;
        session.allocationData = {
            allocations: [{ id: 1, timestamp: 1_000_000_000, totalSize: 4 }],
            minTimestamp: 1_000_000_000,
            maxTimestamp: 2_000_000_000,
        };
        session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
        session.progressiveTotalEventCount = 0;
        setWindow(session, 0, 800);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'unavailable',
            visibleRange: null,
        });
    });

    test('returns unavailable when snapshot allocations exist before worker sizeInfo is initialized', () => {
        const session = createSession('memsnapshot');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 0;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 0;
        session.allocationData = {
            allocations: [{ id: 1, timestamp: 0, totalSize: 4 }],
            minTimestamp: 0,
            maxTimestamp: 50,
        };
        session.funcData = { traces: [], minTimestamp: 0, maxTimestamp: 0 };
        session.progressiveTotalEventCount = 0;
        setWindow(session, 0, 800);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'eventId',
            status: 'unavailable',
            visibleRange: null,
        });
    });

    test('returns unavailable when allocation records exist but timestamps are not finite', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.sizeInfo.minTimestamp = 0;
        session.leaksWorkerInfo.sizeInfo.maxTimestamp = 100;
        session.allocationData = {
            allocations: [{ id: 1, timestamp: 10, totalSize: 4 }],
            minTimestamp: Number.NaN,
            maxTimestamp: Number.NaN,
        };
        setWindow(session, 0, 100);
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getVisibleRange')).toEqual({
            available: true,
            axis: 'timestamp',
            status: 'unavailable',
            visibleRange: null,
        });
    });

    test('keeps the getSummary contract unchanged', () => {
        const session = createSession('leaks');
        session.leaksWorkerInfo.clickItem = {
            id: 7,
            addr: '0x1',
            _startTimestamp: 1,
            _endTimestamp: 9,
            size: 32,
            path: [],
        };
        setMemScopeAgentSession(session);

        expect(invoke('MemScope.lifecycleGraph.getSummary')).toEqual({
            available: true,
            ready: true,
            timestampRange: { min: 0, max: 100 },
            sizeRangeBytes: { min: 0, max: 10 },
            reservedSizeMaxBytes: 0,
            build: null,
            selectedBlock: {
                id: 7,
                address: '0x1',
                startTimestamp: 1,
                endTimestamp: 9,
                lifetime: 8,
                sizeBytes: 32,
            },
            view: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
        });
    });
});

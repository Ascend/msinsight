/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { Session } from '@/entity/session';
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
import { getSnapshotDetail } from '@/utils/RequestUtils';
import { registerMemScopeGraphCommands, setMemScopeAgentSession } from './graphController';

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

jest.mock('@/utils/RequestUtils', () => ({
    getSnapshotDetail: jest.fn(),
}));

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
    session.module = module;
    session.deviceId = 'device-0';
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

beforeEach(() => {
    handlers.clear();
    definitions.clear();
    unregister = registerMemScopeGraphCommands(client);
    setMemScopeAgentSession(undefined);
});

afterEach(() => {
    unregister?.();
    unregister = undefined;
    setMemScopeAgentSession(undefined);
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
    expect(getSnapshotDetail).not.toHaveBeenCalled();
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

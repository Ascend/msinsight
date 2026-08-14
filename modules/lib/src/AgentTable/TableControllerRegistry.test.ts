/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { TableControllerRegistry } from './TableControllerRegistry';
import { TABLE_ERROR_CODES } from './errors';
import type {
    TableCommandRequest,
    TableController,
    TableStableSnapshot,
    TransitionContext,
} from './types';

const snapshot = (page = 1): TableStableSnapshot => ({
    state: {
        query: { filters: [], sort: null, page, pageSize: 10 },
        total: 20,
        rowCount: 10,
        selectedRowIds: [],
        expandedRowIds: [],
    },
    columns: [{ columnId: 'id', title: 'ID', dataType: 'number', readable: true, sortable: true }],
    capabilities: ['table.goToPage'],
    dataAccess: { maxRowsPerRequest: 10, availableRows: 10 },
});

const commandRequest = (targetId: string, requestId: string, expectedRevision = 1): TableCommandRequest => ({
    targetId,
    expectedRevision,
    commandId: 'table.goToPage',
    args: { page: 2 },
    requestId,
    deadline: Date.now() + 5000,
});

describe('TableControllerRegistry', () => {
    beforeAll(() => {
        Object.defineProperty(globalThis, 'crypto', {
            configurable: true,
            value: { randomUUID: jest.fn(() => '00000000-0000-4000-8000-000000000001') },
        });
    });

    test('observes visible controllers including transient readiness state', () => {
        const registry = new TableControllerRegistry();
        registry.register(controller({ visible: false, ready: true, busy: false }));
        registry.register(controller({ visible: true, ready: false, busy: false }));
        registry.register(controller({ visible: true, ready: true, busy: false }));

        expect(registry.observe()).toHaveLength(2);
        expect(registry.observe().map(({ availability }) => availability.ready)).toEqual([false, true]);
    });

    test('notifies command subscribers when controllers register and unregister', () => {
        const registry = new TableControllerRegistry();
        const listener = jest.fn();
        registry.subscribeCommandsChanged(listener);

        const registration = registry.register(controller());
        registration.unregister();

        expect(listener).toHaveBeenCalledTimes(2);
    });

    test('advances revision only after a stable snapshot changes', () => {
        let current = snapshot();
        let listener: ((next: TableStableSnapshot) => void) | undefined;
        const registry = new TableControllerRegistry();
        registry.register(controller(
            { visible: true, ready: true, busy: false },
            {
                getSnapshot: () => current,
                subscribeStable: (next) => {
                    listener = next;
                    return () => {};
                },
            },
        ));

        expect(registry.observe()[0].revision).toBe(1);
        listener?.(current);
        expect(registry.observe()[0].revision).toBe(1);
        current = snapshot(2);
        listener?.(current);
        expect(registry.observe()[0].revision).toBe(2);
    });

    test('rejects a stale expected revision before execution', async () => {
        const execute = jest.fn();
        const registry = new TableControllerRegistry();
        const registration = registry.register(controller(
            { visible: true, ready: true, busy: false },
            { execute },
        ));

        await expect(registry.invoke(commandRequest(registration.targetId, 'request-1', 2)))
            .rejects.toMatchObject({ code: TABLE_ERROR_CODES.STATE_STALE });
        expect(execute).not.toHaveBeenCalled();
    });

    test('serializes requests for the same target', async () => {
        const first = deferred<void>();
        const order: string[] = [];
        let current = snapshot();
        const execute = jest.fn(async (request: TableCommandRequest) => {
            order.push(`start:${request.requestId}`);
            if (request.requestId === 'request-1') await first.promise;
            order.push(`end:${request.requestId}`);
            current = snapshot(2);
            return { snapshot: current, changed: true };
        });
        const registry = new TableControllerRegistry();
        const registration = registry.register(controller(
            { visible: true, ready: true, busy: false },
            { getSnapshot: () => current, execute },
        ));

        const request1 = registry.invoke(commandRequest(registration.targetId, 'request-1'));
        const request2 = registry.invoke(commandRequest(registration.targetId, 'request-2', 2));
        await Promise.resolve();
        expect(order).toEqual(['start:request-1']);
        first.resolve(undefined);
        await Promise.all([request1, request2]);
        expect(order).toEqual(['start:request-1', 'end:request-1', 'start:request-2', 'end:request-2']);
    });

    test('cancels an active request through its transition signal', async () => {
        let transitionSignal: AbortSignal | undefined;
        const execute = jest.fn(async (_request: TableCommandRequest, context: TransitionContext) => {
            transitionSignal = context.signal;
            await new Promise<void>((_resolve, reject) => {
                context.signal.addEventListener('abort', () => reject(context.signal.reason), { once: true });
            });
            return { snapshot: snapshot(2), changed: true };
        });
        const cancel = jest.fn();
        const registry = new TableControllerRegistry();
        const registration = registry.register(controller(
            { visible: true, ready: true, busy: false },
            { execute, cancel },
        ));
        const pending = registry.invoke(commandRequest(registration.targetId, 'request-1'));
        await Promise.resolve();

        registry.cancel('request-1');

        expect(transitionSignal?.aborted).toBe(true);
        expect(cancel).toHaveBeenCalledWith('request-1');
        await expect(pending.then(
            () => 'resolved',
            () => 'rejected',
        )).resolves.toBe('rejected');
    });

    test('cancels a queued request before it executes', async () => {
        const first = deferred<void>();
        let current = snapshot();
        const execute = jest.fn(async (request: TableCommandRequest) => {
            if (request.requestId === 'request-1') await first.promise;
            current = snapshot(2);
            return { snapshot: current, changed: true };
        });
        const registry = new TableControllerRegistry();
        const registration = registry.register(controller(
            { visible: true, ready: true, busy: false },
            { getSnapshot: () => current, execute },
        ));

        const request1 = registry.invoke(commandRequest(registration.targetId, 'request-1'));
        const request2 = registry.invoke(commandRequest(registration.targetId, 'request-2', 2));
        registry.cancel('request-2');
        first.resolve(undefined);

        await request1;
        await expect(request2).rejects.toMatchObject({ code: TABLE_ERROR_CODES.COMMAND_CANCELLED });
        expect(execute).toHaveBeenCalledTimes(1);
    });
});

const controller = (
    availability = { visible: true, ready: true, busy: false },
    overrides: Partial<TableController> = {},
): TableController => ({
    tableKey: 'memscope.system.blocks',
    title: 'Block View',
    getAvailability: () => availability,
    getSnapshot: () => snapshot(),
    execute: async (_request: TableCommandRequest, _context: TransitionContext) => ({ snapshot: snapshot(2), changed: true }),
    ...overrides,
});

const deferred = <T,>(): { promise: Promise<T>; resolve: (value: T) => void } => {
    let resolve!: (value: T) => void;
    const promise = new Promise<T>((resolvePromise) => { resolve = resolvePromise; });
    return { promise, resolve };
};

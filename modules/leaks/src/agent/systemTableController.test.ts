/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { TABLE_ERROR_CODES, TableControllerRegistry } from '@insight/lib/AgentTable';
import type { TableCommandRequest, TableQueryState, TransitionContext } from '@insight/lib/AgentTable';
import {
    createMemScopeSystemTableController,
    filtersToAgentQuery,
    queryToBusinessFilters,
    requireMemScopeTableData,
    type MemScopeSystemTableView,
} from './systemTableController';

const query: TableQueryState = { filters: [], sort: null, page: 1, pageSize: 10 };
const view = (): MemScopeSystemTableView => ({
    columns: [
        { key: 'name', name: 'Name', sortable: true, searchable: true },
        { key: 'size', name: 'Size', sortable: true, rangeFilterable: true },
    ],
    rows: [{ id: 1, name: 'block', size: 1024 }],
    query,
    total: 1,
    busy: false,
    visible: true,
    ready: true,
});

describe('MemScope system table controller', () => {
    beforeEach(() => {
        let sequence = 0;
        Object.defineProperty(globalThis, 'crypto', {
            configurable: true,
            value: { randomUUID: () => `00000000-0000-4000-8000-${String(++sequence).padStart(12, '0')}` },
        });
    });

    test('Registry exposes visible tables with their current readiness', () => {
        const hiddenRegistry = new TableControllerRegistry();
        hiddenRegistry.register(controllerFor({ ...view(), visible: false }));
        expect(hiddenRegistry.observe()).toEqual([]);

        const unreadyRegistry = new TableControllerRegistry();
        unreadyRegistry.register(controllerFor({ ...view(), ready: false }));
        expect(unreadyRegistry.observe()).toEqual([
            expect.objectContaining({ availability: { visible: true, ready: false, busy: false } }),
        ]);

        const visibleRegistry = new TableControllerRegistry();
        visibleRegistry.register(controllerFor(view()));
        expect(visibleRegistry.observe()).toEqual([
            expect.objectContaining({ tableKey: 'memscope.system.blocks', availability: { visible: true, ready: true, busy: false } }),
        ]);
    });

    test('includes an empty context key for legacy views', () => {
        expect(controllerFor(view()).getSnapshot().state.contextKey).toBe('');
    });

    test.each(['project-b/device-0/slice-0', 'project-a/device-1/slice-0', 'project-a/device-0/slice-1'])(
        'rejects the old revision after only the table context changes to %s', async nextContext => {
            const currentView = { ...view(), contextKey: 'project-a/device-0/slice-0' };
            const registry = new TableControllerRegistry();
            const controller = controllerFor(currentView);
            const execute = jest.spyOn(controller, 'execute');
            const { targetId } = registry.register(controller);
            const previous = registry.observe()[0];
            expect(previous.state.contextKey).toBe(currentView.contextKey);
            currentView.contextKey = nextContext;
            await expect(registry.invoke({
                ...request('table.refresh'),
                targetId,
                expectedRevision: previous.revision,
            })).rejects.toMatchObject({ code: TABLE_ERROR_CODES.STATE_STALE });
            expect(execute).not.toHaveBeenCalled();
            const current = registry.observe()[0];
            expect(current.revision).toBe(previous.revision + 1);
            expect(current.state.contextKey).toBe(nextContext);
            registry.dispose();
        },
    );

    test('Registry rejects an unready table without executing a transition', async () => {
        const registry = new TableControllerRegistry();
        const controller = controllerFor({ ...view(), ready: false });
        const execute = jest.spyOn(controller, 'execute');
        const { targetId } = registry.register(controller);
        await expect(registry.invoke({ ...request('table.refresh'), targetId }))
            .rejects.toMatchObject({ code: TABLE_ERROR_CODES.NOT_READY });
        expect(execute).not.toHaveBeenCalled();
        registry.dispose();
    });

    test('maps UI filters to deterministic Agent filters', () => {
        expect(filtersToAgentQuery({
            size: ['10', '20'],
            name: ['tensor'],
            empty: null,
        }, view().columns)).toEqual([
            { columnId: 'name', operator: 'contains', value: 'tensor' },
            { columnId: 'size', operator: 'between', value: { min: 10, max: 20 } },
        ]);
    });

    test('separates Agent contains and between filters for the business request', () => {
        expect(queryToBusinessFilters({
            ...query,
            filters: [
                { columnId: 'name', operator: 'contains', value: 'tensor' },
                { columnId: 'size', operator: 'between', value: { min: 10, max: 20 } },
            ],
        })).toEqual({
            filters: { name: 'tensor' },
            rangeFilters: { size: [10, 20] },
        });
    });

    test('exposes real columns, capabilities, and displayed data', async () => {
        const controller = controllerFor(view());
        const snapshot = controller.getSnapshot();

        expect(snapshot.columns).toEqual([
            expect.objectContaining({ columnId: 'name', dataType: 'string', filterOperators: ['contains'] }),
            expect.objectContaining({ columnId: 'size', dataType: 'number', filterOperators: ['between'] }),
        ]);
        expect(snapshot.capabilities).toEqual(expect.arrayContaining([
            'table.setFilters',
            'table.setSort',
            'table.getDisplayedData',
            'table.copy',
        ]));

        const result = await controller.execute(request('table.getDisplayedData', { offset: 0, limit: 1 }), context());
        expect(result.result).toMatchObject({
            rows: [{ rowId: '1', cells: { name: { value: 'block' }, size: { value: 1024 } } }],
            returned: 1,
        });
    });

    test('supports uppercase row IDs returned by Snapshot tables', async () => {
        const controller = controllerFor({ ...view(), rows: [{ ID: 2, name: 'snapshot block', size: 2048 }] });

        const result = await controller.execute(request('table.getDisplayedData', { offset: 0, limit: 1 }), context());

        expect(result.result).toMatchObject({ rows: [{ rowId: '2' }] });
    });

    test('rejects displayed data when row IDs are empty or duplicated', async () => {
        const duplicateView = { ...view(), rows: [{ id: 1 }, { id: 1 }] };
        const controller = controllerFor(duplicateView);

        await expect(controller.execute(request('table.getDisplayedData', { limit: 2 }), context()))
            .rejects.toMatchObject({ code: TABLE_ERROR_CODES.ROW_UNKNOWN });
    });
});

describe.each(['blocks', 'events'] as const)('MemScope %s response validation', rowsKey => {
    test('returns the original valid response including a successful empty table', () => {
        const response = {
            headers: [{ key: 'id', name: 'ID' }],
            [rowsKey]: [{ id: 0 }],
            total: 1,
        };
        expect(requireMemScopeTableData(response, true, rowsKey)).toBe(response);
        const empty = { headers: [], [rowsKey]: [], total: 0 };
        expect(requireMemScopeTableData(empty, true, rowsKey)).toBe(empty);
    });

    test.each([
        { ready: false, code: TABLE_ERROR_CODES.NOT_READY },
        { ready: true, code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED },
    ])('rejects undefined with $code when ready=$ready', ({ ready, code }) => {
        expect(() => requireMemScopeTableData(undefined, ready, rowsKey))
            .toThrow(expect.objectContaining({ code, retryable: true }));
    });

    test.each([
        { label: 'null payload', payload: null },
        { label: 'missing headers', payload: { total: 0 } },
        { label: 'non-array headers', payload: { headers: {}, total: 0 } },
        { label: 'null header', payload: { headers: [null], total: 0 } },
        { label: 'missing header key', payload: { headers: [{ name: 'ID' }], total: 0 } },
        { label: 'blank header key', payload: { headers: [{ key: ' ', name: 'ID' }], total: 0 } },
        { label: 'missing header name', payload: { headers: [{ key: 'id' }], total: 0 } },
        { label: 'blank header name', payload: { headers: [{ key: 'id', name: ' ' }], total: 0 } },
        { label: 'string total', payload: { headers: [], total: '0' } },
        { label: 'missing total', payload: { headers: [] } },
        { label: 'negative total', payload: { headers: [], total: -1 } },
        { label: 'fractional total', payload: { headers: [], total: 0.5 } },
        { label: 'unsafe total', payload: { headers: [], total: Number.MAX_SAFE_INTEGER + 1 } },
        { label: 'NaN total', payload: { headers: [], total: NaN } },
        { label: 'infinite total', payload: { headers: [], total: Infinity } },
    ])('rejects $label with a typed transition error', ({ payload }) => {
        const invalid = payload === null ? null : { [rowsKey]: [], ...payload };
        expect(() => requireMemScopeTableData(invalid, true, rowsKey))
            .toThrow(expect.objectContaining({ code: TABLE_ERROR_CODES.TRANSITION_FAILED, retryable: false }));
    });

    test.each([undefined, null, {}, 'rows'])('rejects missing or non-array rows (%s)', rows => {
        expect(() => requireMemScopeTableData({ headers: [], [rowsKey]: rows, total: 0 }, true, rowsKey))
            .toThrow(expect.objectContaining({ code: TABLE_ERROR_CODES.TRANSITION_FAILED }));
    });
});

const controllerFor = (currentView: MemScopeSystemTableView) => createMemScopeSystemTableController({
    tableKey: 'memscope.system.blocks',
    title: 'Block View',
    getView: () => currentView,
    runQueryTransition: async () => ({
        state: { query, total: 1, rowCount: 1, selectedRowIds: [], expandedRowIds: [] },
        columns: [],
        capabilities: [],
        dataAccess: { maxRowsPerRequest: 1, availableRows: 1 },
    }),
    copyDisplayedData: async () => ({ rowCount: 1, columnCount: 2 }),
    subscribeStable: () => () => {},
    cancel: () => {},
});

const request = (commandId: TableCommandRequest['commandId'], args = {}): TableCommandRequest => ({
    targetId: 'table-1',
    expectedRevision: 1,
    commandId,
    args,
    requestId: 'request-1',
    deadline: Date.now() + 1000,
});

const context = (): TransitionContext => ({
    requestId: 'request-1',
    transitionId: 'transition-1',
    source: 'agent',
    deadline: Date.now() + 1000,
    signal: new AbortController().signal,
});

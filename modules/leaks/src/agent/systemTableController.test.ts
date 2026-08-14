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
    beforeAll(() => {
        Object.defineProperty(globalThis, 'crypto', {
            configurable: true,
            value: { randomUUID: jest.fn(() => '00000000-0000-4000-8000-000000000001') },
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

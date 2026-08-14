/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { createAgentTableController } from './commands';
import { TABLE_ERROR_CODES } from './errors';
import type {
    TableCommandId,
    TableCommandRequest,
    TableControllerAdapter,
    TableQueryState,
    TableStableSnapshot,
    TransitionContext,
} from './types';

const initialQuery: TableQueryState = {
    filters: [],
    sort: null,
    page: 3,
    pageSize: 10,
};

const snapshot = (query: TableQueryState = initialQuery): TableStableSnapshot => ({
    state: {
        query,
        total: 50,
        rowCount: 10,
        selectedRowIds: [],
        expandedRowIds: [],
    },
    columns: [
        { columnId: 'name', title: 'Name', dataType: 'string', readable: true, sortable: true, filterOperators: ['contains'] },
        { columnId: 'size', title: 'Size', dataType: 'number', readable: true, sortable: true, filterOperators: ['between'] },
    ],
    capabilities: [
        'table.setQuery',
        'table.setFilters',
        'table.setSort',
        'table.getDisplayedData',
    ],
    dataAccess: { maxRowsPerRequest: 5, availableRows: 10 },
});

const context = (): TransitionContext => ({
    requestId: 'request-1',
    transitionId: 'transition-1',
    source: 'agent',
    deadline: Date.now() + 1000,
    signal: new AbortController().signal,
});

const request = (commandId: TableCommandId, args = {}): TableCommandRequest => ({
    targetId: 'table-1',
    expectedRevision: 1,
    commandId,
    args,
    requestId: 'request-1',
    deadline: Date.now() + 1000,
});

describe('AgentTable commands', () => {
    test('setSort resets the page and runs one query transition', async () => {
        let stableSnapshot = snapshot();
        const runQueryTransition = jest.fn(async (query: TableQueryState) => {
            stableSnapshot = snapshot(query);
            return stableSnapshot;
        });
        const controller = createAgentTableController(adapter({
            getSnapshot: () => stableSnapshot,
            runQueryTransition,
        }));

        const result = await controller.execute(request('table.setSort', {
            sort: { columnId: 'size', direction: 'desc' },
        }), context());

        expect(runQueryTransition).toHaveBeenCalledTimes(1);
        expect(runQueryTransition.mock.calls[0][0]).toEqual({
            ...initialQuery,
            page: 1,
            sort: { columnId: 'size', direction: 'desc' },
        });
        expect(result.changed).toBe(true);
    });

    test('returns a no-op without querying when the normalized state is unchanged', async () => {
        const current = snapshot({ ...initialQuery, page: 1 });
        const runQueryTransition = jest.fn();
        const controller = createAgentTableController(adapter({
            getSnapshot: () => current,
            runQueryTransition,
        }));

        const result = await controller.execute(request('table.setQuery', { page: 1 }), context());

        expect(result.changed).toBe(false);
        expect(runQueryTransition).not.toHaveBeenCalled();
    });

    test('rejects an invalid numeric range before querying', async () => {
        const runQueryTransition = jest.fn();
        const controller = createAgentTableController(adapter({ runQueryTransition }));

        await expect(controller.execute(request('table.setFilters', {
            filters: [{ columnId: 'size', operator: 'between', value: { min: 20, max: 10 } }],
        }), context())).rejects.toMatchObject({ code: TABLE_ERROR_CODES.FILTER_VALUE_INVALID });
        expect(runQueryTransition).not.toHaveBeenCalled();
    });

    test('rejects displayed-data reads above the declared limit', async () => {
        const getDisplayedData = jest.fn();
        const controller = createAgentTableController(adapter({ getDisplayedData }));

        await expect(controller.execute(request('table.getDisplayedData', { limit: 6 }), context()))
            .rejects.toMatchObject({ code: TABLE_ERROR_CODES.COMMAND_INVALID });
        expect(getDisplayedData).not.toHaveBeenCalled();
    });
});

const adapter = (overrides: Partial<TableControllerAdapter> = {}): TableControllerAdapter => ({
    tableKey: 'memscope.system.blocks',
    title: 'Block View',
    getAvailability: () => ({ visible: true, ready: true, busy: false }),
    getSnapshot: () => snapshot(),
    runQueryTransition: async (query) => snapshot(query),
    getDisplayedData: async () => ({
        columns: snapshot().columns,
        rows: [],
        offset: 0,
        returned: 0,
        available: 0,
        hasMore: false,
    }),
    ...overrides,
});

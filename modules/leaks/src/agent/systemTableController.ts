/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    AgentTableError,
    TABLE_ERROR_CODES,
    createAgentTableController,
    type DisplayedCell,
    type DisplayedDataResult,
    type JsonObject,
    type JsonValue,
    type TableCommandId,
    type TableColumnDefinition,
    type TableController,
    type TableControllerAdapter,
    type TableFilter,
    type TableQueryState,
    type TableStableSnapshot,
} from '@insight/lib/AgentTable';

export interface MemScopeTableColumn {
    key: string;
    name: string;
    type?: string;
    sortable?: boolean;
    searchable?: boolean;
    rangeFilterable?: boolean;
}

export interface MemScopeSystemTableView {
    columns: MemScopeTableColumn[];
    rows: Array<Record<string, unknown>>;
    query: TableQueryState;
    total: number;
    busy: boolean;
    visible: boolean;
    ready: boolean;
}

export interface MemScopeSystemTableControllerOptions {
    tableKey: string;
    title: string;
    getView: () => MemScopeSystemTableView;
    runQueryTransition: NonNullable<TableControllerAdapter['runQueryTransition']>;
    copyDisplayedData: NonNullable<TableControllerAdapter['copyDisplayedData']>;
    subscribeStable: NonNullable<TableControllerAdapter['subscribeStable']>;
    cancel: NonNullable<TableControllerAdapter['cancel']>;
}

const BASE_CAPABILITIES: TableCommandId[] = [
    'table.setQuery',
    'table.setSort',
    'table.clearSort',
    'table.goToPage',
    'table.setPageSize',
    'table.refresh',
    'table.getDisplayedData',
    'table.copy',
];

export const createMemScopeSystemTableController = (
    options: MemScopeSystemTableControllerOptions,
): TableController => createAgentTableController({
    tableKey: options.tableKey,
    title: options.title,
    getAvailability: () => {
        const view = options.getView();
        return { visible: view.visible, ready: view.ready, busy: view.busy };
    },
    getSnapshot: () => snapshot(options.getView),
    subscribeStable: options.subscribeStable,
    runQueryTransition: options.runQueryTransition,
    getDisplayedData: async (offset, limit) => displayedData(options.getView, offset, limit),
    copyDisplayedData: options.copyDisplayedData,
    cancel: options.cancel,
});

export const filtersToAgentQuery = (
    filters: Record<string, unknown>,
    columns: MemScopeTableColumn[],
): TableFilter[] => {
    const columnMap = new Map(columns.map((column) => [column.key, column]));
    return Object.entries(filters).flatMap(([columnId, raw]) => {
        const values = Array.isArray(raw) ? raw : [];
        if (!values.length) return [];
        const column = columnMap.get(columnId);
        if (column?.rangeFilterable && values.length === 2) {
            return [{
                columnId,
                operator: 'between',
                value: { min: Number(values[0]), max: Number(values[1]) },
            } as TableFilter];
        }
        return [{ columnId, operator: 'contains', value: String(values[0]) } as TableFilter];
    }).sort((left, right) => left.columnId.localeCompare(right.columnId));
};

export const queryToBusinessFilters = (query: TableQueryState): {
    filters: Record<string, string>;
    rangeFilters: Record<string, number[]>;
} => {
    const filters: Record<string, string> = {};
    const rangeFilters: Record<string, number[]> = {};
    query.filters.forEach((filter) => {
        if (filter.operator === 'contains') filters[filter.columnId] = String(filter.value ?? '');
        if (filter.operator === 'between' && isObject(filter.value)) {
            rangeFilters[filter.columnId] = [Number(filter.value.min), Number(filter.value.max)];
        }
    });
    return { filters, rangeFilters };
};

const snapshot = (getView: () => MemScopeSystemTableView): TableStableSnapshot => {
    const view = getView();
    const columns = agentColumns(view.columns);
    const capabilities = [...BASE_CAPABILITIES];
    if (columns.some((column) => column.filterOperators?.length)) {
        capabilities.push('table.setFilters', 'table.clearFilters');
    }
    return {
        state: {
            query: view.query,
            total: view.total,
            rowCount: view.rows.length,
            selectedRowIds: [],
            expandedRowIds: [],
        },
        columns,
        capabilities,
        dataAccess: {
            maxRowsPerRequest: Math.min(100, Math.max(1, view.rows.length)),
            availableRows: view.rows.length,
        },
    };
};

const displayedData = (
    getView: () => MemScopeSystemTableView,
    offset: number,
    limit: number,
): DisplayedDataResult => {
    const view = getView();
    const columns = agentColumns(view.columns);
    const readableColumns = columns.filter((column) => column.readable);
    const rowIds = view.rows.map(rowId);
    if (rowIds.some((id) => id === '') || new Set(rowIds).size !== rowIds.length) {
        throw new AgentTableError({
            code: TABLE_ERROR_CODES.ROW_UNKNOWN,
            message: 'MemScope table rows do not have unique Agent row IDs.',
            retryable: false,
        });
    }
    const rows = view.rows.slice(offset, offset + limit).map((row) => {
        const cells: Record<string, DisplayedCell> = Object.fromEntries(readableColumns.map(({ columnId }) => [columnId, {
            value: toJsonValue(row[columnId]),
        }]));
        return { rowId: rowId(row), cells };
    });
    return {
        columns,
        rows,
        offset,
        returned: rows.length,
        available: view.rows.length,
        hasMore: offset + rows.length < view.rows.length,
    };
};

const agentColumns = (columns: MemScopeTableColumn[]): TableColumnDefinition[] => columns.map((column) => ({
    columnId: column.key,
    title: column.name,
    dataType: column.rangeFilterable ? 'number' : dataType(column.type),
    readable: true,
    sortable: column.sortable ?? false,
    ...(column.searchable ? { filterOperators: ['contains' as const] } : {}),
    ...(column.rangeFilterable ? { filterOperators: ['between' as const] } : {}),
}));

const rowId = (row: Record<string, unknown>): string => String(row.id ?? row.ID ?? '');

const dataType = (type: string | undefined): TableColumnDefinition['dataType'] => {
    const normalized = String(type ?? '').toLowerCase();
    if (normalized.includes('int') || normalized.includes('float') || normalized.includes('double') || normalized.includes('number')) return 'number';
    if (normalized.includes('bool')) return 'boolean';
    return 'string';
};

const toJsonValue = (value: unknown): JsonValue => {
    if (value === null || value === undefined) return null;
    if (['string', 'number', 'boolean'].includes(typeof value)) return value as string | number | boolean;
    if (Array.isArray(value)) return value.map(toJsonValue);
    if (isObject(value)) return Object.fromEntries(Object.entries(value).map(([key, nested]) => [key, toJsonValue(nested)])) as JsonObject;
    return String(value);
};

const isObject = (value: unknown): value is Record<string, JsonValue> => typeof value === 'object' &&
    value !== null &&
    !Array.isArray(value);

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { AgentTableError, TABLE_ERROR_CODES } from './errors';
import { stableStringify } from './utils';
import type {
    TableCommandDefinition,
    JsonObject,
    JsonValue,
    TableCommandRequest,
    TableController,
    TableControllerAdapter,
    TableControllerExecution,
    TableFilter,
    TableQueryPatch,
    TableQueryState,
    TableSort,
    TableColumnDefinition,
    TransitionContext,
} from './types';

const EMPTY_SCHEMA: JsonObject = { type: 'object', properties: {}, additionalProperties: false };
const SORT_SCHEMA: JsonObject = {
    type: 'object',
    properties: {
        columnId: { type: 'string', minLength: 1 },
        direction: { type: 'string', enum: ['asc', 'desc'] },
    },
    required: ['columnId', 'direction'],
    additionalProperties: false,
};
const FILTER_SCHEMA: JsonObject = {
    type: 'object',
    properties: {
        columnId: { type: 'string', minLength: 1 },
        operator: {
            type: 'string',
            enum: ['eq', 'notEq', 'contains', 'startsWith', 'endsWith', 'in', 'notIn', 'gt', 'gte', 'lt', 'lte', 'between', 'isEmpty', 'isNotEmpty'],
        },
        value: {},
    },
    required: ['columnId', 'operator'],
    additionalProperties: false,
};
const FILTERS_SCHEMA: JsonObject = { type: 'array', items: FILTER_SCHEMA };
const FILTER_OPERATORS = new Set([
    'eq', 'notEq', 'contains', 'startsWith', 'endsWith', 'in', 'notIn', 'gt', 'gte', 'lt', 'lte', 'between', 'isEmpty', 'isNotEmpty',
]);
const ROW_IDS_SCHEMA: JsonObject = {
    type: 'object',
    properties: { rowIds: { type: 'array', items: { type: 'string', minLength: 1 }, uniqueItems: true } },
    required: ['rowIds'],
    additionalProperties: false,
};

export const TABLE_COMMAND_DEFINITIONS: TableCommandDefinition[] = [
    {
        id: 'table.setQuery',
        title: 'Set table query',
        inputSchema: {
            type: 'object',
            properties: {
                filters: FILTERS_SCHEMA,
                sort: { oneOf: [SORT_SCHEMA, { type: 'null' }] },
                page: { type: 'integer', minimum: 1 },
                pageSize: { type: 'integer', minimum: 1 },
            },
            minProperties: 1,
            additionalProperties: false,
        },
    },
    {
        id: 'table.setFilters',
        title: 'Set table filters',
        inputSchema: {
            type: 'object',
            properties: { filters: FILTERS_SCHEMA },
            required: ['filters'],
            additionalProperties: false,
        },
    },
    { id: 'table.clearFilters', title: 'Clear table filters', inputSchema: EMPTY_SCHEMA },
    {
        id: 'table.setSort',
        title: 'Set table sort',
        inputSchema: {
            type: 'object',
            properties: { sort: SORT_SCHEMA },
            required: ['sort'],
            additionalProperties: false,
        },
    },
    { id: 'table.clearSort', title: 'Clear table sort', inputSchema: EMPTY_SCHEMA },
    {
        id: 'table.goToPage',
        title: 'Go to table page',
        inputSchema: {
            type: 'object',
            properties: { page: { type: 'integer', minimum: 1 } },
            required: ['page'],
            additionalProperties: false,
        },
    },
    {
        id: 'table.setPageSize',
        title: 'Set table page size',
        inputSchema: {
            type: 'object',
            properties: { pageSize: { type: 'integer', minimum: 1 } },
            required: ['pageSize'],
            additionalProperties: false,
        },
    },
    { id: 'table.refresh', title: 'Refresh table', inputSchema: EMPTY_SCHEMA },
    {
        id: 'table.getDisplayedData',
        title: 'Get displayed table data',
        inputSchema: {
            type: 'object',
            properties: {
                offset: { type: 'integer', minimum: 0 },
                limit: { type: 'integer', minimum: 1 },
            },
            additionalProperties: false,
        },
    },
    {
        id: 'table.getFilterOptions',
        title: 'Get table filter options',
        inputSchema: {
            type: 'object',
            properties: {
                columnId: { type: 'string', minLength: 1 },
                query: { type: 'string' },
                offset: { type: 'integer', minimum: 0 },
                limit: { type: 'integer', minimum: 1 },
            },
            required: ['columnId'],
            additionalProperties: false,
        },
    },
    { id: 'table.copy', title: 'Copy displayed table data', inputSchema: EMPTY_SCHEMA },
    { id: 'table.setSelectedRows', title: 'Set selected table rows', inputSchema: ROW_IDS_SCHEMA },
    { id: 'table.setExpandedRows', title: 'Set expanded table rows', inputSchema: ROW_IDS_SCHEMA },
    {
        id: 'table.invokeCellCommand',
        title: 'Invoke table cell command',
        inputSchema: {
            type: 'object',
            properties: {
                rowId: { type: 'string', minLength: 1 },
                columnId: { type: 'string', minLength: 1 },
                cellCommandId: { type: 'string', minLength: 1 },
                input: {},
            },
            required: ['rowId', 'columnId', 'cellCommandId'],
            additionalProperties: false,
        },
    },
];

export const createAgentTableController = (adapter: TableControllerAdapter): TableController => ({
    tableKey: adapter.tableKey,
    title: adapter.title,
    getAvailability: adapter.getAvailability,
    getSnapshot: adapter.getSnapshot,
    subscribeStable: adapter.subscribeStable,
    cancel: adapter.cancel,
    execute: async (request, context) => executeCommand(adapter, request, context),
});

const executeCommand = async (
    adapter: TableControllerAdapter,
    request: TableCommandRequest,
    context: TransitionContext,
): Promise<TableControllerExecution> => {
    validateCommandArgs(request.commandId, request.args);
    const snapshot = adapter.getSnapshot();
    if (!snapshot.capabilities.includes(request.commandId)) {
        throw new AgentTableError({
            code: TABLE_ERROR_CODES.CAPABILITY_UNSUPPORTED,
            message: `Command '${request.commandId}' is not supported by this table.`,
            retryable: false,
        });
    }

    switch (request.commandId) {
        case 'table.getDisplayedData':
            return getDisplayedData(adapter, request.args, snapshot.dataAccess.maxRowsPerRequest, context);
        case 'table.getFilterOptions':
            return { result: await requireAdapter(adapter.getFilterOptions, request.commandId)(request.args, context) };
        case 'table.copy': {
            const copied = await requireAdapter(adapter.copyDisplayedData, request.commandId)(context);
            return { result: { scope: 'displayed', ...copied } as unknown as JsonValue };
        }
        case 'table.setSelectedRows':
            return runRowsTransition(adapter, adapter.runSelectionTransition, request, context);
        case 'table.setExpandedRows':
            return runRowsTransition(adapter, adapter.runExpansionTransition, request, context);
        case 'table.invokeCellCommand':
            await validateCellCommand(adapter, request.args, snapshot, context);
            return requireAdapter(adapter.runCellCommandTransition, request.commandId)(request.args, context);
        default:
            return runQueryCommand(adapter, request, context);
    }
};

const runQueryCommand = async (
    adapter: TableControllerAdapter,
    request: TableCommandRequest,
    context: TransitionContext,
): Promise<TableControllerExecution> => {
    const snapshot = adapter.getSnapshot();
    const current = snapshot.state.query;
    const next = nextQueryState(current, request);
    validateQuery(next, snapshot.columns);
    const maxPage = Math.max(1, Math.ceil(snapshot.state.total / next.pageSize));
    if (next.page > maxPage) {
        throw new AgentTableError({
            code: TABLE_ERROR_CODES.PAGE_OUT_OF_RANGE,
            message: `Page ${next.page} exceeds the maximum page ${maxPage}.`,
            retryable: true,
            details: { requestedPage: next.page, maxPage, total: snapshot.state.total, pageSize: next.pageSize },
        });
    }
    const force = request.commandId === 'table.refresh';
    if (!force && stableStringify(current) === stableStringify(next)) return { snapshot: adapter.getSnapshot(), changed: false };
    const result = await requireAdapter(adapter.runQueryTransition, request.commandId)(next, context);
    return { snapshot: result, changed: true };
};

const nextQueryState = (current: TableQueryState, request: TableCommandRequest): TableQueryState => {
    const args = request.args;
    const patch: TableQueryPatch = {};
    switch (request.commandId) {
        case 'table.setQuery':
            Object.assign(patch, parseQueryPatch(args));
            break;
        case 'table.setFilters':
            patch.filters = parseFilters(args.filters);
            break;
        case 'table.clearFilters':
            patch.filters = [];
            break;
        case 'table.setSort':
            patch.sort = parseSort(args.sort);
            break;
        case 'table.clearSort':
            patch.sort = null;
            break;
        case 'table.goToPage':
            patch.page = positiveInteger(args.page, 'page');
            break;
        case 'table.setPageSize':
            patch.pageSize = positiveInteger(args.pageSize, 'pageSize');
            break;
        case 'table.refresh':
            return current;
        default:
            throw invalid(`Unsupported query command '${request.commandId}'.`);
    }

    const resetsPage = patch.filters !== undefined || patch.sort !== undefined || patch.pageSize !== undefined;
    if (resetsPage && patch.page !== undefined && patch.page !== 1) {
        throw invalid('Changing filters, sort, or page size only allows page 1.');
    }
    const next = {
        filters: patch.filters ?? current.filters,
        sort: patch.sort === undefined ? current.sort : patch.sort,
        page: resetsPage ? 1 : patch.page ?? current.page,
        pageSize: patch.pageSize ?? current.pageSize,
    };
    return normalizeQuery(next);
};

const parseQueryPatch = (args: JsonObject): TableQueryPatch => {
    validateArgsKeys(args, ['filters', 'sort', 'page', 'pageSize']);
    if (Object.keys(args).length === 0) throw invalid('table.setQuery requires at least one query field.');
    return {
        ...(args.filters === undefined ? {} : { filters: parseFilters(args.filters) }),
        ...(args.sort === undefined ? {} : { sort: args.sort === null ? null : parseSort(args.sort) }),
        ...(args.page === undefined ? {} : { page: positiveInteger(args.page, 'page') }),
        ...(args.pageSize === undefined ? {} : { pageSize: positiveInteger(args.pageSize, 'pageSize') }),
    };
};

const parseFilters = (value: JsonValue | undefined): TableFilter[] => {
    if (!Array.isArray(value)) throw invalid('filters must be an array.');
    const filters = value.map((item) => {
        if (!isObject(item) || typeof item.columnId !== 'string' || !item.columnId || typeof item.operator !== 'string') {
            throw invalid('Each filter must include columnId and operator.');
        }
        validateArgsKeys(item, ['columnId', 'operator', 'value']);
        if (!FILTER_OPERATORS.has(item.operator)) throw invalid(`Unknown filter operator '${item.operator}'.`);
        return item as unknown as TableFilter;
    });
    if (new Set(filters.map(({ columnId }) => columnId)).size !== filters.length) {
        throw invalid('Only one filter per column is supported.');
    }
    return filters.sort((left, right) => left.columnId.localeCompare(right.columnId));
};

const parseSort = (value: JsonValue | undefined): TableSort => {
    if (!isObject(value) || typeof value.columnId !== 'string' || !value.columnId || !['asc', 'desc'].includes(String(value.direction))) {
        throw invalid('sort must include columnId and direction asc or desc.');
    }
    validateArgsKeys(value, ['columnId', 'direction']);
    return { columnId: value.columnId, direction: value.direction as 'asc' | 'desc' };
};

const validateQuery = (query: TableQueryState, columns: TableColumnDefinition[]): void => {
    const columnMap = new Map(columns.map((column) => [column.columnId, column]));
    query.filters.forEach((filter) => {
        const column = columnMap.get(filter.columnId);
        if (!column) {
            throw new AgentTableError({
                code: TABLE_ERROR_CODES.COLUMN_UNKNOWN,
                message: `Unknown table column '${filter.columnId}'.`,
                retryable: false,
            });
        }
        if (!column.filterOperators?.includes(filter.operator)) {
            throw new AgentTableError({
                code: TABLE_ERROR_CODES.FILTER_UNSUPPORTED,
                message: `Filter operator '${filter.operator}' is not supported by '${filter.columnId}'.`,
                retryable: false,
            });
        }
        validateFilterValue(filter, column);
    });
    if (!query.sort) return;
    const sortColumn = columnMap.get(query.sort.columnId);
    if (!sortColumn) {
        throw new AgentTableError({
            code: TABLE_ERROR_CODES.COLUMN_UNKNOWN,
            message: `Unknown table column '${query.sort.columnId}'.`,
            retryable: false,
        });
    }
    if (!sortColumn.sortable) {
        throw new AgentTableError({
            code: TABLE_ERROR_CODES.SORT_UNSUPPORTED,
            message: `Column '${query.sort.columnId}' is not sortable.`,
            retryable: false,
        });
    }
};

const runRowsTransition = async (
    adapter: TableControllerAdapter,
    executor: TableControllerAdapter['runSelectionTransition'] | TableControllerAdapter['runExpansionTransition'],
    request: TableCommandRequest,
    context: TransitionContext,
): Promise<TableControllerExecution> => {
    const rowIds = request.args.rowIds;
    if (!isStringArray(rowIds)) {
        throw invalid('rowIds must be an array of strings.');
    }
    if (new Set(rowIds).size !== rowIds.length || rowIds.some((rowId) => rowId.length === 0)) {
        throw invalid('rowIds must contain unique, non-empty strings.');
    }
    const normalized = [...rowIds].sort();
    const currentSnapshot = adapter.getSnapshot();
    const current = request.commandId === 'table.setSelectedRows'
        ? currentSnapshot.state.selectedRowIds
        : currentSnapshot.state.expandedRowIds;
    if (stableStringify([...current].sort()) === stableStringify(normalized)) {
        return { snapshot: currentSnapshot, changed: false };
    }
    validateRowConstraint(request.commandId, normalized, currentSnapshot);
    await validateInteractiveRows(adapter, request.commandId, normalized, currentSnapshot, context);
    const snapshot = await requireAdapter(executor, request.commandId)(normalized, context);
    return { snapshot, changed: true };
};

const getDisplayedData = async (
    adapter: TableControllerAdapter,
    args: JsonObject | undefined,
    maxRows: number,
    context: TransitionContext,
): Promise<TableControllerExecution> => {
    const offset = nonNegativeInteger(args?.offset ?? 0, 'offset');
    const limit = positiveInteger(args?.limit ?? Math.min(20, maxRows), 'limit');
    if (limit > maxRows) throw invalid(`limit exceeds maxRowsPerRequest ${maxRows}.`);
    const result = await requireAdapter(adapter.getDisplayedData, 'table.getDisplayedData')(offset, limit, context);
    return { result: result as unknown as JsonValue };
};

const validateFilterValue = (filter: TableFilter, column: TableColumnDefinition): void => {
    const value = filter.value;
    const requiresNoValue = ['isEmpty', 'isNotEmpty'].includes(filter.operator);
    if (requiresNoValue) {
        if (value !== undefined) throw filterValueInvalid(filter, 'does not accept a value');
        return;
    }
    if (value === undefined || value === null) throw filterValueInvalid(filter, 'requires a value');
    if (['in', 'notIn'].includes(filter.operator)) {
        if (!Array.isArray(value) || value.length === 0) throw filterValueInvalid(filter, 'requires a non-empty array');
        value.forEach((item) => validateScalarFilterValue(filter, column, item));
        return;
    }
    if (filter.operator === 'between') {
        if (!isObject(value) || value.min === undefined || value.max === undefined) {
            throw filterValueInvalid(filter, 'requires { min, max }');
        }
        validateScalarFilterValue(filter, column, value.min);
        validateScalarFilterValue(filter, column, value.max);
        if (typeof value.min === 'number' && typeof value.max === 'number' && value.min > value.max) {
            throw filterValueInvalid(filter, 'requires min <= max');
        }
        return;
    }
    validateScalarFilterValue(filter, column, value);
};

const validateScalarFilterValue = (filter: TableFilter, column: TableColumnDefinition, value: JsonValue): void => {
    const valid = column.dataType === 'number'
        ? typeof value === 'number' && Number.isFinite(value)
        : column.dataType === 'boolean'
            ? typeof value === 'boolean'
            : typeof value === 'string';
    if (!valid) throw filterValueInvalid(filter, `requires a ${column.dataType} value`);
    if (column.allowedValues && !column.allowedValues.some((option) => option.value === value)) {
        throw filterValueInvalid(filter, 'is not one of the allowed values');
    }
};

const filterValueInvalid = (filter: TableFilter, reason: string): AgentTableError => new AgentTableError({
    code: TABLE_ERROR_CODES.FILTER_VALUE_INVALID,
    message: `Filter '${filter.columnId}' ${reason}.`,
    retryable: false,
});

const validateRowConstraint = (
    commandId: TableCommandRequest['commandId'],
    rowIds: string[],
    snapshot: ReturnType<TableControllerAdapter['getSnapshot']>,
): void => {
    const constraint = commandId === 'table.setSelectedRows' ? snapshot.constraints?.selection : snapshot.constraints?.expansion;
    if (!constraint) return;
    const configuredLimit = commandId === 'table.setSelectedRows'
        ? snapshot.constraints?.selection?.maxSelectedRows
        : snapshot.constraints?.expansion?.maxExpandedRows;
    const limit = constraint.mode === 'single' ? 1 : configuredLimit;
    if (limit !== undefined && rowIds.length > limit) {
        throw new AgentTableError({
            code: commandId === 'table.setSelectedRows'
                ? TABLE_ERROR_CODES.SELECTION_LIMIT_EXCEEDED
                : TABLE_ERROR_CODES.EXPANSION_LIMIT_EXCEEDED,
            message: `Command '${commandId}' allows at most ${limit} rows.`,
            retryable: false,
        });
    }
};

const validateInteractiveRows = async (
    adapter: TableControllerAdapter,
    commandId: TableCommandRequest['commandId'],
    rowIds: string[],
    snapshot: ReturnType<TableControllerAdapter['getSnapshot']>,
    context: TransitionContext,
): Promise<void> => {
    if (rowIds.length === 0) return;
    const rowsById = await loadRowsById(adapter, rowIds, snapshot, context, commandId);
    const invalidRow = rowIds.find((rowId) => {
        const row = rowsById.get(rowId);
        if (!row) return true;
        return commandId === 'table.setSelectedRows'
            ? row.interactions?.selectable !== true
            : row.interactions?.expandable !== true;
    });
    if (!invalidRow) return;
    throw new AgentTableError({
        code: TABLE_ERROR_CODES.ROW_UNKNOWN,
        message: `Row '${invalidRow}' is unavailable for command '${commandId}'.`,
        retryable: false,
    });
};

const loadRowsById = async (
    adapter: TableControllerAdapter,
    rowIds: string[],
    snapshot: ReturnType<TableControllerAdapter['getSnapshot']>,
    context: TransitionContext,
    commandId: string,
): Promise<Map<string, Awaited<ReturnType<NonNullable<TableControllerAdapter['getDisplayedData']>>>['rows'][number]>> => {
    const getDisplayedData = requireAdapter(adapter.getDisplayedData, commandId);
    const remaining = new Set(rowIds);
    const rowsById = new Map<string, Awaited<ReturnType<typeof getDisplayedData>>['rows'][number]>();
    const pageSize = Math.max(1, snapshot.dataAccess.maxRowsPerRequest);
    for (let offset = 0; offset < snapshot.dataAccess.availableRows && remaining.size; offset += pageSize) {
        const data = await getDisplayedData(offset, Math.min(pageSize, snapshot.dataAccess.availableRows - offset), context);
        data.rows.forEach((row) => {
            if (!remaining.delete(row.rowId)) return;
            rowsById.set(row.rowId, row);
        });
        if (!data.hasMore) break;
    }
    return rowsById;
};

const validateCellCommand = async (
    adapter: TableControllerAdapter,
    args: JsonObject | undefined,
    snapshot: ReturnType<TableControllerAdapter['getSnapshot']>,
    context: TransitionContext,
): Promise<void> => {
    if (typeof args?.rowId !== 'string' || !args.rowId ||
        typeof args.columnId !== 'string' || !args.columnId ||
        typeof args.cellCommandId !== 'string' || !args.cellCommandId) {
        throw invalid('rowId, columnId, and cellCommandId must be non-empty strings.');
    }
    const { rowId, columnId, cellCommandId } = args;
    const column = snapshot.columns.find((item) => item.columnId === columnId);
    if (!column) throw new AgentTableError({
        code: TABLE_ERROR_CODES.COLUMN_UNKNOWN,
        message: `Unknown table column '${columnId}'.`,
        retryable: false,
    });
    if (!column.cellCommands?.some((command) => command.cellCommandId === cellCommandId)) {
        throw invalid(`Cell command '${cellCommandId}' is not declared by column '${columnId}'.`);
    }
    const row = (await loadRowsById(adapter, [rowId], snapshot, context, 'table.invokeCellCommand')).get(rowId);
    if (!row) throw new AgentTableError({
        code: TABLE_ERROR_CODES.ROW_UNKNOWN,
        message: `Unknown row '${rowId}'.`,
        retryable: false,
    });
    if (!row.cells[columnId]?.availableCommands?.includes(cellCommandId)) {
        throw invalid(`Cell command '${cellCommandId}' is unavailable for row '${rowId}'.`);
    }
};

const requireAdapter = <T,>(value: T | undefined, commandId: string): T => {
    if (value) return value;
    throw new AgentTableError({
        code: TABLE_ERROR_CODES.CAPABILITY_UNSUPPORTED,
        message: `Command '${commandId}' has no business adapter.`,
        retryable: false,
    });
};

const validateCommandArgs = (commandId: TableCommandRequest['commandId'], args: JsonObject): void => {
    const allowedKeys: Record<TableCommandRequest['commandId'], string[]> = {
        'table.setQuery': ['filters', 'sort', 'page', 'pageSize'],
        'table.setFilters': ['filters'],
        'table.clearFilters': [],
        'table.setSort': ['sort'],
        'table.clearSort': [],
        'table.goToPage': ['page'],
        'table.setPageSize': ['pageSize'],
        'table.refresh': [],
        'table.getDisplayedData': ['offset', 'limit'],
        'table.getFilterOptions': ['columnId', 'query', 'offset', 'limit'],
        'table.copy': [],
        'table.setSelectedRows': ['rowIds'],
        'table.setExpandedRows': ['rowIds'],
        'table.invokeCellCommand': ['rowId', 'columnId', 'cellCommandId', 'input'],
    };
    validateArgsKeys(args, allowedKeys[commandId]);
    if (commandId === 'table.getFilterOptions') {
        if (typeof args.columnId !== 'string' || !args.columnId) throw invalid('columnId is required.');
        if (args.query !== undefined && typeof args.query !== 'string') throw invalid('query must be a string.');
        if (args.offset !== undefined) nonNegativeInteger(args.offset, 'offset');
        if (args.limit !== undefined) positiveInteger(args.limit, 'limit');
    }
};

const normalizeQuery = (query: TableQueryState): TableQueryState => ({
    filters: [...query.filters].sort((left, right) => left.columnId.localeCompare(right.columnId)),
    sort: query.sort,
    page: positiveInteger(query.page, 'page'),
    pageSize: positiveInteger(query.pageSize, 'pageSize'),
});

const positiveInteger = (value: JsonValue | undefined, field: string): number => {
    if (!Number.isInteger(value) || Number(value) < 1) throw invalid(`${field} must be a positive integer.`);
    return Number(value);
};

const nonNegativeInteger = (value: JsonValue | undefined, field: string): number => {
    if (!Number.isInteger(value) || Number(value) < 0) throw invalid(`${field} must be a non-negative integer.`);
    return Number(value);
};

const isObject = (value: JsonValue | undefined): value is { [key: string]: JsonValue } => (
    typeof value === 'object' && value !== null && !Array.isArray(value)
);

const isStringArray = (value: JsonValue | undefined): value is string[] => (
    Array.isArray(value) && value.every((item) => typeof item === 'string')
);

const validateArgsKeys = (args: JsonObject, allowedKeys: string[]): void => {
    const unknownKey = Object.keys(args).find((key) => !allowedKeys.includes(key));
    if (unknownKey) throw invalid(`Unknown command argument '${unknownKey}'.`);
};

const invalid = (message: string): AgentTableError => new AgentTableError({
    code: TABLE_ERROR_CODES.COMMAND_INVALID,
    message,
    retryable: false,
});

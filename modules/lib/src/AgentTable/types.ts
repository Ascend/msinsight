/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { JsonObject, JsonPrimitive, JsonValue } from '../FrontendAgentCommand';

export type { JsonObject, JsonPrimitive, JsonValue } from '../FrontendAgentCommand';

export const TABLE_COMMAND_IDS = [
    'table.setQuery',
    'table.setFilters',
    'table.clearFilters',
    'table.setSort',
    'table.clearSort',
    'table.goToPage',
    'table.setPageSize',
    'table.refresh',
    'table.getDisplayedData',
    'table.getFilterOptions',
    'table.copy',
    'table.setSelectedRows',
    'table.setExpandedRows',
    'table.invokeCellCommand',
] as const;

export type TableCommandId = typeof TABLE_COMMAND_IDS[number];
export type TableDataType = 'string' | 'number' | 'boolean' | 'date';
export type TableFilterOperator = 'eq' | 'notEq' | 'contains' | 'startsWith' | 'endsWith'
| 'in' | 'notIn' | 'gt' | 'gte' | 'lt' | 'lte' | 'between' | 'isEmpty' | 'isNotEmpty';

export interface TableFilter {
    columnId: string;
    operator: TableFilterOperator;
    value?: JsonValue;
}

export interface TableSort {
    columnId: string;
    direction: 'asc' | 'desc';
}

export interface TableQueryState {
    filters: TableFilter[];
    sort: TableSort | null;
    page: number;
    pageSize: number;
}

export interface CellCommandDefinition {
    cellCommandId: string;
    title: string;
    effect: 'view' | 'navigation' | 'table-state';
    inputSchema?: JsonObject;
}

export interface TableColumnDefinition {
    columnId: string;
    title: string;
    dataType: TableDataType;
    unit?: string;
    readable: boolean;
    sortable?: boolean;
    filterOperators?: TableFilterOperator[];
    allowedValues?: Array<{ value: JsonPrimitive; label: string }>;
    filterOptions?: 'dynamic';
    cellCommands?: CellCommandDefinition[];
}

export interface TableAvailability {
    visible: boolean;
    ready: boolean;
    busy: boolean;
}

export interface TableInteractionConstraints {
    selection?: { mode: 'single' | 'multiple'; maxSelectedRows?: number };
    expansion?: { mode: 'single' | 'multiple'; maxExpandedRows?: number };
}

export interface TableStableState {
    query: TableQueryState;
    total: number;
    rowCount: number;
    selectedRowIds: string[];
    expandedRowIds: string[];
    [key: string]: JsonValue | TableQueryState | string[] | number;
}

export interface TableStableSnapshot {
    state: TableStableState;
    columns: TableColumnDefinition[];
    capabilities: TableCommandId[];
    constraints?: TableInteractionConstraints;
    dataAccess: {
        maxRowsPerRequest: number;
        availableRows: number;
    };
}

export interface TableObservation extends TableStableSnapshot {
    protocolVersion: 1;
    targetId: string;
    tableKey: string;
    title: string;
    revision: number;
    availability: TableAvailability;
}

export interface TableCommandObservation extends Omit<TableObservation, 'capabilities'> {
    commands: string[];
}

export interface DisplayedCell {
    value: JsonValue;
    availableCommands?: string[];
}

export interface DisplayedRow {
    rowId: string;
    cells: Record<string, DisplayedCell>;
    selected?: boolean;
    expanded?: boolean;
    interactions?: {
        selectable?: boolean;
        expandable?: boolean;
    };
}

export interface DisplayedDataResult {
    revision?: number;
    columns: TableColumnDefinition[];
    rows: DisplayedRow[];
    offset: number;
    returned: number;
    available: number;
    hasMore: boolean;
}

export interface TableCommandRequest {
    targetId: string;
    expectedRevision: number;
    commandId: TableCommandId;
    args: JsonObject;
    requestId: string;
    deadline: number;
}

export interface TableCommandEffect {
    type: 'table-state-changed' | 'navigation' | 'view-opened';
    moduleId?: string;
    destination?: string;
    viewKey?: string;
}

export interface TableCommandResult {
    status: 'completed';
    targetId?: string;
    targetStatus?: 'available' | 'unavailable';
    revision?: number;
    noOp?: boolean;
    state?: TableStableState;
    result?: JsonValue;
    effect?: TableCommandEffect;
    requiresObserve?: boolean;
}

export interface TableCommandErrorShape {
    code: string;
    message: string;
    retryable: boolean;
    details?: JsonObject;
    state?: TableStableState;
}

export interface TransitionContext {
    requestId: string;
    transitionId: string;
    source: 'agent' | 'user';
    deadline: number;
    signal: AbortSignal;
}

export interface TableControllerExecution {
    snapshot?: TableStableSnapshot;
    result?: JsonValue;
    changed?: boolean;
    targetStatus?: 'available' | 'unavailable';
    effect?: TableCommandEffect;
    requiresObserve?: boolean;
}

export interface TableController {
    tableKey: string;
    title: string;
    getAvailability: () => TableAvailability;
    getSnapshot: () => TableStableSnapshot;
    execute: (request: TableCommandRequest, context: TransitionContext) => Promise<TableControllerExecution>;
    subscribeStable?: (listener: (snapshot: TableStableSnapshot) => void) => () => void;
    cancel?: (requestId: string) => void;
}

export interface TableControllerRegistration {
    targetId: string;
    unregister: () => void;
}

export interface TableQueryPatch {
    filters?: TableFilter[];
    sort?: TableSort | null;
    page?: number;
    pageSize?: number;
}

export interface TableControllerAdapter {
    tableKey: string;
    title: string;
    getAvailability: () => TableAvailability;
    getSnapshot: () => TableStableSnapshot;
    subscribeStable?: (listener: (snapshot: TableStableSnapshot) => void) => () => void;
    runQueryTransition?: (query: TableQueryState, context: TransitionContext) => Promise<TableStableSnapshot>;
    runSelectionTransition?: (rowIds: string[], context: TransitionContext) => Promise<TableStableSnapshot>;
    runExpansionTransition?: (rowIds: string[], context: TransitionContext) => Promise<TableStableSnapshot>;
    runCellCommandTransition?: (args: JsonObject, context: TransitionContext) => Promise<TableControllerExecution>;
    getDisplayedData?: (offset: number, limit: number, context: TransitionContext) => Promise<DisplayedDataResult>;
    getFilterOptions?: (args: JsonObject, context: TransitionContext) => Promise<JsonValue>;
    copyDisplayedData?: (context: TransitionContext) => Promise<{ rowCount: number; columnCount: number }>;
    cancel?: (requestId: string) => void;
}

export interface TableCommandDefinition {
    id: TableCommandId;
    title: string;
    description?: string;
    inputSchema: JsonObject;
}

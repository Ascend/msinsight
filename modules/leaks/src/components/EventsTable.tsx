/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React, { useEffect, useMemo, useRef, useState } from 'react';
import i18n from '@insight/lib/i18n';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { ResizeTable, fetchColumnFilterProps, type ResizeTableRef } from '@insight/lib/resize';
import { Tooltip } from '@insight/lib/components';
import {
    AgentTableError,
    TABLE_ERROR_CODES,
    useAgentTableController,
    type TableController,
    type TableQueryState,
    type TableStableSnapshot,
    type TransitionContext,
} from '@insight/lib/AgentTable';
import type { Session } from '../entity/session';
import { getEventTableData } from './dataHandler';
import { generateJsonShow } from '../utils/utils';
import { applyMemSnapshotEventLocate, isMemSnapshotSliceReadyForQuery } from '../utils/memSnapshotSlices';
import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from './blockLoadState';
import type { EventsTableData } from '../utils/RequestUtils';
import {
    createMemScopeSystemTableController,
    requireMemScopeTableData,
    filtersToAgentQuery,
    queryToBusinessFilters,
    type MemScopeSystemTableView,
} from '../agent/systemTableController';
import { memScopeTableControllerRegistry } from '../agent/runtime';

const DEFAULT_TABLE_HEIGHT = 400;
const TABLE_CHROME_HEIGHT = 88;
const MIN_TABLE_SCROLL_Y = 120;

const getRecordValue = (record: any, keys: string[]): any => {
    for (const key of keys) {
        if (record?.[key] !== undefined && record?.[key] !== null && record?.[key] !== '') return record[key];
    }
    return undefined;
};

const toNumber = (value: any): number | null => {
    const num = Number(value);
    return Number.isNaN(num) ? null : num;
};

const isValidLocateId = (value: number | null): boolean => value !== null && value >= 0;
const isEventIdColumn = (col: any): boolean => ['id', 'ID', 'eventId', 'Event ID'].includes(String(col.key ?? col.name));

const LocateLink = ({ disabled, children, onClick }: { disabled: boolean; children: React.ReactNode; onClick: () => void }): React.ReactElement => (
    <button
        type="button"
        disabled={disabled}
        onClick={(event): void => {
            event.stopPropagation();
            if (!disabled) onClick();
        }}
        onMouseEnter={(event): void => {
            if (!disabled) event.currentTarget.style.textDecoration = 'underline';
        }}
        onMouseLeave={(event): void => {
            event.currentTarget.style.textDecoration = 'none';
        }}
        style={{
            border: 0,
            padding: 0,
            color: disabled ? 'inherit' : '#1677ff',
            background: 'transparent',
            cursor: disabled ? 'default' : 'pointer',
            textAlign: 'left',
            font: 'inherit',
        }}
        className="locate-link"
    >
        {children}
    </button>
);

const isCallStackColumn = (col: any): boolean => {
    const key = String(col.key ?? '').toLowerCase();
    const name = String(col.name ?? '').toLowerCase();
    return key.includes('callstack') || key.includes('call stack') || name.includes('callstack') || name.includes('call stack');
};

const getTooltipTitle = (col: any, text: string): React.ReactNode => {
    const title = col.key === 'attr' && text ? generateJsonShow(text) : text ?? '';
    if (!isCallStackColumn(col)) return title;
    return <div style={{ maxHeight: 280, overflow: 'auto', maxWidth: 560 }}>{title}</div>;
};

const getTableColumns = (t: TFunction, session: Session, query: TableQueryState): any => session.eventsTableHeader.map((col: any) => {
    const filter = query.filters.find(({ columnId }) => columnId === col.key);
    const sort = query.sort;
    const filteredValue = filter?.operator === 'between' && typeof filter.value === 'object' && filter.value !== null && !Array.isArray(filter.value)
        ? [filter.value.min, filter.value.max]
        : filter === undefined ? null : [filter.value];
    const item = {
        dataIndex: col.key,
        key: col.key,
        title: t(col.name, { defaultValue: col.name, keyPrefix: 'tableHead' }),
        sorter: col.sortable,
        sortOrder: sort !== null && sort.columnId === col.key ? (sort.direction === 'desc' ? 'descend' : 'ascend') : null,
        ellipsis: { showTitle: false },
        showSorterTooltip: t(col.name, { keyPrefix: 'tableHeadTooltip', defaultValue: '' }) === ''
            ? true
            : { title: t(col.name, { keyPrefix: 'tableHeadTooltip' }) },
        render: (text: string, record: any): React.ReactNode => {
            if (session.module !== 'leaks' && isEventIdColumn(col)) {
                const eventId = toNumber(getRecordValue(record, ['id', 'ID', 'eventId', 'Event ID']));
                return <LocateLink
                    disabled={!isValidLocateId(eventId)}
                    onClick={(): void => {
                        if (!isValidLocateId(eventId)) return;
                        runInAction(() => {
                            applyMemSnapshotEventLocate(session, eventId as number, session.deviceId);
                        });
                    }}
                >
                    {text ?? ''}
                </LocateLink>;
            }
            return <Tooltip
                title={getTooltipTitle(col, text)}
                placement="top"
                overlayInnerStyle={isCallStackColumn(col) ? { maxHeight: 300, overflow: 'hidden', maxWidth: 600 } : undefined}
            >
                {text ?? ''}
            </Tooltip>;
        },
    };
    if (col.searchable) {
        return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', '')), filteredValue };
    }
    if (col.rangeFilterable) {
        const filterOptions = { min: col.min, max: col.max };
        return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', ''), true, filterOptions), filteredValue };
    }
    return item;
});

const queryFromSession = (session: Session): TableQueryState => ({
    filters: [
        ...Object.entries(session.eventsFilters).map(([columnId, value]) => ({ columnId, operator: 'contains' as const, value })),
        ...Object.entries(session.eventsRangeFilters).map(([columnId, value]) => ({
            columnId,
            operator: 'between' as const,
            value: { min: value[0], max: value[1] },
        })),
    ].sort((left, right) => left.columnId.localeCompare(right.columnId)),
    sort: session.eventsOrder !== '' && session.eventsOrderBy
        ? { columnId: session.eventsOrderBy, direction: session.eventsOrder ? 'desc' : 'asc' }
        : null,
    page: session.eventsCurrentPage,
    pageSize: session.eventsPageSize,
});

const applyQuery = (session: Session, query: TableQueryState): void => {
    const { filters, rangeFilters } = queryToBusinessFilters(query);
    runInAction(() => {
        session.eventsFilters = filters;
        session.eventsRangeFilters = rangeFilters;
        session.eventsOrder = query.sort ? query.sort.direction === 'desc' : '';
        session.eventsOrderBy = query.sort?.columnId ?? '';
        session.eventsCurrentPage = query.page;
        session.eventsPageSize = query.pageSize;
    });
};

const EventsTable = observer(({
    session,
    height,
    visible,
}: {
    session: Session;
    height?: number;
    visible: boolean;
}): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const {
        deviceId, eventsTableData, eventsTableHeader, eventsCurrentPage, tableKey,
        eventsPageSize, eventsTotal, maxTime, minTime, eventType,
    } = session;
    const sliceReady = isMemSnapshotSliceReadyForQuery(session);
    const blockLoadReady = isMemoryBlockLoadReady(session);
    const { fileHash, module, deviceIds, selectedSliceIndex, loadedMemoryBlockContextKey, loadingBlocks } = session;
    const [loading, setLoading] = useState(false);
    const tableRef = useRef<ResizeTableRef>(null);
    const controllerRef = useRef<TableController>();
    const latestRequestRef = useRef(0);
    const lastAutoQueryKeyRef = useRef<string>();
    const mountedRef = useRef(false);
    const requestSequencesRef = useRef(new Map<string, { sequence: number; isCurrentContext: () => boolean }>());
    const contextIdentityRef = useRef({ session, deviceIds, generation: 0 });
    if (contextIdentityRef.current.session !== session || contextIdentityRef.current.deviceIds !== deviceIds) {
        contextIdentityRef.current = { session, deviceIds, generation: contextIdentityRef.current.generation + 1 };
    }
    const contextGeneration = contextIdentityRef.current.generation;
    const contextKey = JSON.stringify([createMemoryBlockContextKey(session), contextGeneration, tableKey, minTime, maxTime]);
    const stableQueryRef = useRef(queryFromSession(session));
    const stableContextKeyRef = useRef(contextKey);
    const committedContextKeyRef = useRef(contextKey);
    if (stableContextKeyRef.current !== contextKey) {
        stableContextKeyRef.current = contextKey;
        stableQueryRef.current = queryFromSession(session);
    }
    const listenersRef = useRef(new Set<(snapshot: TableStableSnapshot) => void>());
    const viewRef = useRef<MemScopeSystemTableView>({
        columns: [], rows: [], query: stableQueryRef.current, total: 0, busy: false, visible: true, ready: false,
    });

    // User transitions bypass the registry, so readiness is checked here as well.
    const isQueryReady = (): boolean => session.deviceId !== '' && session.maxTime !== 0 &&
        session.maxTime !== undefined && isMemSnapshotSliceReadyForQuery(session) &&
        isMemoryBlockLoadReady(session);
    const requestContextKey = (): string => JSON.stringify([
        createMemoryBlockContextKey(session), contextIdentityRef.current.generation,
        session.minTime, session.maxTime, session.tableKey, session.tableType,
        session.loadedMemoryBlockContextKey, session.loadingBlocks,
        isMemSnapshotSliceReadyForQuery(session), isMemoryBlockLoadReady(session),
    ]);
    useEffect(() => {
        mountedRef.current = true;
        return () => {
            mountedRef.current = false;
            latestRequestRef.current++;
            requestSequencesRef.current.clear();
            lastAutoQueryKeyRef.current = undefined;
        };
    }, [session]);

    viewRef.current = {
        columns: eventsTableHeader.map((column: any) => ({
            ...column,
            name: t(column.name, { defaultValue: column.name, keyPrefix: 'tableHead' }),
        })),
        rows: committedContextKeyRef.current === contextKey && isQueryReady()
            ? eventsTableData as unknown as Array<Record<string, unknown>>
            : [],
        query: stableQueryRef.current,
        contextKey,
        total: committedContextKeyRef.current === contextKey && isQueryReady() ? eventsTotal : 0,
        busy: loading,
        visible: visible && session.tableType === 'events',
        ready: isQueryReady() && eventsTableHeader.length > 0 && committedContextKeyRef.current === contextKey,
    };

    const currentSnapshot = (): TableStableSnapshot => controllerRef.current?.getSnapshot() ?? {
        state: { query: stableQueryRef.current, total: 0, rowCount: 0, selectedRowIds: [], expandedRowIds: [] },
        columns: [],
        capabilities: [],
        dataAccess: { maxRowsPerRequest: 1, availableRows: 0 },
    };

    const publishStable = (): TableStableSnapshot => {
        const snapshot = currentSnapshot();
        listenersRef.current.forEach((listener) => listener(snapshot));
        return snapshot;
    };

    const runQueryTransition = async (query: TableQueryState, context: TransitionContext): Promise<TableStableSnapshot> => {
        if (!isQueryReady()) {
            throw new AgentTableError({
                code: TABLE_ERROR_CODES.NOT_READY,
                message: 'The MemScope event table is not ready for this context.',
                retryable: true,
            });
        }
        const requestSequence = ++latestRequestRef.current;
        const frozenContextKey = requestContextKey();
        const frozenDeviceIds = session.deviceIds;
        const isCurrentContext = (): boolean => mountedRef.current && contextIdentityRef.current.session === session &&
            session.deviceIds === frozenDeviceIds && requestContextKey() === frozenContextKey;
        const ownsTransition = (): boolean => requestSequence === latestRequestRef.current && isCurrentContext();
        requestSequencesRef.current.set(context.requestId, { sequence: requestSequence, isCurrentContext });
        const assertCurrent = (): void => {
            if (context.signal.aborted) {
                throw context.signal.reason ?? new AgentTableError({
                    code: TABLE_ERROR_CODES.COMMAND_CANCELLED,
                    message: 'The MemScope event table command was cancelled.',
                    retryable: true,
                });
            }
            if (!ownsTransition()) {
                throw new AgentTableError({
                    code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED,
                    message: 'The MemScope event table context or command was superseded.',
                    retryable: true,
                });
            }
        };
        let nextQuery = query;
        try {
            assertCurrent();
            applyQuery(session, nextQuery);
            viewRef.current = { ...viewRef.current, busy: true };
            setLoading(true);
            let data = await getEventTableData(session);
            assertCurrent();
            let response = requireMemScopeTableData<EventsTableData>(data, isQueryReady(), 'events');
            const maxPage = Math.max(Math.ceil(response.total / nextQuery.pageSize), 1);
            if (nextQuery.page > maxPage) {
                nextQuery = { ...nextQuery, page: maxPage };
                applyQuery(session, nextQuery);
                data = await getEventTableData(session);
                assertCurrent();
                response = requireMemScopeTableData<EventsTableData>(data, isQueryReady(), 'events');
            }
            runInAction(() => {
                session.eventsTableData = response.events;
                session.eventsTableHeader = response.headers;
                session.eventsTotal = response.total;
            });
            stableQueryRef.current = nextQuery;
            committedContextKeyRef.current = stableContextKeyRef.current;
            viewRef.current = {
                ...viewRef.current,
                rows: response.events as unknown as Array<Record<string, unknown>>,
                columns: response.headers.map((column: any) => ({
                    ...column,
                    name: t(column.name, { defaultValue: column.name, keyPrefix: 'tableHead' }),
                })),
                query: nextQuery,
                total: response.total,
                ready: isQueryReady() && response.headers.length > 0,
                busy: false,
            };
            return publishStable();
        } catch (error) {
            if (ownsTransition()) {
                applyQuery(session, stableQueryRef.current);
                viewRef.current = { ...viewRef.current, busy: false };
            }
            throw error;
        } finally {
            requestSequencesRef.current.delete(context.requestId);
            if (ownsTransition()) setLoading(false);
        }
    };

    const controller = useMemo(() => createMemScopeSystemTableController({
        tableKey: 'memscope.system.events',
        title: 'Event View',
        getView: () => viewRef.current,
        runQueryTransition,
        copyDisplayedData: async () => {
            if (!tableRef.current) throw new Error('The MemScope event table copy command is unavailable.');
            await tableRef.current.copy();
            return { rowCount: viewRef.current.rows.length, columnCount: viewRef.current.columns.length };
        },
        subscribeStable: (listener) => {
            listenersRef.current.add(listener);
            return () => listenersRef.current.delete(listener);
        },
        cancel: (requestId) => {
            const request = requestSequencesRef.current.get(requestId);
            requestSequencesRef.current.delete(requestId);
            if (!request || request.sequence !== latestRequestRef.current) return;
            latestRequestRef.current++;
            if (!request.isCurrentContext()) return;
            applyQuery(session, stableQueryRef.current);
            viewRef.current = { ...viewRef.current, busy: false };
            setLoading(false);
        },
    }), [session]);
    controllerRef.current = controller;
    useAgentTableController(controller, memScopeTableControllerRegistry);
    const tableHeaderKey = JSON.stringify(eventsTableHeader);
    useEffect(() => {
        publishStable();
    }, [visible, session.tableType, fileHash, module, deviceId, eventType, selectedSliceIndex,
        contextGeneration, deviceIds, tableKey, minTime, maxTime, sliceReady, blockLoadReady,
        loadedMemoryBlockContextKey, loadingBlocks, tableHeaderKey]);

    const runUserQuery = (query: TableQueryState): void => {
        const context: TransitionContext = {
            requestId: crypto.randomUUID(),
            transitionId: crypto.randomUUID(),
            source: 'user',
            deadline: Date.now() + 30000,
            signal: new AbortController().signal,
        };
        void runQueryTransition(query, context).catch(() => undefined);
    };

    const columns = useMemo(
        () => getTableColumns(t, session, stableQueryRef.current),
        [tableHeaderKey, JSON.stringify(stableQueryRef.current), session.module, t],
    );
    const displayedRows = committedContextKeyRef.current === contextKey && isQueryReady() ? eventsTableData : [];
    const defaultDataSource = process.env.NODE_ENV === 'development' ? [{}] : [];
    const tableHeight = Math.max(height ?? DEFAULT_TABLE_HEIGHT, MIN_TABLE_SCROLL_Y + TABLE_CHROME_HEIGHT);
    const scrollY = Math.max(MIN_TABLE_SCROLL_Y, tableHeight - TABLE_CHROME_HEIGHT);

    const onTableChange = (_pagination: any, filters: Record<string, unknown>, sorter: any, extra: any): void => {
        if (extra.action !== 'sort' && extra.action !== 'filter') return;
        const current = stableQueryRef.current;
        runUserQuery({
            ...current,
            ...(extra.action === 'sort'
                ? {
                    sort: sorter.order
                        ? { columnId: String(sorter.field), direction: sorter.order === 'descend' ? 'desc' : 'asc' }
                        : null,
                }
                : {}),
            ...(extra.action === 'filter' ? { filters: filtersToAgentQuery(filters, viewRef.current.columns) } : {}),
            page: 1,
        });
    };

    const onChange = (newCurrent: number, newPageSize: number): void => {
        const current = stableQueryRef.current;
        if (current.page === newCurrent && current.pageSize === newPageSize) return;
        runUserQuery({
            ...current,
            page: current.pageSize === newPageSize ? newCurrent : 1,
            pageSize: newPageSize,
        });
    };

    useEffect(() => {
        if (!isQueryReady()) {
            latestRequestRef.current++;
            requestSequencesRef.current.clear();
            lastAutoQueryKeyRef.current = undefined;
            stableQueryRef.current = queryFromSession(session);
            runInAction(() => {
                session.eventsTableData = [];
                session.eventsTotal = 0;
            });
            const waitingForBlocks = deviceId !== '' && maxTime !== 0 && (!sliceReady || !blockLoadReady);
            viewRef.current = {
                ...viewRef.current, rows: [], total: 0, query: stableQueryRef.current, ready: false, busy: waitingForBlocks,
            };
            setLoading(waitingForBlocks);
            publishStable();
            return;
        }
        const queryKey = JSON.stringify({
            contextKey, sliceReady, blockLoadReady, loadedMemoryBlockContextKey, loadingBlocks,
        });
        if (lastAutoQueryKeyRef.current === queryKey) return;
        // A graph reload can clear/invalidate table data even when the selected range is unchanged.
        latestRequestRef.current++;
        requestSequencesRef.current.clear();
        lastAutoQueryKeyRef.current = queryKey;
        stableQueryRef.current = queryFromSession(session);
        viewRef.current = { ...viewRef.current, query: stableQueryRef.current };
        runUserQuery(stableQueryRef.current);
    }, [fileHash, module, deviceId, eventType, selectedSliceIndex, deviceIds, contextGeneration,
        tableKey, minTime, maxTime, sliceReady, blockLoadReady, loadedMemoryBlockContextKey, loadingBlocks]);

    return <ResizeTable
        ref={tableRef}
        data-testid="eventsTable"
        columns={columns}
        dataSource={displayedRows.length === 0 ? defaultDataSource : displayedRows.map((item: any, index: number) => ({ ...item, key: `${item.id ?? item.ID}_${index}` }))}
        rowKey={(record: any, index?: number): string => `${record.id ?? record.ID ?? 'event'}_${index ?? 0}`}
        onChange={onTableChange}
        pagination={{
            current: eventsCurrentPage,
            pageSize: eventsPageSize,
            pageSizeOptions: [10, 20, 30, 50, 100],
            onChange,
            total: eventsTotal,
            showTotal: (totalNum: number): string => i18n.t('PaginationTotal', { total: totalNum }),
            showQuickJumper: true,
        }}
        scroll={{ x: 150 * columns.length, y: scrollY }}
        style={{ height: tableHeight }}
        loading={loading}
        key={`${tableKey}_Events`}
    />;
});

export default EventsTable;

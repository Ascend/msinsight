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
import { Tag } from 'antd';
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
import { getBlockTableData } from './dataHandler';
import { generateJsonShow } from '../utils/utils';
import {
    createMemScopeSystemTableController,
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

const isValidBlockId = (value: number | null): boolean => value !== null;
const isValidEventId = (value: number | null): boolean => value !== null && value >= 0;
const isBlockIdColumn = (col: any): boolean => ['id', 'ID', 'blockId', 'Block ID'].includes(String(col.key ?? col.name));
const isAllocEventIdColumn = (col: any): boolean => ['Alloc Event ID', 'allocEventId', 'allocOrMapEventId'].includes(String(col.key ?? col.name));
const isFreeEventIdColumn = (col: any): boolean => ['Free Event ID', 'freeEventId', 'freeOrUnmapEventId'].includes(String(col.key ?? col.name));

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

const columnRender = (col: any, text: string, record: any, t: TFunction, session: Session): React.ReactNode => {
    const tags: { [key: string]: boolean } = { 'early-alloc': record.lazyUsed, 'late-free': record.delayedFree, idle: record.longIdle };
    const showTag = Object.keys(tags).filter(tag => tags[tag]);
    const blockId = toNumber(getRecordValue(record, ['id', 'ID', 'blockId', 'Block ID']));
    const allocEventId = toNumber(getRecordValue(record, ['Alloc Event ID', 'allocEventId', 'allocOrMapEventId']));
    const freeEventId = toNumber(getRecordValue(record, ['Free Event ID', 'freeEventId', 'freeOrUnmapEventId']));
    const locateEvent = (eventId: number | null): void => {
        if (!isValidEventId(eventId)) return;
        runInAction(() => {
            session.pendingEventLocate = { eventId: eventId as number, deviceId: session.deviceId };
        });
    };
    if (isBlockIdColumn(col)) {
        return <>
            <LocateLink disabled={!isValidBlockId(blockId)} onClick={(): void => {
                if (!isValidBlockId(blockId)) return;
                runInAction(() => { session.pendingBlockLocateId = blockId as number; });
            }}>
                <span style={{ marginRight: '5px' }}>{text}</span>
            </LocateLink>
            {showTag.map((tag) => <Tag key={tag} color="red">{t(tag)}</Tag>)}
        </>;
    }
    if (session.module !== 'leaks' && isAllocEventIdColumn(col)) {
        return <LocateLink disabled={!isValidEventId(allocEventId)} onClick={(): void => locateEvent(allocEventId)}>{text ?? ''}</LocateLink>;
    }
    if (session.module !== 'leaks' && isFreeEventIdColumn(col)) {
        return <LocateLink disabled={!isValidEventId(freeEventId)} onClick={(): void => locateEvent(freeEventId)}>{text ?? ''}</LocateLink>;
    }
    return <Tooltip title={col.key === 'attr' && text ? generateJsonShow(text) : text || ''} placement="top">
        {text ?? ''}
    </Tooltip>;
};

const getTableColumns = (t: TFunction, session: Session, query: TableQueryState): any => session.blocksTableHeader.map((col: any) => {
    const filter = query.filters.find(({ columnId }) => columnId === col.key);
    const sort = query.sort;
    const filteredValue = filter?.operator === 'between' && typeof filter.value === 'object' && filter.value !== null && !Array.isArray(filter.value)
        ? [filter.value.min, filter.value.max]
        : filter === undefined ? null : [filter.value];
    const item = {
        dataIndex: col.key,
        key: col.key,
        title: isBlockIdColumn(col) ? t('blockId', { keyPrefix: 'tableHead' }) : t(col.name, { defaultValue: col.name, keyPrefix: 'tableHead' }),
        sorter: col.sortable,
        sortOrder: sort !== null && sort.columnId === col.key ? (sort.direction === 'desc' ? 'descend' : 'ascend') : null,
        ellipsis: { showTitle: false },
        showSorterTooltip: t(col.name, { keyPrefix: 'tableHeadTooltip', defaultValue: '' }) === ''
            ? true
            : { title: t(col.name, { keyPrefix: 'tableHeadTooltip' }) },
        render: (text: string, record: any): React.ReactNode => columnRender(col, text, record, t, session),
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
        ...Object.entries(session.blocksFilters).map(([columnId, value]) => ({ columnId, operator: 'contains' as const, value })),
        ...Object.entries(session.blocksRangeFilters).map(([columnId, value]) => ({
            columnId,
            operator: 'between' as const,
            value: { min: value[0], max: value[1] },
        })),
    ].sort((left, right) => left.columnId.localeCompare(right.columnId)),
    sort: session.blocksOrder !== '' && session.blocksOrderBy
        ? { columnId: session.blocksOrderBy, direction: session.blocksOrder ? 'desc' : 'asc' }
        : null,
    page: session.blocksCurrentPage,
    pageSize: session.blocksPageSize,
});

const applyQuery = (session: Session, query: TableQueryState): void => {
    const { filters, rangeFilters } = queryToBusinessFilters(query);
    runInAction(() => {
        session.blocksFilters = filters;
        session.blocksRangeFilters = rangeFilters;
        session.blocksOrder = query.sort ? query.sort.direction === 'desc' : '';
        session.blocksOrderBy = query.sort?.columnId ?? '';
        session.blocksCurrentPage = query.page;
        session.blocksPageSize = query.pageSize;
    });
};

const BlocksTable = observer(({
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
        deviceId, eventType, blocksTableData, blocksTableHeader, tableKey,
        blocksCurrentPage, blocksPageSize, blocksTotal, maxTime, minTime,
        lazyUsedThreshold, delayedFreeThreshold, longIdleThreshold, onlyInefficient,
        autoFilterPotentialLeaks,
    } = session;
    const [loading, setLoading] = useState(false);
    const tableRef = useRef<ResizeTableRef>(null);
    const controllerRef = useRef<TableController>();
    const latestRequestRef = useRef(0);
    const lastAutoQueryKeyRef = useRef<string>();
    const requestSequencesRef = useRef(new Map<string, number>());
    const stableQueryRef = useRef(queryFromSession(session));
    const listenersRef = useRef(new Set<(snapshot: TableStableSnapshot) => void>());
    const viewRef = useRef<MemScopeSystemTableView>({
        columns: [], rows: [], query: stableQueryRef.current, total: 0, busy: false, visible: true, ready: false,
    });

    viewRef.current = {
        columns: blocksTableHeader.map((column: any) => ({
            ...column,
            name: t(column.name, { defaultValue: column.name, keyPrefix: 'tableHead' }),
        })),
        rows: blocksTableData as unknown as Array<Record<string, unknown>>,
        query: stableQueryRef.current,
        total: blocksTotal,
        busy: loading,
        visible: visible && session.tableType === 'blocks',
        ready: deviceId !== '' && maxTime !== 0 && maxTime !== undefined && blocksTableHeader.length > 0,
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
        const requestSequence = ++latestRequestRef.current;
        requestSequencesRef.current.set(context.requestId, requestSequence);
        let nextQuery = query;
        applyQuery(session, nextQuery);
        viewRef.current = { ...viewRef.current, busy: true };
        setLoading(true);
        const assertCurrent = (): void => {
            if (context.signal.aborted) throw context.signal.reason;
            if (requestSequence !== latestRequestRef.current) {
                throw new AgentTableError({
                    code: TABLE_ERROR_CODES.COMMAND_SUPERSEDED,
                    message: 'The MemScope block table command was superseded by a newer command.',
                    retryable: true,
                });
            }
        };
        try {
            let response = await getBlockTableData(session);
            assertCurrent();
            const maxPage = Math.max(Math.ceil(response.total / nextQuery.pageSize), 1);
            if (nextQuery.page > maxPage) {
                nextQuery = { ...nextQuery, page: maxPage };
                applyQuery(session, nextQuery);
                response = await getBlockTableData(session);
                assertCurrent();
            }
            runInAction(() => {
                session.blocksTableData = response.blocks;
                session.blocksTableHeader = response.headers;
                session.blocksTotal = response.total;
            });
            stableQueryRef.current = nextQuery;
            viewRef.current = {
                ...viewRef.current,
                rows: response.blocks as unknown as Array<Record<string, unknown>>,
                columns: response.headers.map((column: any) => ({
                    ...column,
                    name: t(column.name, { defaultValue: column.name, keyPrefix: 'tableHead' }),
                })),
                query: nextQuery,
                total: response.total,
                busy: false,
            };
            return publishStable();
        } catch (error) {
            if (requestSequence === latestRequestRef.current) {
                applyQuery(session, stableQueryRef.current);
                viewRef.current = { ...viewRef.current, busy: false };
            }
            throw error;
        } finally {
            requestSequencesRef.current.delete(context.requestId);
            if (requestSequence === latestRequestRef.current) setLoading(false);
        }
    };

    const controller = useMemo(() => createMemScopeSystemTableController({
        tableKey: 'memscope.system.blocks',
        title: 'Block View',
        getView: () => viewRef.current,
        runQueryTransition,
        copyDisplayedData: async () => {
            if (!tableRef.current) throw new Error('The MemScope block table copy command is unavailable.');
            await tableRef.current.copy();
            return { rowCount: viewRef.current.rows.length, columnCount: viewRef.current.columns.length };
        },
        subscribeStable: (listener) => {
            listenersRef.current.add(listener);
            return () => listenersRef.current.delete(listener);
        },
        cancel: (requestId) => {
            const requestSequence = requestSequencesRef.current.get(requestId);
            requestSequencesRef.current.delete(requestId);
            if (requestSequence !== latestRequestRef.current) return;
            applyQuery(session, stableQueryRef.current);
            viewRef.current = { ...viewRef.current, busy: false };
            setLoading(false);
        },
    }), [session]);
    controllerRef.current = controller;
    useAgentTableController(controller, memScopeTableControllerRegistry);
    const tableHeaderKey = JSON.stringify(blocksTableHeader);
    useEffect(() => {
        publishStable();
    }, [visible, session.tableType, deviceId, maxTime, tableHeaderKey]);

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
    const defaultDataSource = process.env.NODE_ENV === 'development' ? [{}] : [];
    const scrollX = Math.max(120 * columns.length, 1000);
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
        if (deviceId === '' || maxTime === 0 || maxTime === undefined) return;
        const queryKey = JSON.stringify({
            module: session.module,
            deviceId,
            eventType,
            maxTime,
            minTime,
            lazyUsedThreshold,
            delayedFreeThreshold,
            longIdleThreshold,
            onlyInefficient,
            autoFilterPotentialLeaks,
        });
        if (lastAutoQueryKeyRef.current === queryKey) return;
        lastAutoQueryKeyRef.current = queryKey;
        runUserQuery(queryFromSession(session));
    }, [session.module, deviceId, eventType, maxTime, minTime, JSON.stringify(lazyUsedThreshold), JSON.stringify(delayedFreeThreshold), JSON.stringify(longIdleThreshold), onlyInefficient, autoFilterPotentialLeaks]);

    return <ResizeTable
        ref={tableRef}
        data-testid="blocksTable"
        columns={columns}
        dataSource={blocksTableData.length === 0 ? defaultDataSource : blocksTableData.map((item: any, index: number) => ({ ...item, key: `${item.id ?? item.ID}_${index}` }))}
        rowKey={(record: any, index?: number): string => `${record.id ?? record.ID ?? 'block'}_${index ?? 0}`}
        onChange={onTableChange}
        pagination={{
            current: blocksCurrentPage,
            pageSize: blocksPageSize,
            pageSizeOptions: [10, 20, 30, 50, 100],
            onChange,
            total: blocksTotal,
            showTotal: (totalNum: number): string => i18n.t('PaginationTotal', { total: totalNum }),
            showQuickJumper: true,
        }}
        scroll={{ x: scrollX, y: scrollY }}
        style={{ height: tableHeight }}
        loading={loading}
        key={`${tableKey}_BLocks`}
    />;
});

export default BlocksTable;

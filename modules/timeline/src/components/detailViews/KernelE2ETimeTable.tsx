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

import { observer } from 'mobx-react';
import React, { useCallback, useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { getTimeOffset } from '../../insight/units/utils';
import { ResizeTable, fetchColumnFilterProps } from '@insight/lib/resize';
import { StyledEmpty } from '@insight/lib/utils';
import { DragDirection, useDraggableContainer, createSmartDebounceRequestFunc } from '@insight/lib';
import { DETAIL_HEADER_HEIGHT_ETC_PX, type SelectContentViewProps } from './SystemView';
import { queryOneKernel, queryKernelE2ETime } from './Common';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import { ProjectType } from '../../entity/insight';
import type { CardMetaData } from '../../entity/data';
import type { KernelE2ETimeRecord, KernelE2EHighlightSlice, KernelE2ETimeResponse } from '../../api/interface';
import { KernelE2ERole } from '../../api/interface';
import { message, Tooltip, type TablePaginationConfig } from 'antd';
import type { FilterValue, SorterResult } from 'antd/lib/table/interface';
import { MoreContainer, StyledMoreCard } from '../BottomPanel';
import styled from '@emotion/styled';
import i18n from '@insight/lib/i18n';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';

const STATUS_COLOR_MAP: Record<string, string> = {
    normal: '#52c41a',
    fallback: '#fa8c16',
    incomplete: '#f5222d',
};

const ROLE_ORDER: Record<string, number> = {
    [KernelE2ERole.PYTHON_CALL]: 1,
    [KernelE2ERole.PYTHON_OP]: 2,
    [KernelE2ERole.ENQUEUE]: 3,
    [KernelE2ERole.DEQUEUE]: 4,
    [KernelE2ERole.CANN_API]: 5,
    [KernelE2ERole.LAUNCH]: 6,
    [KernelE2ERole.HARDWARE_TASK]: 7,
};

interface HighlightSlicesDetailProps {
    record: KernelE2ETimeRecord | null;
    cardId: string;
    dbPath: string;
    timestampOffset: number;
    bottomHeight: number;
}

const HighlightSlicesDetail = ({ record, cardId, dbPath, timestampOffset, bottomHeight }: HighlightSlicesDetailProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });
    const [selectedSliceKey, setSelectedSliceKey] = useState<string | undefined>();

    const slices = [...(record?.highlightSlices ?? [])].sort(
        (a, b) => (ROLE_ORDER[a.role] ?? 99) - (ROLE_ORDER[b.role] ?? 99),
    );

    const getSliceKey = (slice: KernelE2EHighlightSlice, idx: number): string =>
        slice.id ?? `slice-${slice.role}-${slice.startTime}-${slice.duration}-${idx}`;

    const handleJump = async(slice: KernelE2EHighlightSlice, idx: number): Promise<void> => {
        const res = await queryOneKernel({
            rankId: cardId,
            dbPath,
            name: slice.name,
            timestamp: slice.startTime,
            duration: slice.duration,
        });
        jumpToUnitOperator({
            name: slice.name,
            id: res.id ?? slice.id,
            depth: res.depth,
            cardId,
            dbPath,
            tid: res.threadId,
            pid: res.pid,
            duration: slice.duration,
            timestamp: slice.startTime,
            metaType: res.metaType ?? '',
        });
        setSelectedSliceKey(getSliceKey(slice, idx));
    };

    const sliceColumns = [
        {
            title: '#',
            dataIndex: 'role',
            width: 30,
            align: 'right' as const,
            render: (_val: string, _slice: KernelE2EHighlightSlice, idx: number): JSX.Element => (
                <Tooltip title={_val}>{idx + 1}</Tooltip>
            ),
        },
        {
            title: t('Name'),
            dataIndex: 'name',
            ellipsis: true,
            width: 100,
            render: (val: string | undefined, slice: KernelE2EHighlightSlice, idx: number): JSX.Element | string => {
                if (slice.missingReason) {
                    return slice.missingReason;
                }
                const canHighlight = slice.startTime > 0 && slice.duration > 0 && !slice.missingReason;
                if (canHighlight) {
                    return (
                        <a href="javascript:void(0)" onClick={(): void => { handleJump(slice, idx); }} style={{ cursor: 'pointer' }}>
                            {val ?? '--'}
                        </a>
                    );
                }
                return val ?? '--';
            },
        },
        {
            title: t('Start Time'),
            dataIndex: 'startTime',
            width: 80,
            render: (val: number): string => getDetailTimeDisplay(Math.max(0, val - timestampOffset)) || '--',
        },
        {
            title: t('Duration'),
            dataIndex: 'duration',
            width: 80,
            render: (val: number): string => getDetailTimeDisplay(val) || '--',
        },
    ];
    return (
        <ResizeTable
            key={`kernelE2e-${cardId}-${dbPath}-${timestampOffset}`}
            rowKey={(row: KernelE2EHighlightSlice, idx?: number): string => getSliceKey(row, idx ?? 0)}
            dataSource={slices}
            columns={sliceColumns}
            size="small"
            pagination={false}
            scroll={{ y: bottomHeight !== undefined ? bottomHeight - 120 : undefined }}
            rowClassName={(slice: KernelE2EHighlightSlice, idx?: number): string =>
                getSliceKey(slice, idx ?? 0) === selectedSliceKey ? 'selected-row' : ''
            }
        />
    );
};

const KernelE2ETimeTableWrapper = styled.div`
    height: 100%;
    display: flex;
    flex-direction: column;
    .ant-pagination-total-text {
        flex: 1;
    }
`;

const KernelE2ETimeTableBody = styled.div`
    flex: 1;
    min-height: 0;
`;

type SummaryData = Omit<KernelE2ETimeResponse, 'records'>;

const MatchRateSummary = ({ summary }: { summary: SummaryData }): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });
    return (
        <Tooltip title={
            <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                <span>{t('Normal')}: <strong style={{ color: STATUS_COLOR_MAP.normal }}>{summary.normalCount}</strong></span>
                <span>{t('Fallback')}: <strong style={{ color: STATUS_COLOR_MAP.fallback }}>{summary.fallbackCount}</strong></span>
                <span>{t('Incomplete')}: <strong style={{ color: STATUS_COLOR_MAP.incomplete }}>{summary.incompleteCount}</strong></span>
            </div>
        }>
            <span style={{ cursor: 'pointer' }}>{t('Match Rate')}: <strong>{(summary.launchMatchRate * 100).toFixed(2)}%</strong></span>
        </Tooltip>
    );
};

const PAGING_MARGIN_TOP = 16;
const PAGINATION_H = 24;
const DEFAULT_PAGE = { current: 1, pageSize: 100, total: 0 };
const EMPTY_SUMMARY: SummaryData = { totalCount: 0, normalCount: 0, fallbackCount: 0, incompleteCount: 0, launchMatchRate: 0 };
const PATH_TYPE_FILTERS = [
    { text: 'ACLOP', value: 'ACLOP' },
    { text: 'ACLNN', value: 'ACLNN' },
] as const;

const ColumnTitle = (name: string, tip?: string): JSX.Element => {
    if (!tip) return <span>{name}</span>;
    return <Tooltip placement="left" title={tip}><span>{name}</span></Tooltip>;
};

interface FetchParams {
    cardId: string;
    dbPath: string;
    timestampOffset: number;
    timeAnalysisRange: [number, number] | undefined;
    current: number;
    pageSize: number;
    pathType: string;
    opNameSearch: string | undefined;
    sortField: string;
    sortOrder: string;
}

const debouncedFetch = createSmartDebounceRequestFunc(
    async (params: FetchParams) => {
        let startTime = params.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = params.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        return queryKernelE2ETime({
            rankId: params.cardId,
            dbPath: params.dbPath,
            startTime: Math.floor(startTime + params.timestampOffset),
            endTime: Math.ceil(endTime + params.timestampOffset),
            current: params.current,
            pageSize: params.pageSize,
            pathType: params.pathType,
            opName: params.opNameSearch,
            sortField: params.sortField,
            sortOrder: params.sortOrder,
        });
    },
    { delay: 100 },
);

export const KernelE2ETimeTable = observer((props: SelectContentViewProps) => {
    const [dataSource, setDataSource] = useState<KernelE2ETimeRecord[]>([]);
    const [loading, setLoading] = useState(false);
    const [sortField, setSortField] = useState('endToEndTime');
    const [sortOrder, setSortOrder] = useState('desc');
    const [opNameSearch, setOpNameSearch] = useState('');
    const [pathType, setPathType] = useState('all');
    const [page, setPage] = useState(DEFAULT_PAGE);
    const [selectedRow, setSelectedRow] = useState<KernelE2ETimeRecord | null>(null);
    const [expandedRowKeys, setExpandedRowKeys] = useState<React.Key[]>([]);
    const [summary, setSummary] = useState<SummaryData>(EMPTY_SUMMARY);
    const tableWrapperRef = useRef<HTMLDivElement>(null);
    const [tableScrollY, setTableScrollY] = useState<number>(props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX - PAGING_MARGIN_TOP);

    const { t: tTableHead } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    const updateTableHeight = useCallback((): void => {
        if (tableWrapperRef.current) {
            const wrapperHeight = tableWrapperRef.current.clientHeight;
            const tableHeader = tableWrapperRef.current.querySelector('.ant-table-header');
            const tablePagination = tableWrapperRef.current.querySelector('.ant-table-pagination');
            const headerH = tableHeader ? tableHeader.clientHeight : 0;
            const paginationH = tablePagination ? tablePagination.clientHeight : PAGINATION_H;
            setTableScrollY(wrapperHeight - headerH - paginationH - PAGING_MARGIN_TOP);
            return;
        }
        setTableScrollY(props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX - PAGING_MARGIN_TOP);
    }, [props.bottomHeight]);

    useEffect(() => {
        updateTableHeight();
        const wrapper = tableWrapperRef.current;
        if (!wrapper) return;
        const observer = new ResizeObserver((): void => {
            setTimeout(updateTableHeight, 0);
        });
        observer.observe(wrapper);
        return (): void => observer.disconnect();
    }, [updateTableHeight]);

    const status = props.session.units.find((unit: any) => (unit.metadata as CardMetaData).cardId === props.card.cardId)?.phase;
    const targetInfo = props.session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === props.card?.cardId);
    const timestampOffset = getTimeOffset(props.session, { cardId: props.card.cardId });

    const isDisabled = !props.card?.cardId || targetInfo?.projectType === ProjectType.IE;

    const resetState = useCallback((): void => {
        setDataSource([]);
        setPage(DEFAULT_PAGE);
        setSummary(EMPTY_SUMMARY);
        setSelectedRow(null);
        setOpNameSearch('');
        setPathType('all');
        setSortField('endToEndTime');
        setSortOrder('desc');
        setExpandedRowKeys([]);
    }, []);

    useEffect(() => {
        resetState();
    }, [props.card.cardId, resetState]);

    useEffect(() => {
        if (status !== 'download') {
            debouncedFetch.cancel();
            return;
        }

        if (isDisabled) {
            debouncedFetch.cancel();
            resetState();
            return;
        }

        setSelectedRow(null);
        setExpandedRowKeys([]);
        setLoading(true);

        let stale = false;
        debouncedFetch({
            cardId: props.card.cardId,
            dbPath: props.card.dbPath,
            timestampOffset,
            timeAnalysisRange: props.session.timeAnalysisRange,
            current: page.current,
            pageSize: page.pageSize,
            pathType,
            opNameSearch: opNameSearch || undefined,
            sortField,
            sortOrder,
        }).then((res) => {
            if (stale) return;
            setDataSource(res.records);
            setPage({ current: page.current, pageSize: page.pageSize, total: res.totalCount });
            setSummary({
                totalCount: res.totalCount,
                normalCount: res.normalCount,
                fallbackCount: res.fallbackCount,
                incompleteCount: res.incompleteCount,
                launchMatchRate: res.launchMatchRate,
            });
            setLoading(false);
        }).catch(() => {
            if (stale) return;
            setLoading(false);
            message.error(i18n.t('RenderError'));
        });

        return (): void => {
            stale = true;
            debouncedFetch.cancel();
        };
    }, [status, debouncedFetch, isDisabled, props.card.cardId, props.card.dbPath,
        timestampOffset, props.session.timeAnalysisRange, pathType, opNameSearch, sortField,
        sortOrder, page.current, page.pageSize, resetState]);

    const columns = [
        {
            title: tTableHead('Operator'),
            dataIndex: 'opName',
            ellipsis: true,
            width: 100,
            key: 'opName',
            ...fetchColumnFilterProps('opName', 'Operator', false, undefined, (value: string): void => {
                setOpNameSearch(value);
                setPage({ current: 1, pageSize: page.pageSize, total: page.total });
            }),
        },
        {
            title: ColumnTitle(tTableHead('Path'), tTableHead('PathTooltip')),
            dataIndex: 'pathType',
            ellipsis: true,
            width: 60,
            filters: [...PATH_TYPE_FILTERS],
            key: 'pathType',
        },
        {
            title: ColumnTitle(tTableHead('Prepare Time'), tTableHead('PrepareTimeTooltip')),
            dataIndex: 'prepareTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'prepareTime',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('Python API Time'), tTableHead('PythonApiTimeTooltip')),
            dataIndex: 'pythonApiTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'pythonApiTime',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('Enqueue Time'), tTableHead('EnqueueTimeTooltip')),
            dataIndex: 'enqueueTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'enqueueTime',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('Queue Time'), tTableHead('QueueTimeTooltip')),
            dataIndex: 'queueTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'queueTime',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('Pipeline2 Time'), tTableHead('Pipeline2TimeTooltip')),
            dataIndex: 'pipeline2Time',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'pipeline2Time',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('Launch Time'), tTableHead('LaunchTimeTooltip')),
            dataIndex: 'launchTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'launchTime',
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: ColumnTitle(tTableHead('E2E Time'), tTableHead('E2ETimeTooltip')),
            dataIndex: 'endToEndTime',
            ellipsis: true,
            width: 80,
            sorter: true,
            key: 'endToEndTime',
            defaultSortOrder: 'descend' as const,
            render: (val: number | undefined): string => getDetailTimeDisplay(val) || '--',
        },
        {
            title: tTableHead('Notes'),
            dataIndex: 'diagnostic',
            ellipsis: true,
            width: 50,
            key: 'diagnostic',
            render: (val: string | undefined): JSX.Element => (
                <Tooltip title={val}>
                    <span>{val ?? '--'}</span>
                </Tooltip>
            ),
        },
    ];

    const [view] = useDraggableContainer({ dragDirection: DragDirection.RIGHT, draggableWH: 300, open: true });

    if (isDisabled || status !== 'download') {
        return (
            <div style={{ display: 'flex', height: '100%' }}>
                <StyledEmpty style={{ margin: 'auto' }} />
            </div>
        );
    }

    const handleRowClick = (record: KernelE2ETimeRecord): void => {
        setSelectedRow(record);
    };

    // 默认排序：按端到端耗时倒序；取消排序时回退到默认排序
    // 排序/筛选变化时重置页码到第 1 页，避免搜索结果为空时仍停留在旧页码
    const handleTableChange = (pagination: TablePaginationConfig, filters: Record<string, FilterValue | null>, sorter: SorterResult<KernelE2ETimeRecord> | Array<SorterResult<KernelE2ETimeRecord>>): void => {
        const s = Array.isArray(sorter) ? sorter[0] : sorter;
        const pathTypeFilters = filters?.pathType ?? [];
        const newPathType = (pathTypeFilters.length === 0 || pathTypeFilters.length === PATH_TYPE_FILTERS.length)
            ? 'all'
            : pathTypeFilters[0] as string;
        const newSortField = s?.order ? (s.field as string) : 'endToEndTime';
        const newSortOrder = s?.order ? (s.order === 'ascend' ? 'asc' : 'desc') : 'desc';

        const isPageChange = pagination.current !== page.current || pagination.pageSize !== page.pageSize;
        const isFilterOrSortChange = newSortField !== sortField || newSortOrder !== sortOrder || newPathType !== pathType;
        if (isPageChange) {
            setPage({ current: pagination.current ?? 1, pageSize: pagination.pageSize ?? 100, total: page.total });
        } else if (isFilterOrSortChange) {
            setPage({ current: 1, pageSize: page.pageSize, total: page.total });
        }
        setSortField(newSortField);
        setSortOrder(newSortOrder);
        setPathType(newPathType);
    };

    const paginationConfig = {
        current: page.current,
        pageSize: page.pageSize,
        total: page.total,
        showSizeChanger: page.total > 10,
        pageSizeOptions: [10, 20, 50, 100],
        showTotal: (total: number): JSX.Element => (
            <span>
                <span style={{ marginRight: 10 }}>{i18n.t('PaginationTotal', { total })}</span>
                <MatchRateSummary summary={summary} />
            </span>
        ),
        hideOnSinglePage: false,
        showQuickJumper: page.total / (page.pageSize || 1) > 5,
    };

    return view({
        mainContainer: (
            <KernelE2ETimeTableWrapper>
                <KernelE2ETimeTableBody ref={tableWrapperRef}>
                    <ResizeTable
                        rowKey="id"
                        dataSource={dataSource}
                        columns={columns}
                        size="small"
                        loading={loading}
                        scroll={tableScrollY !== undefined ? { y: tableScrollY } : undefined}
                        pagination={paginationConfig}
                        rowHoverable={false}
                        expandable={{ expandedRowKeys, onExpandedRowsChange: (keys: readonly React.Key[]): void => setExpandedRowKeys([...keys]) }}
                        onChange={handleTableChange}
                        onRow={(record: KernelE2ETimeRecord): React.HTMLAttributes<any> => ({
                            onClick: (): void => handleRowClick(record),
                        })}
                        rowClassName={(record: KernelE2ETimeRecord): string =>
                            selectedRow?.id === record.id ? 'selected-row' : ''
                        }
                    />
                </KernelE2ETimeTableBody>
            </KernelE2ETimeTableWrapper>
        ),
        draggableContainer: (
            <StyledMoreCard title={i18n.t('Details')} bordered={false}>
                <MoreContainer>
                    <HighlightSlicesDetail
                        record={selectedRow}
                        cardId={props.card.cardId}
                        dbPath={props.card.dbPath}
                        timestampOffset={timestampOffset}
                        bottomHeight={props.bottomHeight}
                    />
                </MoreContainer>
            </StyledMoreCard>
        ),
        id: 'kernelE2ETime',
    });
});

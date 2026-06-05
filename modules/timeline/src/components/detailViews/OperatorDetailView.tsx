/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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
import React, { useEffect, useState, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { Button } from '@insight/lib/components';
import { DownOutlined } from '@ant-design/icons';
import { ResizeTable, fetchColumnFilterProps, type ResizeTableRef } from '@insight/lib/resize';
import type { TFunction } from 'i18next';
import { StyledEmpty } from '@insight/lib/utils';
import type { CardMetaData } from '../../entity/data';
import { getPageData, queryKernelDetails, useKernelDetails } from './Common';
import { DETAIL_HEADER_HEIGHT_ETC_PX, type SelectContentViewProps } from './SystemView';
import { ProjectType } from '../../entity/insight';
import { getTimeOffset } from '../../insight/units/utils';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';

// ==================== Data Types ====================

interface OpStatisticRecord {
    type: string;
    acceleratorCore: string;
    number: number;
    totalTime: number;
    avgTime: number;
    maxTime: number;
    minTime: number;
    inputShape?: string;
    rowKey: string;
}

interface OpDetailRecord {
    name: string;
    type: string;
    opType: string;
    accCore: string;
    startTime: number;
    startTimeLabel?: string;
    duration: number;
    waitTime: number;
    blockNum: number;
    inputShape: string;
    inputType: string;
    inputFormat: string;
    outputShape: string;
    outputType: string;
    outputFormat: string;
    rowKey: string;
}

interface OpStatisticResponse {
    data: OpStatisticRecord[];
    count: number;
    pageSize: number;
    current: number;
}

// ==================== API Param Types ====================

interface QueryOperatorStatisticParams {
    rankId: string;
    dbPath: string;
    group: string;
    topK: number;
    current: number;
    pageSize: number;
    orderBy: string;
    order: string;
    filterCondition: string[];
    startTime: number;
    endTime: number;
}

// ==================== Component Props Types ====================

interface OperatorSubTableProps {
    cardId: string;
    dbPath: string;
    opType?: string;
    accCore?: string;
    inputShape?: string;
    session: SelectContentViewProps['session'];
    card: SelectContentViewProps['card'];
}

// ==================== Pagination & Sorter Types ====================

interface PageConfig {
    current: number;
    pageSize: number;
    total: number;
}

interface SorterConfig {
    field: string;
    order: string;
}

// ==================== Column Definitions ====================

const useOpTypeColumns = (t: TFunction): any[] => {
    return [
        { title: t('Type'), dataIndex: 'type', sorter: true, ellipsis: true, ...fetchColumnFilterProps('name', 'Type') },
        { title: t('AcceleratorCore'), dataIndex: 'acceleratorCore', sorter: true, ellipsis: true, ...fetchColumnFilterProps('accCore', 'AcceleratorCore') },
        { title: t('Count'), dataIndex: 'number', sorter: true, ellipsis: true },
        { title: `${t('TotalTime')}(μs)`, dataIndex: 'totalTime', sorter: true, ellipsis: true },
        { title: `${t('AvgTime')}(μs)`, dataIndex: 'avgTime', sorter: true, ellipsis: true },
        { title: `${t('MaxTime')}(μs)`, dataIndex: 'maxTime', sorter: true, ellipsis: true },
        { title: `${t('MinTime')}(μs)`, dataIndex: 'minTime', sorter: true, ellipsis: true },
    ];
};

// ==================== API Functions ====================

const queryOperatorStatistic = async (param: QueryOperatorStatisticParams): Promise<OpStatisticResponse> => {
    return window.requestData('kernel/total/list', param, 'timeline');
};

// ==================== OperatorSubTable ====================

const filterColumn = [
    'name', 'type', 'acceleratorCore', 'taskId', 'inputShapes', 'inputDataTypes',
    'inputFormats', 'outputShapes', 'outputDataTypes', 'outputFormats',
];

const defaultFilters = {
    name: [],
    type: [],
    acceleratorCore: [],
    taskId: [],
    inputShapes: [],
    inputDataTypes: [],
    inputFormats: [],
    outputShapes: [],
    outputDataTypes: [],
    outputFormats: [],
};

const OperatorSubTable = observer(({ cardId, dbPath, opType, session, card }: OperatorSubTableProps) => {
    const subDefaultPage = { current: 1, pageSize: 10, total: 0 };
    const subDefaultSorter = { field: 'duration', order: 'descend' };
    const columns = useKernelDetails();
    const [subData, setSubData] = useState<OpDetailRecord[]>([]);
    const [subLoading, setSubLoading] = useState(false);
    const [subPage, setSubPage] = useState(subDefaultPage);
    const [subSorter, setSubSorter] = useState(subDefaultSorter);
    const [subFilters, setSubFilters] = useState(defaultFilters);

    const updateSubData = async (pages: typeof subDefaultPage, sorters: typeof subDefaultSorter, filtersConditions: typeof defaultFilters): Promise<void> => {
        if (!cardId || !dbPath) return;
        setSubLoading(true);
        const filterTypes: string[] = [];
        Object.keys(filtersConditions).forEach(key => {
            const filterValue = (filtersConditions as Record<string, unknown>)[key];
            if (filterColumn.includes(key) && filterValue != null) {
                if (Array.isArray(filterValue) && filterValue.length > 0) {
                    filterTypes.push(JSON.stringify({ columnName: key, value: filterValue[0] }));
                }
            }
        });
        if (opType && !filterTypes.some(f => f.includes('"type"'))) {
            filterTypes.push(JSON.stringify({ columnName: 'type', value: opType }));
        }
        const sortedField = sorters.field === 'startTimeLabel' ? 'startTime' : sorters.field;
        let startTime = session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(session, card);
        queryKernelDetails({
            rankId: cardId,
            dbPath,
            pageSize: pages.pageSize,
            current: pages.current,
            orderBy: sorters.order ? sortedField : subDefaultSorter.field,
            order: sorters.order ?? subDefaultSorter.order,
            startTime: Math.floor(startTime + timestampOffset),
            endTime: Math.ceil(endTime + timestampOffset),
            coreType: '',
            filterCondition: filterTypes,
        }).then(res => {
            const data: OpDetailRecord[] = res.kernelDetails ?? [];
            data.forEach((item: OpDetailRecord) => {
                item.rowKey = `${item.name ?? item.type}_${item.startTime}_${item.accCore}`;
                item.startTimeLabel = getDetailTimeDisplay(item.startTime - timestampOffset);
            });
            setSubData(data);
            setSubPage(prev => ({ ...prev, total: res.count }));
        }).finally(() => {
            setSubLoading(false);
        });
    };

    useEffect(() => {
        updateSubData(subPage, subSorter, subFilters);
    }, [subPage.current, subPage.pageSize, subSorter, subFilters, cardId, dbPath, opType, session.timeAnalysisRange]);

    return <ResizeTable
        size="small"
        loading={subLoading}
        columns={columns}
        dataSource={subData}
        scroll={{ x: 'max-content' }}
        rowKey="rowKey"
        pagination={getPageData(subPage, setSubPage)}
        onChange={(_pagination: any, newFilters: any, newSorter: any): void => {
            const field = typeof newSorter?.field === 'string' ? newSorter.field : '';
            const order = newSorter?.order == null ? '' : String(newSorter.order);
            setSubSorter({ field, order });
            setSubFilters(newFilters);
        }} />;
});

// ==================== OperatorDetailView ====================

const defaultPage: PageConfig = { current: 1, pageSize: 10, total: 0 };
const defaultSorter: SorterConfig = { field: 'totalTime', order: 'descend' };

const OperatorDetailView = observer((props: SelectContentViewProps) => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    const tableRef = useRef<ResizeTableRef>(null);
    const baseColumns = useOpTypeColumns(t);
    const [page, setPage] = useState<PageConfig>(defaultPage);
    const [sorter, setSorter] = useState<SorterConfig>(defaultSorter);
    const [tableData, setTableData] = useState<OpStatisticRecord[]>([]);
    const [loading, setLoading] = useState(false);
    const [expandedRowKeys, setExpandedKeys] = useState<string[]>([]);
    const [filters, setFilters] = useState<Record<string, unknown>>({});

    const rowKey = 'rowKey';

    const columns = [
        ...baseColumns,
        {
            title: t('Operation'),
            width: 115,
            key: 'action',
            render: (_: unknown, record: OpStatisticRecord) => (
                <Button type="link"
                    onClick={(): void => {
                        const key = record[rowKey] as string;
                        setExpandedKeys((prev: string[]) => {
                            const list = [...prev];
                            const idx = list.indexOf(key);
                            if (idx === -1) {
                                list.push(key);
                            } else {
                                list.splice(idx, 1);
                            }
                            return list;
                        });
                    }}>{t('Details')}<DownOutlined /></Button>
            ),
        },
    ];

    // Request counter to discard stale responses.
    // Each effect invocation bumps the counter; after await, if the counter
    // has changed, the response is from an outdated request and is dropped.
    const requestSeqRef = useRef(0);

    const fetchData = async (
        pages: PageConfig,
        sorters: SorterConfig,
        tableFilters: Record<string, unknown>,
        seq: number,
    ): Promise<void> => {
        const targetInfo = props.session.units.find(unitItem =>
            (unitItem.metadata as CardMetaData | undefined)?.cardId === props.card?.cardId);
        if (props.card === undefined || props.card.cardId === '' || targetInfo?.projectType === ProjectType.IE) {
            setTableData([]);
            setPage(prev => prev.current === 1 ? prev : { current: 1, pageSize: prev.pageSize, total: 0 });
            return;
        }
        setLoading(true);
        let startTime = props.session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = props.session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(props.session, props.card);
        const res = await queryOperatorStatistic({
            rankId: props.card.cardId,
            dbPath: props.card.dbPath,
            group: 'Operator Type',
            topK: 0,
            current: pages.current,
            pageSize: pages.pageSize,
            orderBy: sorters.field || defaultSorter.field,
            order: sorters.order || defaultSorter.order,
            filterCondition: Object.entries(tableFilters)
                .filter(([_, v]) => v != null)
                .map(([key, value]) => JSON.stringify({ columnName: key, value: Array.isArray(value) ? value[0] : value })),
            startTime: Math.floor(startTime + timestampOffset),
            endTime: Math.ceil(endTime + timestampOffset),
        }).finally(() => {
            setLoading(false);
        });

        // Discard stale response if a newer request has been dispatched
        if (seq !== requestSeqRef.current) {
            return;
        }

        const data: OpStatisticRecord[] = res.data ?? [];
        data.forEach((item: OpStatisticRecord) => {
            item.rowKey = `${item.type}_${item.acceleratorCore}`;
        });
        setTableData(data);
        setExpandedKeys([]);

        // Adjust page if out of bounds, otherwise only update total
        if (pages.current * pages.pageSize > res.count && res.count > 0) {
            const newCurrent = Math.floor((res.count - 1) / pages.pageSize) + 1;
            setPage({ current: newCurrent, pageSize: pages.pageSize, total: res.count });
        } else if (pages.total !== res.count) {
            setPage(prev => ({ ...prev, total: res.count }));
        }
    };

    useEffect(() => {
        requestSeqRef.current += 1;
        fetchData(page, sorter, filters, requestSeqRef.current);
    }, [page.current, page.pageSize, sorter.field, sorter.order, filters, props.card.cardId, props.session.timeAnalysisRange]);

    useEffect(() => {
        const targetInfo = props.session.units.find(unitItem =>
            (unitItem.metadata as CardMetaData | undefined)?.cardId === props.card?.cardId);
        if (targetInfo?.phase === 'download') {
            requestSeqRef.current += 1;
            fetchData(page, sorter, filters, requestSeqRef.current);
        }
    }, [props.session.units]);

    return (
        props.card !== undefined && props.card.cardId !== ''
            ? <ResizeTable
                ref={tableRef}
                size="small"
                minThWidth={50}
                loading={loading}
                columns={columns}
                dataSource={tableData}
                scroll={{
                    y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX,
                    x: 100 * columns.length,
                }}
                pagination={getPageData(page, setPage)}
                onChange={(_pagination: any, newFilters: any, newSorter: any, extra: { action: string }): void => {
                    if (extra.action === 'sort') {
                        const field = typeof newSorter?.field === 'string' ? newSorter.field : '';
                        const order = newSorter?.order == null ? '' : String(newSorter.order);
                        setSorter({ field, order });
                    }
                    if (extra.action === 'filter') {
                        setFilters(newFilters);
                        setPage(prev => ({ ...prev, current: 1 }));
                    }
                }}
                rowKey={rowKey}
                expandable={{
                    expandedRowRender: (record: OpStatisticRecord) => <OperatorSubTable
                        cardId={props.card.cardId}
                        dbPath={props.card.dbPath}
                        opType={record.type}
                        inputShape={record.inputShape}
                        session={props.session}
                        card={props.card}
                    />,
                    expandedRowKeys,
                    expandIcon: () => (<></>),
                }}
            />
            : <div style={{ display: 'flex', height: '100%' }}>
                <StyledEmpty style={{ margin: 'auto' }} />
            </div>
    );
});

export default OperatorDetailView;

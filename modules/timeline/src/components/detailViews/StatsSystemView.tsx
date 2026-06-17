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
import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import {
    useKernelDetails,
    queryKernelDetails,
    queryOneKernel,
    getPageData,
    querySystemViewDetails,
    querySystemViewTrace,
    summaryAndTraceTypes,
} from './Common';
import type { CardMetaData } from '../../entity/data';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';
import { getTimeOffset } from '../../insight/units/utils';
import { Button } from '@insight/lib/components';
import { ResizeTable } from '@insight/lib/resize';
import { StyledEmpty } from '@insight/lib/utils';
import { DETAIL_HEADER_HEIGHT_ETC_PX, BaseSummary, SelectContentViewProps } from './SystemView';
import { OverallMetrics } from './OverallMetrics';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import { ProjectType } from '../../entity/insight';
import { MemcpyOverallMetrics } from './MemcpyOverallMetrics';
import OperatorDetailView from './OperatorDetailView';
import { KernelE2ETimeTable } from './KernelE2ETimeTable';
import { FtraceTaskSummary } from './FtraceTaskSummary';

const filterColumn = [
    'name', 'type', 'acceleratorCore', 'taskId', 'inputShapes', 'inputDataTypes',
    'inputFormats', 'outputShapes', 'outputDataTypes', 'outputFormats',
];

const handleSelected = async(rowData: any, props: SelectContentViewProps): Promise<void> => {
    const res = await queryOneKernel({
        rankId: props.card.cardId,
        dbPath: props.card.dbPath,
        name: rowData.name,
        timestamp: rowData.startTime,
        duration: Number((rowData.duration * 1000).toFixed(0)),
    });

    jumpToUnitOperator({
        ...res,
        name: rowData.name,
        id: props.session.isFullDb as boolean ? rowData.id : res.id,
        cardId: props.card.cardId,
        dbPath: props.card.dbPath,
        tid: res.threadId,
        pid: res.pid,
        duration: Number((rowData.duration * 1000).toFixed(0)),
        timestamp: rowData.startTime,
        metaType: res.metaType ?? '',
    });
};

const defaultFilters = {
    name: [],
    type: [],
    accCore: [],
    taskId: [],
    inputShapes: [],
    inputDataTypes: [],
    inputFormats: [],
    outputShapes: [],
    outputDataTypes: [],
    outputFormats: [],
};

const KernelDetails = observer((props: SelectContentViewProps) => {
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const defaultSorter = { field: 'duration', order: 'descend' };
    const [dataSource, setDataSource] = useState<any[]>([]);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [filters, setFilters] = useState(defaultFilters);
    const [rowData, setRowData] = useState<any>({});
    const [loading, setLoading] = useState(false);
    const kernelDetails = useKernelDetails();
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    const status = props.session.units.find((unit: any) => (unit.metadata as CardMetaData).cardId === props.card.cardId)?.phase;
    useEffect(() => {
        updateData(page, sorter, filters);
    }, [sorter, filters, props.card.cardId, props.session.timeAnalysisRange]);
    useEffect(() => {
        if (status === 'download') {
            updateData(page, sorter, filters);
        }
    }, [status]);
    const updateData = async(pages: any, sorters: {field: string;order: string}, filtersConditions: any): Promise<void> => {
        const targetInfo = props.session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === props.card?.cardId);
        if (props.card === undefined || props.card.cardId === '' || targetInfo?.projectType === ProjectType.IE) {
            setDataSource([]);
            setPage(defaultPage);
            return;
        }
        const filterTypes: string[] = [];
        Object.keys(filtersConditions).forEach(key => {
            const filterValue = filtersConditions[key];
            if (filterColumn.includes(key) && filterValue != null) {
                if (Array.isArray((filterValue)) && filterValue.length > 0) {
                    filterTypes.push(JSON.stringify({ columnName: key, value: filterValue[0] }));
                }
            }
        });
        setLoading(true);
        let startTime = props.session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = props.session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(props.session, props.card);
        const sortedField = sorters.field === 'startTimeLabel' ? 'startTime' : sorters.field;
        try {
            const res = await queryKernelDetails({
                rankId: props.card.cardId,
                dbPath: props.card.dbPath,
                pageSize: pages.pageSize,
                current: pages.current,
                orderBy: sorters.order ? sortedField : defaultSorter.field,
                order: sorters.order ?? defaultSorter.order,
                startTime: Math.floor(startTime + timestampOffset),
                endTime: Math.ceil(endTime + timestampOffset),
                coreType: '',
                filterCondition: filterTypes,
            });

            const data = res.kernelDetails.map((item: {
                startTimeLabel: string;
                startTime: number;}) => {
                item.startTimeLabel = getDetailTimeDisplay(item.startTime - timestampOffset);
                return item;
            });
            setDataSource(data);
            setPage({ ...pages, total: res.count });
        } catch {
            setDataSource([]);
            setPage({ ...pages, total: 0 });
        } finally {
            setLoading(false);
        }
    };

    const colums = [
        ...kernelDetails,
        {
            title: t('Click To Timeline'),
            dataIndex: 'click',
            key: 'click',
            ellipsis: true,
            render: (_: any, record: any): JSX.Element => (<Button type="link"
                onClick={(): void => {
                    setRowData(record as any);
                }}>{t('Click')}</Button>),
        },
    ];

    useEffect(() => {
        if (rowData.name === null || rowData.name === undefined) {
            return;
        }
        handleSelected(rowData, props);
    }, [rowData]);
    return (
        (status === 'download' || props.card === undefined || props.card.cardId === '')
            ? <ResizeTable
                onChange={(pagination: any, newFilters: any, newSorter: any): void => {
                    setSorter(newSorter);
                    setFilters(newFilters);
                }}
                loading={loading}
                pagination={getPageData(page, setPage)}
                dataSource={dataSource}
                columns={colums}
                scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
                rowClassName={(record: any): string => {
                    return record.id === rowData.id ? 'selected-row' : 'click-able';
                }}
                size="small"/>
            : <div style={{ display: 'flex', height: '100%' }}>
                <StyledEmpty style={{ margin: 'auto' }}/>
            </div>
    );
});

const LayerAndTrace = summaryAndTraceTypes.map((item) => {
    return observer((props: SelectContentViewProps) => {
        return (
            <BaseSummary
                layerType={item.type}
                request={item.isStats ? querySystemViewDetails : querySystemViewTrace}
                isStats={item.isStats}
                isTrace={item.isTrace}
                columns={item.columns}
                {...props}
            />
        );
    });
});

export const StatsSystemView = [
    OverallMetrics,
    ...LayerAndTrace,
    KernelE2ETimeTable,
    KernelDetails,
    OperatorDetailView,
    MemcpyOverallMetrics,
    FtraceTaskSummary,
];

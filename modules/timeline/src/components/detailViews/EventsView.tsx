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

import React, { useEffect, useState } from 'react';
import { observer } from 'mobx-react';
import { eventViewData, getDefaultColumData, getPageData, queryOneKernel } from './Common';
import { ResizeTable, fetchColumnFilterProps } from '@insight/lib/resize';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';
import type { ThreadMetaData } from '../../entity/data';
import { Button } from '@insight/lib/components';
import { getTimeOffset } from '../../insight/units/utils';
import { useTranslation } from 'react-i18next';
import i18n from '@insight/lib/i18n';
import { DETAIL_HEADER_HEIGHT_ETC_PX, SelectContentViewProps } from './SystemView';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
export interface EventTableData {
    eventDetails: EventDetails[];
    columnList: EventTableColumn[];
    count: number;
}

export interface EventDetails {
    id: string;
    start: number;
    name: string;
    duration: number;
    startTime: string;
    tid?: string;
    pid?: string;
    threadName?: string;
    rankId?: string;
    analysisType?: string;
}

export interface EventTableColumn {
    name: string;
    type: string;
    key: string;
}

interface UpdateDatas {
    pages: { current: number; pageSize: number; total: number };
    sorters: { field: string; order: string };
    filters: any;
    props: any;
    setLoading: React.Dispatch<React.SetStateAction<boolean>>;
    setDataSource: React.Dispatch<React.SetStateAction<any[]>>;
    setPage: React.Dispatch<React.SetStateAction<any>>;
    setEventColum: React.Dispatch<React.SetStateAction<string[]>>;
}

const getColumns = (tableColumns: EventTableColumn[], filters: { [key: string]: string[] }): any => {
    const result = [];
    for (const tableColumn of tableColumns) {
        if (tableColumn.key === 'rankId') {
            result.push({ title: i18n.t(`timeline:tableHead.${tableColumn.name}`), dataIndex: tableColumn.key });
        } else if (tableColumn.key === 'name') {
            result.push({ title: i18n.t(`timeline:tableHead.${tableColumn.name}`), dataIndex: tableColumn.key, ...getDefaultColumData(tableColumn.key), ...fetchColumnFilterProps('name', 'Name'), filteredValue: filters.name });
        } else if (tableColumn.key === 'start') {
            result.push({ title: i18n.t(`timeline:tableHead.${tableColumn.name}`), dataIndex: 'startTime', ...getDefaultColumData('startTime') });
        } else {
            result.push({ title: i18n.t(`timeline:tableHead.${tableColumn.name}`), dataIndex: tableColumn.key, ...getDefaultColumData(tableColumn.key) });
        }
    }
    return result;
};

const filterColumn = ['name'];

const defaultPage = { current: 1, pageSize: 10, total: 0 };
const defaultSorter = { field: 'duration', order: 'descend' };
const defaultFilters: { [key: string]: string[] } = { name: [] };
export const EventDetail = observer((props: SelectContentViewProps & { request: any }) => {
    const [dataSource, setDataSource] = useState<any[]>([]);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [filters, setFilters] = useState(defaultFilters);
    const [isLoading, setLoading] = useState(false);
    const [eventColum, setEventColum] = useState<string[]>([]);
    const [rowData, setRowData] = useState<any>({});
    const [allCondition, setAllCondition] = useState({ showEvent: props.session.showEvent, page, sorter, filters });
    const { t } = useTranslation();

    useEffect(() => {
        setAllCondition({ ...allCondition, page, sorter, filters });
    }, [sorter, filters, page.current, page.pageSize]);
    useEffect(() => {
        setPage(defaultPage);
        setSorter(defaultSorter);
        setFilters(defaultFilters);
        setAllCondition({ ...allCondition, showEvent: props.session.showEvent, page: defaultPage, sorter: defaultSorter, filters: defaultFilters });
    }, [props.session.showEvent, props.session.timeAnalysisRange]);

    useEffect(() => {
        if (props.session.eventUnits === undefined || props.session.eventUnits.length === 0) {
            setDataSource([]);
            setPage(defaultPage);
            setSorter(defaultSorter);
            setFilters(defaultFilters);
            setEventColum([]);
            setAllCondition({ ...allCondition, page: defaultPage, sorter: defaultSorter, filters: defaultFilters });
            return;
        }
        updateData({ pages: allCondition.page, sorters: allCondition.sorter, filters: allCondition.filters, props, setLoading, setDataSource, setPage, setEventColum });
    }, [props.session.timeAnalysisRange, allCondition.showEvent, allCondition.page.current, allCondition.page.pageSize,
        allCondition.sorter.field, allCondition.sorter.order, allCondition.filters, props.session.doReset, t]);

    useEffect(() => {
        if (rowData.name !== null && rowData.name !== undefined) {
            handleSelected(rowData, props);
        }
    }, [rowData]);

    const eventColumns = generateEventColumns(eventColum, setRowData);

    return (
        <div style={{ height: '100%' }}>
            <ResizeTable
                onChange={(pagination: unknown, newFilters: unknown, newsorter: unknown, extra: {action: string}): void => {
                    if (extra.action === 'sort') {
                        setSorter(newsorter as typeof sorter);
                    }
                    if (extra.action === 'filter') {
                        setFilters(newFilters as typeof filters);
                    }
                }}
                pagination={getPageData(page, setPage)} dataSource={dataSource} columns={eventColumns} size="small" loading={isLoading}
                scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
                rowClassName={(record: any): string => {
                    return record.id === rowData.id ? 'selected-row' : 'click-able';
                }}
            />
        </div>
    );
});

const updateData = async ({
    pages,
    sorters,
    filters,
    props,
    setLoading,
    setDataSource,
    setPage,
    setEventColum,
}: UpdateDatas): Promise<void> => {
    const filterTypes: string[] = [];
    Object.keys(filters).forEach(key => {
        const filterValue = filters[key];
        if (filterColumn.includes(key) && filterValue != null) {
            if (Array.isArray((filterValue)) && filterValue.length > 0) {
                filterTypes.push(JSON.stringify({ columnName: key, value: filterValue[0] }));
            }
        }
    });
    setLoading(true);
    const res = await searchData(pages, sorters, filterTypes, props).finally(() => setLoading(false));
    const requestData = props.session.eventUnits?.[0]?.metadata as ThreadMetaData;
    const timestampoffset = getTimeOffset(props.session, requestData);
    const data = res.eventDetails.map((item: any) => {
        item.startTime = getDetailTimeDisplay(item.start - timestampoffset);
        return item;
    });
    setEventColum(getColumns(res.columnList, filters));
    setDataSource(data);
    setPage((prevPage: any) => ({ ...pages, total: res.count }));
};

const generateEventColumns = (
    eventColum: string[],
    setRowData: React.Dispatch<React.SetStateAction<any>>,
): any[] => eventColum.length === 0
    ? []
    : [
        ...eventColum,
        {
            title: i18n.t('timeline:tableHead.Click To Timeline'),
            dataIndex: 'click',
            key: 'click',
            ellipsis: true,
            render: (_: any, record: any) => (
                <Button type="link" onClick={(): void => {
                    setRowData(record);
                }}>
                    {i18n.t('timeline:tableHead.Click')}
                </Button>
            ),
        },
    ];

const handleSelected = async(rowData: any, props: SelectContentViewProps): Promise<void> => {
    if (!Array.isArray(props.session.eventUnits) || props.session.eventUnits.length <= 0) { return; }
    const rankId = props.session.eventUnits[0].metadata.cardId as string;
    const dbPath = props.session.eventUnits[0].metadata.dbPath as string;
    const res = await queryOneKernel({
        rankId,
        dbPath,
        name: rowData.name,
        timestamp: rowData.start,
        duration: rowData.duration,
    });
    const depth = rowData.depth > res.depth ? rowData.depth : res.depth;
    jumpToUnitOperator({
        ...rowData,
        cardId: rankId,
        dbPath,
        timestamp: rowData.start,
        id: res.id,
        tid: res.threadId,
        pid: res.pid,
        depth,
        metaType: res.metaType ?? '',
    });
};

const searchData = async(pages: any, sorters: {field: string;order: string}, filters: string[], prop: any): Promise<EventTableData> => {
    const requestData = prop.session.eventUnits?.[0]?.metadata as ThreadMetaData;
    let startTime = prop.session.timeAnalysisRange?.[0] ?? 0;
    startTime = startTime < 0 ? 0 : startTime;
    let endTime = prop.session.timeAnalysisRange?.[1] ?? 0;
    endTime = endTime < 0 ? 0 : endTime;
    const timestampoffset = getTimeOffset(prop.session, prop.card);
    return await eventViewData({
        rankId: requestData.cardId as string,
        dbPath: requestData.dbPath as string,
        pageSize: pages.pageSize,
        currentPage: pages.current,
        orderBy: sorters.field === 'startTime' ? 'start' : sorters.field ?? defaultSorter.field,
        order: sorters.order ?? defaultSorter.order,
        pid: requestData.processId as string,
        tid: requestData.threadId as string,
        threadIdList: requestData.threadIdList,
        threadName: requestData.threadName,
        processName: requestData.processName as string,
        metaType: requestData.metaType as string,
        filterCondition: filters,
        startTime: Math.floor(startTime + timestampoffset),
        endTime: Math.ceil(endTime + timestampoffset),
    });
};

export const EventView = observer((props: SelectContentViewProps) => {
    return <EventDetail request={eventViewData} {...props} />;
});

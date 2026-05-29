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
import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { DETAIL_HEADER_HEIGHT_ETC_PX, type SelectContentViewProps } from './SystemView';
import { fetchColumnFilterProps, ResizeTable } from '@insight/lib/resize';
import { getDefaultColumData, getPageData, queryTableDataDetails } from './Common';
interface TableData {
    columnAttr: any[];
    tableData: any[];
    totalNum: number;
}

interface FilterCondition {
    col: string;
    content: string;
}
export const TableDataDetail = observer((props: SelectContentViewProps & { selectKey: number }) => {
    const [dataSource, setDataSource] = useState<any[]>([]);
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const [page, setPage] = useState(defaultPage);
    const defaultSorter = { field: '', order: '' };
    const [sorter, setSorter] = useState(defaultSorter);
    const [column, setColumn] = useState<any[]>([]);
    const [isLoading, setLoading] = useState(false);
    const [condition, setCondition] = useState({ page, sorter });
    const [filters, setFilters] = useState<any>();
    useTranslation();
    useEffect(() => {
        setCondition({ page, sorter });
    }, [sorter, page.current, page.pageSize]);
    useEffect(() => {
        const filterconditions: FilterCondition[] = [];
        if (filters !== undefined) {
            Object.keys(filters).forEach(key => {
                const filterValue = filters[key];
                if (Array.isArray((filterValue)) && filterValue.length > 0) {
                    const filterCondition = { col: key, content: filterValue[0] };
                    filterconditions.push(filterCondition);
                }
            });
        }
        const param = {
            rankId: props.card.cardId,
            dbPath: props.card.dbPath,
            pageSize: condition.page.pageSize,
            currentPage: condition.page.current,
            selectKey: props.selectKey as number,
            order: condition.sorter.order,
            orderBy: condition.sorter.field,
            filterconditions,
        };
        if (param.rankId === '' || param.rankId === undefined) {
            setColumn([]);
            setDataSource([]);
            setPage(defaultPage);
            return;
        }
        setLoading(true);
        queryTableDataDetails(param).then((res) => {
            const data = res as TableData;
            const cols = data.columnAttr.map((item) => {
                return { title: item.key as string, dataIndex: item.key, ...getDefaultColumData(item.key), ...fetchColumnFilterProps(item.key, item.key) };
            });
            setColumn(cols);
            setDataSource(data.tableData);
            setPage((prevPage: any) => ({ ...prevPage, total: res.totalNum }));
            setLoading(false);
        });
    }, [condition.page.current, condition.page.pageSize,
        condition.sorter.field, condition.sorter.order, props.selectKey, props.card, filters]);
    return (
        <div style={{ height: '100%' }}>
            <ResizeTable
                onChange={(pagination: unknown, filters: any, newsorter: unknown, extra: {action: string}): void => {
                    if (extra.action === 'sort') {
                        setSorter(newsorter as typeof sorter);
                    }
                    setFilters(filters);
                }}
                rowClassName={(record: any): string => {
                    return 'click-able';
                }}
                pagination={getPageData(page, setPage)} dataSource={dataSource} columns={column} size="small" loading={isLoading}
                scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
            />
        </div>
    );
});
export const TableDataView = observer((props: SelectContentViewProps & { selectKey: number }) => {
    return (
        <TableDataDetail {...props} />
    );
});

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
import React, { useEffect, useMemo, useState } from 'react';
import i18n from '@insight/lib/i18n';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { Tag } from 'antd';
import { ResizeTable, fetchColumnFilterProps } from '@insight/lib/resize';
import { Tooltip } from '@insight/lib/components';
import { Session } from '../entity/session';
import { getBlockTableData } from './dataHandler';
import { generateJsonShow } from '../utils/utils';
const columnRender = (col: any, text: string, record: any, t: TFunction): React.ReactNode => {
    const tags: { [key: string]: boolean } = { 'early-alloc': record.lazyUsed, 'late-free': record.delayedFree, idle: record.longIdle };
    const showTag = Object.keys(tags).filter(tag => tags[tag]);
    if (col.key === 'id') {
        return <>
            <span style={{ marginRight: '5px' }}>{text}</span>
            {showTag.map((tag) => <Tag key={tag} color="red">{t(tag)}</Tag>)}
        </>;
    } else {
        return <Tooltip
            title={col.key === 'attr' && text ? generateJsonShow(text) : text || ''}
            placement="top"
        >
            {text ?? ''}
        </Tooltip>;
    }
};
const getTableColumns = (t: TFunction, session: Session): any => {
    return session.blocksTableHeader.map((col: any) => {
        const item = {
            dataIndex: col.key,
            key: col.key,
            title: t(col.name, { defaultValue: col.name, keyPrefix: 'tableHead' }),
            sorter: col.sortable,
            ellipsis: {
                showTitle: false,
            },
            showSorterTooltip: t(col.name, { keyPrefix: 'tableHeadTooltip', defaultValue: '' }) === ''
                ? true
                : { title: t(col.name, { keyPrefix: 'tableHeadTooltip' }) },
            render: (text: string, record: any): React.ReactNode => columnRender(col, text, record, t),
        };
        if (col.searchable) {
            return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', '')) };
        } else if (col.rangeFilterable) {
            const filterOptions = { min: col.min, max: col.max };
            return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', ''), true, filterOptions) };
        } else {
            return item;
        }
    });
};
const handleFilters = (filters: any, session: Session): void => {
    const newFilters: { [key: string]: string } = {};
    const newRangeFilters: { [key: string]: number[] } = {};
    const oldFilters = Object.keys(filters);
    oldFilters.forEach((key: string) => {
        const isRange = filters[key]?.length === 2;
        if (isRange) {
            newRangeFilters[key] = filters[key].map(Number);
        } else {
            if (filters[key]?.[0] !== undefined) { newFilters[key] = filters[key]?.[0]; }
        }
    });
    runInAction(() => {
        session.blocksFilters = newFilters;
        session.blocksRangeFilters = newRangeFilters;
    });
};
const BlocksTable = observer(({ session }: { session: Session }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const {
        deviceId, eventType, blocksTableData, blocksTableHeader, tableKey,
        blocksCurrentPage, blocksPageSize, blocksTotal, blocksOrder, blocksOrderBy,
        blocksFilters, blocksRangeFilters, maxTime, minTime,
        lazyUsedThreshold, delayedFreeThreshold, longIdleThreshold, onlyInefficient,
        autoFilterPotentialLeaks,
    } = session;
    const [loading, setLoading] = useState(false);
    const defaultDataSource = (process.env.NODE_ENV === 'development' ? [{}] : []);
    const columns = useMemo(() => getTableColumns(t, session), [JSON.stringify(blocksTableHeader), t]);
    const onTableChange = (pagination: any, filters: any, sorter: any, extra: any): void => {
        if (extra.action === 'sort') {
            runInAction(() => {
                if (sorter.order === undefined) {
                    session.blocksOrder = '';
                } else {
                    session.blocksOrder = sorter.order !== 'ascend';
                }
                session.blocksOrderBy = sorter.field;
            });
        }
        if (extra.action === 'filter') {
            handleFilters(filters, session);
        }
    };
    const onChange = (newCurrent: number, newPageSize: number): void => {
        runInAction(() => {
            session.blocksCurrentPage = newCurrent;
            session.blocksPageSize = newPageSize;
        });
    };
    useEffect(() => {
        if (deviceId === '' || maxTime === 0 || maxTime === undefined) return;
        setLoading(true);
        getBlockTableData(session);
        setLoading(false);
    }, [deviceId, eventType, maxTime, minTime, blocksCurrentPage,
        blocksPageSize, blocksOrder, blocksOrderBy, JSON.stringify(blocksFilters), JSON.stringify(blocksRangeFilters),
        JSON.stringify(lazyUsedThreshold), JSON.stringify(delayedFreeThreshold), JSON.stringify(longIdleThreshold),
        onlyInefficient, autoFilterPotentialLeaks,
    ]);
    return (
        <>
            <ResizeTable
                data-testid={'blocksTable'}
                columns={columns}
                dataSource={blocksTableData.length === 0 ? defaultDataSource : blocksTableData.map((item: any, index: number) => ({ ...item, key: `${item.id}_${index}` }))}
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
                scroll={{ x: 150 * columns.length }}
                style={{ height: 400 }}
                loading={loading}
                key={`${tableKey}_BLocks`}
            />
        </>
    );
});

export default BlocksTable;

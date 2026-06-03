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
import { ResizeTable, fetchColumnFilterProps } from '@insight/lib/resize';
import { Tooltip } from '@insight/lib/components';
import { Session } from '../entity/session';
import { getEventTableData } from './dataHandler';
import { generateJsonShow } from '../utils/utils';

const DEFAULT_TABLE_HEIGHT = 400;
const TABLE_CHROME_HEIGHT = 88;
const MIN_TABLE_SCROLL_Y = 120;

const getRecordValue = (record: any, keys: string[]): any => {
    for (const key of keys) {
        if (record?.[key] !== undefined && record?.[key] !== null && record?.[key] !== '') {
            return record[key];
        }
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
            if (!disabled) {
                onClick();
            }
        }}
        onMouseEnter={(event): void => {
            if (!disabled) {
                event.currentTarget.style.textDecoration = 'underline';
            }
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
    if (!isCallStackColumn(col)) {
        return title;
    }
    return <div style={{ maxHeight: 280, overflow: 'auto', maxWidth: 560 }}>{title}</div>;
};

const getTableColumns = (t: TFunction, session: Session): any => {
    const columns = session.eventsTableHeader.map((col: any) => {
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
            render: (text: string, record: any): React.ReactNode => {
                if (session.module !== 'leaks' && isEventIdColumn(col)) {
                    const eventId = toNumber(getRecordValue(record, ['id', 'ID', 'eventId', 'Event ID']));
                    return <LocateLink
                        disabled={!isValidLocateId(eventId)}
                        onClick={(): void => {
                            if (!isValidLocateId(eventId)) return;
                            runInAction(() => {
                                session.pendingEventLocate = { eventId: eventId as number, deviceId: session.deviceId };
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
            return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', '')) };
        } else if (col.rangeFilterable) {
            const filterOptions = { min: col.min, max: col.max };
            return { ...item, ...fetchColumnFilterProps(col.key, col.name.replace(' ', ''), true, filterOptions) };
        } else {
            return item;
        }
    });
    return columns;
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
        session.eventsFilters = newFilters;
        session.eventsRangeFilters = newRangeFilters;
    });
};
const EventsTable = observer(({ session, height }: { session: Session; height?: number }): React.ReactElement => {
    const { t } = useTranslation('leaks');
    const {
        deviceId, eventsTableData, eventsTableHeader, eventsCurrentPage, tableKey,
        eventsPageSize, eventsTotal, eventsOrder, eventsOrderBy, eventsFilters,
        eventsRangeFilters, maxTime, minTime,
    } = session;
    const [loading, setLoading] = useState(false);
    const defaultDataSource = (process.env.NODE_ENV === 'development' ? [{}] : []);
    const columns = useMemo(() => getTableColumns(t, session), [JSON.stringify(eventsTableHeader), session.module, t]);
    const tableHeight = Math.max(height ?? DEFAULT_TABLE_HEIGHT, MIN_TABLE_SCROLL_Y + TABLE_CHROME_HEIGHT);
    const scrollY = Math.max(MIN_TABLE_SCROLL_Y, tableHeight - TABLE_CHROME_HEIGHT);
    const onTableChange = (pagination: any, filters: any, sorter: any, extra: any): void => {
        if (extra.action === 'sort') {
            runInAction(() => {
                if (sorter.order === undefined) {
                    session.eventsOrder = '';
                } else {
                    session.eventsOrder = sorter.order !== 'ascend';
                }
                session.eventsOrderBy = sorter.field;
            });
        }
        if (extra.action === 'filter') {
            handleFilters(filters, session);
        }
    };
    const onChange = (newCurrent: number, newPageSize: number): void => {
        runInAction(() => {
            session.eventsCurrentPage = newCurrent;
            session.eventsPageSize = newPageSize;
        });
    };
    useEffect(() => {
        if (deviceId === '' || maxTime === 0 || maxTime === undefined) return;
        setLoading(true);
        getEventTableData(session);
        setLoading(false);
    }, [deviceId, maxTime, minTime, eventsCurrentPage, eventsPageSize, eventsOrder, eventsOrderBy, JSON.stringify(eventsFilters), JSON.stringify(eventsRangeFilters)]);
    return (
        <>
            <ResizeTable
                data-testid={'eventsTable'}
                columns={columns}
                dataSource={eventsTableData.length === 0 ? defaultDataSource : eventsTableData.map((item: any, index: number) => ({ ...item, key: `${item.id}_${index}` }))}
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
            />
        </>
    );
});

export default EventsTable;

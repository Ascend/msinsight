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
import React, { ReactNode, useEffect, useMemo, useState } from 'react';
import { Table } from 'antd';
import { ColumnFilterIcon } from '@insight/lib/icon';
import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import { TableState, TableViewProps } from './types';
import { useDetailUpdater } from './hooks';
import type { CommonStateProto, TabProto } from './base/Tabs';
import { ResizeTable } from '@insight/lib/resize';
import { AutoKey, getAutoKey } from '../../utils/dataAutoKey';
import type { FilterDropdownProps, FilterValue, SorterResult } from 'antd/lib/table/interface';
import { SliceNameFilter } from './SliceNameFilter';

const TABLE_HEAD_HEIGHT = 35;
const TABLE_SUMMARY_HEIGHT = 30;
const TABLE_MIN_WIDTH = 1260;

// 公共表格会缓存列配置；通过局部 Context 读取最新条件，不用为关键词变化重建列配置。
const NameFilterContext = React.createContext<Pick<React.ComponentProps<typeof SliceNameFilter>,
'includedKeywords' | 'excludedKeywords' | 'activeMode' | 'onApply'> | null>(null);

const generateSummary = (state: TableState, dataSource: Array<AutoKey<object>>): ReactNode => (
    <Table.Summary fixed={'top'}>
        <Table.Summary.Row>
            { state.columns.map((column, index) => (
                <Table.Summary.Cell key={column.key} index={index}>
                    {column.summary?.(dataSource)}
                </Table.Summary.Cell>
            ))}
        </Table.Summary.Row>
    </Table.Summary>
);

export const SelectSimpleTabularDetail = observer(<T extends CommonStateProto>(
    { session, height, detail, tabState, commonState, depsList }: TableViewProps<TabProto, T>) => {
    useEffect(() => {
        runInAction(() => {
            session.selectedMultiSlice = '';
        });
    }, [session.selectedRange]);
    const state = useDetailUpdater(session, detail, tabState, depsList);
    const unit = session.selectedUnits[0];
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceList' });
    const [excludedKeywords, setExcludedKeywords] = useState<string[]>([]);
    const [includedKeywords, setIncludedKeywords] = useState<string[]>([]);
    const [nameFilterMode, setNameFilterMode] = useState<'include' | 'exclude'>('include');
    const [filters, setFilters] = useState<Record<string, FilterValue | null>>({});
    const [sorter, setSorter] = useState<SorterResult<Record<string, unknown>>>({});
    const [selectedKey, setSelectedKey] = useState<string | null>(null);
    const dataSource = useMemo(() => {
        // 两个模式都从完整选中汇总开始；只使用当前模式的关键词，另一侧仅保留输入。
        const keywords = (nameFilterMode === 'include' ? includedKeywords : excludedKeywords).map(keyword => keyword.toLowerCase());
        let filteredData = state.dataSource.filter(row => {
            if (keywords.length === 0) {
                return true;
            }
            const name = String(row.title ?? '').toLowerCase();
            const matches = keywords.some(keyword => name.includes(keyword));
            return nameFilterMode === 'include' ? matches : !matches;
        });
        Object.keys(filters).forEach(key => {
            if (key === 'Name') {
                return;
            }
            const filterValues = filters[key];
            const columnFilter = state.columns.find(col => col.key === key)?.onFilter;
            if (filterValues?.length && columnFilter) {
                filteredData = filteredData.filter(item => filterValues.some(value => columnFilter(value, item)));
            }
        });
        const columnSorter = state.columns.find(col => col.key === sorter.columnKey)?.sorter;
        if (sorter.order && typeof columnSorter === 'function') {
            filteredData.sort((a, b) => {
                const result = columnSorter(a, b, sorter.order);
                return sorter.order === 'ascend' ? result : -result;
            });
        }
        return filteredData;
    }, [state.dataSource, state.columns, includedKeywords, excludedKeywords, nameFilterMode, filters, sorter]);
    const nameFilterProps = useMemo(() => ({
        includedKeywords,
        excludedKeywords,
        activeMode: nameFilterMode,
        onApply: (mode: 'include' | 'exclude', include: string[], exclude: string[]): void => {
            setNameFilterMode(mode);
            setIncludedKeywords(include);
            setExcludedKeywords(exclude);
        },
    }), [includedKeywords, excludedKeywords, nameFilterMode]);
    // 弹窗开关、筛选图标及排序状态交回表格管理，避免它们改变外部列配置并覆盖拖动宽度。
    const columns = useMemo(() => state.columns.map(column => column.key !== 'Name'
        ? column
        : {
            ...column,
            // 公共表格按 dataIndex 读取筛选状态，与本列 key 对齐；名称仍由原 render 展示。
            dataIndex: column.key as string,
            onFilter: undefined,
            filterIcon: (filtered: boolean) => <span role="img" aria-label={t('Name filter')}><ColumnFilterIcon active={filtered} /></span>,
            filterDropdown: (props: FilterDropdownProps) => <NameFilterContext.Consumer>
                {options => options && <SliceNameFilter {...props} {...options} />}
            </NameFilterContext.Consumer>,
        }), [state.columns, t]);
    const scrollY = useMemo(() => {
        let val = height - TABLE_HEAD_HEIGHT;
        if (dataSource?.length) {
            val = val - TABLE_SUMMARY_HEIGHT;
        }
        return Math.max(0, val);
    }, [height, dataSource?.length]);
    useEffect(() => {
        if (selectedKey !== null && !dataSource.some(row => (state.rowKey?.(row) ?? getAutoKey(row)) === selectedKey)) {
            setSelectedKey(null);
            runInAction(() => { session.selectedMultiSlice = ''; });
        }
    }, [dataSource, selectedKey, state.rowKey, session]);
    // 新增Summary(Totals)行
    const summary = (): React.ReactNode => dataSource?.length ? generateSummary(state, dataSource) : undefined;
    return (
        <NameFilterContext.Provider value={nameFilterProps}>
            <ResizeTable
                className={'table-slice-list'}
                {...state}
                columns={columns}
                summary={summary}
                dataSource={dataSource}
                allowCopy
                scroll={{ y: scrollY, x: TABLE_MIN_WIDTH }} virtual
                rowClassName={(row): string => {
                    return selectedKey !== null && selectedKey === (state.rowKey?.(row) ?? getAutoKey(row)) ? 'selected-row' : 'click-able';
                }}
                showSorterTooltip={false}
                expandable={{ showExpandColumn: false }}
                onRow={(row): React.HTMLAttributes<any> => ({
                    onClick: async (): Promise<void> => {
                        detail?.clickCallback?.({ row, session, detail, unit, commonState });
                        setSelectedKey(state.rowKey?.(row) ?? getAutoKey(row));
                        if (detail?.fetchMoreData !== undefined && detail.more?.field !== undefined) {
                            row[detail.more?.field] = await detail?.fetchMoreData(session, row).catch(() => []);
                        }
                    },
                    onDoubleClick: (): void => {
                        setSelectedKey(state.rowKey?.(row) ?? getAutoKey(row));
                        detail?.doubleClickCallback?.({ row, session, detail, unit, commonState });
                    },
                    onMouseEnter: (): void => {
                        detail?.mouseEnterCallback?.({ session, row });
                    },
                    onMouseLeave: (): void => {
                        detail?.mouseLeaveCallback?.({ session, row });
                    },
                })}
                onChange={(pagination, nextFilters, nextSorter): void => {
                    setFilters(nextFilters);
                    setSorter(nextSorter as SorterResult<Record<string, unknown>>);
                }}
            />
        </NameFilterContext.Provider>
    );
});

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
import React, { cloneElement, useEffect, useImperativeHandle, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { type PaginationProps, Table } from 'antd';
import type { ColumnGroupType, ColumnsType, ColumnType, TablePaginationConfig, TableProps } from 'antd/es/table';
import { isArray } from 'lodash';
import { Resizor } from './Resizor';
import { getColumnSearchProps } from './ColumnFilterWithSelection';
import { copyTableToClipboard, limitInput, StyledEmpty } from '../utils';
import { useWatchVirtualRender } from '../utils/VirtualRenderUtils';
import { CaretRightIcon, CopyOutlinedIcon } from '../icon/Icon';
import type { JSX } from '@emotion/react/jsx-runtime';
import type { FilterValue } from 'antd/lib/table/interface';
import { safeStringify } from '../utils/safeStringify';

export interface ResizeTableRef {
    clearAllFilters: () => void;
    clearAllSorts: () => void;
    getVirtualBoxDom: () => HTMLDivElement | null;
    // ... 其他需要暴露的方法
}

const Support = React.forwardRef(
    (props: ResizeTableProps<any>, _ref: React.ForwardedRef<unknown>) => {
        return <Table {...props} />;
    },
);
Support.displayName = 'table';

const StyledTable = styled(Support)`
    .ant-table {
        background-color: unset;
        font-size: 12px;
        color: ${(p): string => p.theme.tableTextColor};
    }

    //表头
    .ant-table-thead > tr > th {
        background-color: ${(p): string => p.theme.bgColorLight};
        color: ${(p): string => p.theme.textColorSecondary};
        box-shadow: inset 0 -1px 0 0 ${(p): string => p.theme.borderColorLight};
        border-bottom: none;
        user-select: text;
    }

    .ant-table-thead th.ant-table-column-has-sorters:hover {
        background: ${(p): string => p.theme.bgColorLight};
    }

    .ant-table-thead > tr > th, .ant-table tfoot > tr > th, .ant-table.ant-table-small .ant-table-thead > tr > th{
        padding: 8px;
        line-height: 16px;
        height: 32px;
    }

    .ant-table-thead > tr > th:not(:last-child):not(.ant-table-selection-column):not(.ant-table-row-expand-icon-cell):not([colspan])::before {
        background-color: ${(p): string => p.theme.borderColorLight};
    }
    .ant-table-thead th.ant-table-column-has-sorters:hover::before {
        background-color: ${(p): string => p.theme.borderColorLight} !important;
    }

    //行
    .ant-table-tbody > tr.ant-table-row:hover > td, .ant-table-tbody > tr > td.ant-table-cell-row-hover {
        background: ${(p): string => p.rowHoverable ? p.theme.bgColorLight : 'unset'};
    }
    .ant-table-tbody > tr.ant-table-placeholder:hover > td {
        background: unset;
    }

    .ant-table-tbody > tr > td {
        box-shadow: inset 0 -1px 0 0 ${(p): string => p.theme.borderColorLight};
        border-bottom: none;
        user-select: text;
    }

    .ant-table-tbody > tr > td:has(.ant-empty) {
        box-shadow: none;
    }

    .ant-table-cell-scrollbar:not([rowspan]) {
        box-shadow: 0 1px 0 1px ${(p): string => p.theme.borderColorLight};
    }

    .ant-table-tbody > tr > td, .ant-table tfoot > tr > td {
        padding: 8px;
        line-height: 16px;
    }
    .ant-table-tbody > tr > td.height20 {
        padding: 6px 8px;
        line-height: 20px;
    }
    .ant-empty-normal{
        color:${(p): string => p.theme.textColorTertiary};
        user-select: none;
    }

    tr.ant-table-row.click-able:hover > td.ant-table-cell {
        background: ${(p): string => p.theme.bgColorLight};
        cursor: pointer;
    }

    tr.ant-table-row.click-select > td.ant-table-cell {
        background: ${(p): string => p.theme.primaryColorLight6};
        color: ${(p): string => p.theme.textColorPrimary};
    }

    tr.ant-table-row.selected-row > td.ant-table-cell {
        background: ${(p): string => p.theme.bgColorLight};
    }

    //筛选
    .ant-table-filter-trigger {
        padding: 0;
    }
    .ant-table-filter-trigger:hover {
        color: #a6a6a6;
        background: rgba(0, 0, 0, 0.04);
    }

    .ant-table-filter-trigger.active {
        color: ${(p): string => p.theme.primaryColor};
    }

    //排序
    td.ant-table-column-sort {
        background: none;
    }
    .ant-table-column-sorter {
        height: 16px;
        margin-top: -2px;
    }

    //分页
    .ant-pagination {
        color: ${(p): string => p.theme.textColorSecondary};
    }

    .ant-pagination * {
        font-size: 12px;
    }
    //固定列
    td.ant-table-cell-fix-right {
        background-color: ${(p): string => p.theme.bgColor};
    }

    .ant-pagination-item {
        background: none;
        border-color: transparent;
    }

    .ant-pagination-item-active.ant-pagination-item a {
        color: ${(p): string => p.theme.primaryColor};
    }

    .ant-pagination-item a {
        color: ${(p): string => p.theme.textColorSecondary};
        font-size: 12px;
    }

    .ant-pagination-prev button, .ant-pagination-next button {
        color: ${(p): string => p.theme.textColorTertiary};
    }

    .ant-pagination-disabled .ant-pagination-item-link, .ant-pagination-disabled:hover .ant-pagination-item-link {
        color: ${(p): string => p.theme.textColorDisabled};
    }

    .ant-select:not(.ant-select-customize-input) .ant-select-selector {
        background-color: unset;
        border-color: ${(p): string => p.theme.borderColorLighter};
    }

    .ant-select {
        color: ${(p): string => p.theme.textColorSecondary};
    }

    .ant-pagination-options-quick-jumper input {
        border-color: ${(p): string => p.theme.borderColorLighter};
        background-color: unset;
        color: ${(p): string => p.theme.textColorSecondary};
    }

    .ant-select-dropdown {
        background-color: ${(p): string => p.theme.bgColor};
        color: ${(p): string => p.theme.textColorSecondary};
    }

    .ant-select-item {
        color: ${(p): string => p.theme.textColorSecondary};
    }

    .ant-select-item-option-selected:not(.ant-select-item-option-disabled) {
        color: ${(p): string => p.theme.textColorSecondary};
        background-color: ${(p): string => p.theme.primaryColor};
    }

    .ant-select-item-option-active:not(.ant-select-item-option-disabled) {
        background-color: ${(p): string => p.theme.primaryColorLight6};
    }

    //嵌套表格
    .ant-table-tbody > tr.ant-table-expanded-row > td {
        padding: 16px 16px 16px 40px;
        background: unset;
    }
    .ant-table.ant-table-small .ant-table-tbody .ant-table-wrapper:only-child .ant-table {
        margin:0;
    }

    //按钮
    .ant-btn-link {
        padding: 0;
        height: 16px;
        line-height: 16px;
        font-size: 12px;
        border: none;
    }

    .ant-table-row-expand-icon {
      background: ${(p): string => p.theme.bgColorDark};
    }

    // summary 汇总行
    .ant-table-summary {
        background: ${(p): string => p.theme.bgColorLight};
    }

    //表格为空时
    .ant-table.ant-table-small .ant-table-expanded-row-fixed {
        margin: -8px -16px;
    }
`;

const ResizeTableContainer = styled.div`
    .exportTableBtn {
        position: absolute;
        width: 28px;
        height: 28px;
        border-radius: 50%;
        right: 10px;
        top: 34px;
        z-index: 10;
        cursor: pointer;
        background-color: ${(p): string => p.theme.borderColorLight};
        color: ${(p): string => p.theme.textColorSecondary};
        display: none;
        &:hover {
            color: ${(p): string => p.theme.radioSelectedColor};
        }
    }
    &:hover {
        .exportTableBtn {
            display: block;
        }
    }
`;

interface ExtendsColumnType { minWidth?: number };

interface IResizableTitleProps extends React.ReactElement {
    index?: number;
    className: string;
    resizable: boolean;
    onResize: (deltaX: number, width: number, nextWidth?: number) => void;
}
const resizableTitle: React.FC<IResizableTitleProps> = (props) => {
    const { onResize, resizable, index, ...restProps } = props;
    const th: React.ReactElement = <th {...restProps} /> as React.ReactElement;
    if (props?.className?.includes('ant-table-row-expand-icon-cell') || !resizable) {
        return th;
    }
    return cloneElement(th, {},
        [...th.props.children,
            <Resizor key={th.props.children.length} onResize={onResize} style={{ width: 8, right: -4 }} />]);
};

interface ResizeTableProps<T> extends TableProps<T> {
    id?: string;
    variableTotalWidth?: boolean;
    minThWidth?: number;
    style?: object;
    virtual?: boolean;
    scroll?: { x?: string | number | true; y?: number; rowHeight?: number; scrollToFirstRowOnChange?: boolean };
    rowHoverable?: boolean;
    allowCopy?: boolean;
    resetScroll?: boolean;
}
type TablePaginationPosition = 'topLeft' | 'topCenter' | 'topRight' | 'bottomLeft' | 'bottomCenter' | 'bottomRight';

// ============================ Resize ============================
interface IParams {
    columns: any[];
    setColumns: (cols: ColumnsType<any>) => void;
    index: number;
    width: number;
    nextWidth?: number;
    minThWidth: number;
    variableTotalWidth: boolean;
}
const resizeColumns = ({ columns, setColumns, index, width, nextWidth, minThWidth, variableTotalWidth }: IParams): void => {
    if (width < minThWidth) {
        return;
    }
    const newColumns = getResizeColumns({ columns, index, width, nextWidth, minThWidth, variableTotalWidth });
    setColumns(newColumns);
};
const getResizeColumns = ({ columns, index, width, nextWidth, minThWidth, variableTotalWidth }:
{ columns: any[]; index: number; width: number; nextWidth?: number; minThWidth: number; variableTotalWidth: boolean }): any[] => {
    const newColumns = [...columns];
    newColumns[index] = {
        ...newColumns[index],
        width: Math.max(width, minThWidth, (columns[index] as ExtendsColumnType).minWidth ?? 0),
    };
    if (nextWidth !== null && nextWidth !== undefined && !variableTotalWidth) {
        newColumns[index + 1] = {
            ...newColumns[index + 1],
            width: Math.max(nextWidth, minThWidth, (columns[index + 1] as ExtendsColumnType).minWidth ?? 0),
        };
    }
    return newColumns;
};

// ============================ virtual ============================
const getVirtualElement = (dom: Element, boxRef: React.RefObject<HTMLElement>, targetRef: React.RefObject<HTMLElement>):
[Element | null, Element | null] => {
    const box = dom.querySelector('.ant-table-body');
    const target = dom.querySelector('.ant-table-body table');
    if (box !== null && target !== null) {
        (boxRef as any).current = box;
        (targetRef as any).current = target;
    }
    return [box, target];
};

// ============================ pagination ============================
const showTotal: PaginationProps['showTotal'] = total => `Total ${total} items`;
const getFullPagination = (pagination?: false | TablePaginationConfig, total?: number): false | TablePaginationConfig => {
    if (typeof pagination === 'boolean') {
        return false;
    }
    const size: 'default' | 'small' = 'small';
    const position: TablePaginationPosition[] = ['bottomLeft'];
    return { size, position, showTotal, total, showQuickJumper: true, showSizeChanger: true, ...pagination ?? {} };
};

// ============================ expandable ============================
const getFullExpandable = (expandable?: any): any => {
    if (expandable === null || expandable === undefined || typeof expandable !== 'object') {
        return null;
    }
    const expandIcon = <T extends { children?: unknown[] }>({ expandable: _expandable, expanded, onExpand, record }:
    { expandable: boolean; expanded: boolean; onExpand: (record: T, event: React.MouseEvent<any>) => void; record: T }): React.ReactNode => {
        if (_expandable) {
            return <CaretRightIcon onClick={(e): void => {
                e.stopPropagation();
                onExpand(record, e);
            }} style={{ cursor: 'pointer', transform: `rotate(${expanded ? '90deg' : 0})` }} />;
        } else {
            return <></>;
        }
    };
    return { expandIcon, ...expandable };
};

// ============================ 安全防护 ============================
const handleChangeSafe = (onChange?: (...p: any) => void, ...params: any): void => {
    const [, , , action] = params;
    if (['paginate', 'filter'].includes(action?.action)) {
        limitInput();
    }
    if (onChange !== null && onChange !== undefined) {
        onChange(...params);
    }
};

// 清除所有列过滤
const clearAllFilters = (mergeColumns: ColumnsType<any>, setFiltersState: (val: Record<string, any[]>) => void): void => {
    const empty: Record<string, any[]> = {};

    mergeColumns?.forEach((col: any) => {
        const key = col.dataIndex;
        if (typeof key === 'string') {
            empty[key] = [];
        }
    });

    setFiltersState(empty);
};

// 重置 filteredValue
const handleFilteredValueReset = (filters: Record<string, FilterValue | null>, setFiltersState: any): void => {
    Object.entries(filters).forEach(([key, val]) => {
        setFiltersState((prev: Record<string, any[]>) => ({
            ...prev,
            [key]: val,
        }));
    });
};

// 重置 sortOrder
const handleSortOrderReset = (sorter: any, setSortState: (val: SortState) => void): void => {
    if ('order' in sorter) {
        setSortState({
            columnKey: sorter.field as string,
            order: sorter.order || null,
        });
    } else {
        setSortState({});
    }
};

// 每列的 filterState 值
const getFilteredValue = (col: ColumnType<any> | ColumnGroupType<any>, filtersState: Record<string, any[]>): null | any => {
    if ('children' in col) return null;

    const idx = col.dataIndex;

    if (typeof idx === 'string' || typeof idx === 'number') {
        return filtersState[idx];
    }
    return null;
};

// 每列的 sortOrder 值
const getSortedValue = (col: ColumnType<any> | ColumnGroupType<any>, sortState: SortState): null | any => {
    if ('children' in col) return null;
    const key = col.dataIndex as string | undefined;
    return key && key === sortState.columnKey
        ? sortState.order ?? null
        : null;
};

interface SortState {
    columnKey?: string;
    order?: 'ascend' | 'descend' | null;
}

const EMPTY_VIEW_HEIGHT = 60;
export function ResizeTableInner<T extends object>(prop: ResizeTableProps<T>, ref: React.Ref<ResizeTableRef>): JSX.Element {
    const {
        columns: propColumns, variableTotalWidth = false, minThWidth = 50, id, style, virtual = false,
        scroll, dataSource, pagination, expandable, onChange, rowHoverable = true,
        className, locale, allowCopy = true, resetScroll = true, ...restProps
    } = prop;
    const [columns, setColumns] = useState<ColumnsType<T>>([]);
    const marginY = scroll?.y ? (scroll.y - EMPTY_VIEW_HEIGHT) / 2 : 50;
    const [filtersState, setFiltersState] = useState<Record<string, any[]>>({});
    const [sortState, setSortState] = useState<SortState>({});

    // ============================ Resize ============================
    useEffect(() => { setColumns(propColumns ?? []); }, [safeStringify(propColumns)]);

    const mergeColumns: any = useMemo(() => columns.map((col, index) => ({
        ...col,
        onHeaderCell: () => ({
            onResize: (_diff: number, width: number, nextWidth?: number): void => resizeColumns({ columns, setColumns, index, width, nextWidth, minThWidth, variableTotalWidth }),
            resizable: variableTotalWidth || (propColumns !== undefined && index !== propColumns.length - 1),
        }),
        // ============================ filters ============================
        ...((isArray(col.filters) && col.filters.length > 0) ? getColumnSearchProps() : {}),
        ...(col.onFilter ? {} : { filteredValue: getFilteredValue(col, filtersState) }),
        ...(col.sorter === true ? { sortOrder: getSortedValue(col, sortState) } : {}),
    })), [columns, filtersState, sortState]);

    // ============================ virtual ============================
    const innerTableRef = useRef(null);
    const { data: renderList, boxRef, targetRef } = useWatchVirtualRender({ visibleHeight: scroll?.y ?? 0, itemHeight: scroll?.rowHeight ?? 32, dataSource: dataSource ?? [], resetScroll });
    useEffect(() => { if (virtual && innerTableRef.current !== null) { getVirtualElement(innerTableRef.current as Element, boxRef, targetRef); } }, []);

    // ============================ pagination ============================
    const fullPagination = useMemo(() => getFullPagination(pagination, dataSource?.length), [pagination]);

    // ============================ expandable ============================
    const fullExpandable = useMemo(() => getFullExpandable(expandable), [expandable]);

    const copyTable = async (): Promise<void> => { await copyTableToClipboard(columns, dataSource as any[]); };

    useImperativeHandle(ref, (): ResizeTableRef => ({
        clearAllFilters: () => clearAllFilters(mergeColumns, setFiltersState),
        clearAllSorts(): void { setSortState({}); },
        getVirtualBoxDom(): HTMLDivElement | null { return boxRef.current; },
    }));

    // 出现分页跳转输入框
    useEffect(() => { limitInput(); }, [dataSource?.length, fullPagination]);

    const handleTableChange: TableProps<any>['onChange'] = (...params): void => {
        const filters = params[1]; const sorter = params[2];

        handleFilteredValueReset(filters, setFiltersState);
        handleSortOrderReset(sorter, setSortState);
        handleChangeSafe(onChange, ...params);
    };

    return (
        <ResizeTableContainer id={id} style={{ ...(style ?? {}), position: 'relative' }} ref={innerTableRef}>
            {(allowCopy && (dataSource ?? []).length > 0) && <div className="exportTableBtn" onClick={copyTable}><CopyOutlinedIcon style={{ width: '100%', height: '100%', lineHeight: '32px', display: 'inline-block' }} /></div>}
            <StyledTable {...restProps} onChange={handleTableChange}
                pagination={virtual ? false : fullPagination} expandable={fullExpandable} rowHoverable={rowHoverable} scroll={scroll}
                dataSource={virtual ? renderList : dataSource} components={{ header: { cell: resizableTitle } }}
                className={`${className ?? ''} ${variableTotalWidth ? 'variableTotalWidth' : ''}`} columns={mergeColumns}
                locale={{ emptyText: () => prop.loading ? null : <StyledEmpty style={{ marginTop: marginY, marginBottom: marginY }}></StyledEmpty>, ...(locale ?? {}) }}
            />
        </ResizeTableContainer>
    );
};

export const ResizeTable = React.forwardRef(ResizeTableInner) as <T extends object>(
    props: ResizeTableProps<T> & { ref?: React.Ref<ResizeTableRef> }
) => React.ReactElement;

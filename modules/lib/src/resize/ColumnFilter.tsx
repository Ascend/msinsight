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
import React from 'react';
import { message } from 'antd';
import type { ColumnType } from 'antd/es/table';
import { Button, Input, InputNumber } from '../components/index';
import { SearchOutlined } from '@ant-design/icons';
import type { FilterDropdownProps, Key } from 'antd/es/table/interface';
import { limitInput } from '../utils';
import i18n from '../i18n';
import { ButtonGroup } from './ColumnFilterWithSelection';
import { ColumnFilterIcon } from '../icon/Icon';
import { ValueType } from '../utils/VirtualUl';

interface FilterProps {
    setSelectedKeys: (selectedKeys: React.Key[]) => void;
    selectedKeys: React.Key[];
    confirm: () => void;
    clearFilters?: () => void;
    dataIndex: string;
}

interface FilterOptions {
    max?: number;
    min?: number;
    precision?: number;
}

const state = {
    searchText: '',
    searchedColumn: '',
    searchInput: null,
    searchMinInput: null,
    searchMaxInput: null,
    searchMin: '',
    searchMax: '',
};
const handleSearch = (
    selectedKeys: string[],
    confirm: FilterDropdownProps['confirm'],
    dataIndex: string,
    setSelectedKeys?: (selectedKeys: Key[]) => void,
): void => {
    const trimmed = selectedKeys[0]?.trim() ?? '';
    setSelectedKeys?.([trimmed]);
    confirm();
    state.searchText = trimmed;
    state.searchedColumn = dataIndex;
};
const handleReset = (
    clearFilters: () => void,
    confirm: FilterDropdownProps['confirm'],
    selectedKeys: string[],
    dataIndex: string,
): void => {
    state.searchText = '';
    clearFilters();
    handleSearch(selectedKeys as string[], confirm, dataIndex);
};
const handleRange = (
    selectedKeys: string[],
    confirm: FilterDropdownProps['confirm'],
    dataIndex: string,
    setSelectedKeys?: (selectedKeys: string[]) => void,
    filterOptions?: FilterOptions,
): void => {
    const min = selectedKeys[0] === undefined ? String(filterOptions?.min ?? 0) : String(selectedKeys[0]);
    const max = selectedKeys[1] === undefined ? String(filterOptions?.max ?? Number.MAX_SAFE_INTEGER) : String(selectedKeys[1]);
    if (Number(min) > Number(max)) {
        message.error(i18n.t('buttonText:limitMinMax'));
        return;
    }
    setSelectedKeys?.([min, max]);
    confirm();
    state.searchMin = '';
    state.searchMax = '';
    state.searchedColumn = dataIndex;
};
const handleRangeReset = (
    clearFilters: () => void,
    confirm: FilterDropdownProps['confirm'],
    dataIndex: string,
    setSelectedKeys: (selectedKeys: string[]) => void,
): void => {
    state.searchText = '';
    state.searchMin = '';
    state.searchMax = '';
    state.searchedColumn = dataIndex;
    clearFilters();
    setSelectedKeys([]);
    confirm();
};
const filterSearch = (params: FilterProps, columnTitle: string) => {
    const { setSelectedKeys, selectedKeys, confirm, clearFilters, dataIndex } = params;
    return (
        < div style={{ padding: 8 }} onKeyDown={(e): void => e.stopPropagation()}>
            <Input
                ref={(node): void => {
                    state.searchInput = node as null;
                }}
                placeholder={`${i18n.t('buttonText:Search')} ${i18n.t(`filterColumnName:${columnTitle}`)}`}
                value={selectedKeys[0]}
                onChange={(e): void => setSelectedKeys(e.target.value ? [e.target.value] : [])}
                onPressEnter={(): void => handleSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                style={{ marginBottom: 8, display: 'block' }}
            />
            <ButtonGroup>
                <Button
                    type="primary"
                    onClick={(): void => handleSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                    icon={<SearchOutlined />} size="small" style={{ marginRight: 8 }}
                >{i18n.t('buttonText:Search')}</Button>
                <Button
                    onClick={(): void => clearFilters && handleReset(clearFilters, confirm, selectedKeys as string[], dataIndex)}
                    size="small"
                >{i18n.t('buttonText:Reset')}</Button>
            </ButtonGroup>
        </div >
    );
};
const rangeChange = (value: ValueType | null, selectedKeys: FilterProps['selectedKeys'], setSelectedKeys: FilterProps['setSelectedKeys'], key: number) => {
    const newSelectKeys = [...selectedKeys];
    if (value !== null) {
        newSelectKeys[key] = String(value);
        setSelectedKeys(newSelectKeys);
    } else {
        newSelectKeys[key] = undefined as unknown as Key;
        setSelectedKeys(newSelectKeys);
    }
};
const filterRange = (params: FilterProps, filterOptions?: FilterOptions) => {
    const { setSelectedKeys, selectedKeys, confirm, clearFilters, dataIndex } = params;
    return (<div style={{ padding: 8 }} onKeyDown={(e): void => e.stopPropagation()}>
        <InputNumber
            ref={(node): void => { state.searchMinInput = node as null; }}
            placeholder={i18n.t('buttonText:Min')}
            value={selectedKeys[0]}
            onChange={(value): void => { rangeChange(value, selectedKeys, setSelectedKeys, 0); }}
            onPressEnter={(): void => handleRange(selectedKeys as string[], confirm, dataIndex, setSelectedKeys, filterOptions)}
            size="small" style={{ marginBottom: 8, marginRight: 8 }}
            min={filterOptions?.min ?? 0} max={filterOptions?.max ?? Number.MAX_SAFE_INTEGER}
            precision={filterOptions?.precision ?? 3}
        />
        <InputNumber
            ref={(node): void => { state.searchMaxInput = node as null; }}
            placeholder={i18n.t('buttonText:Max')}
            value={selectedKeys[1]}
            onChange={(value): void => { rangeChange(value, selectedKeys, setSelectedKeys, 1); }}
            onPressEnter={(): void => handleRange(selectedKeys as string[], confirm, dataIndex, setSelectedKeys, filterOptions)}
            size="small" style={{ marginBottom: 8 }}
            min={filterOptions?.min ?? 0} max={filterOptions?.max ?? Number.MAX_SAFE_INTEGER}
            precision={filterOptions?.precision ?? 3}
        />
        <ButtonGroup>
            <Button
                type="primary"
                onClick={(): void => handleRange(selectedKeys as string[], confirm, dataIndex, setSelectedKeys, filterOptions)}
                size="small" style={{ marginRight: 8 }}
            >{i18n.t('buttonText:Search')}</Button>
            <Button
                onClick={(): void => clearFilters && handleRangeReset(clearFilters, confirm, dataIndex, setSelectedKeys)}
                size="small"
            >{i18n.t('buttonText:Reset')}</Button>
        </ButtonGroup>
    </div>);
};

export function fetchColumnFilterProps(columnDataIndex: string, columnTitle: string, showRange: boolean = false,
    filterOptions?: FilterOptions, onFilter?: (value: string) => void): ColumnType<any> {
    const getColumnFilterProps = (dataIndex: string): ColumnType<any> => ({
        filterDropdown: ({ setSelectedKeys, selectedKeys, confirm, clearFilters }): React.ReactNode => {
            const params = { setSelectedKeys, selectedKeys, confirm, clearFilters, dataIndex };
            const handleSearchWithCallback = (keys: string[], conf: FilterDropdownProps['confirm'], idx: string, setKeys?: (selectedKeys: Key[]) => void): void => {
                handleSearch(keys, conf, idx, setKeys);
                if (onFilter && keys[0]) {
                    onFilter(keys[0].trim());
                }
            };
            const handleResetWithCallback = (clear: () => void, conf: FilterDropdownProps['confirm'], keys: string[], idx: string): void => {
                handleReset(clear, conf, keys, idx);
                if (onFilter) {
                    onFilter('');
                }
            };
            return showRange
                ? filterRange(params, filterOptions)
                : filterSearchWithCallback(params, columnTitle, handleSearchWithCallback, handleResetWithCallback);
        },
        filterIcon: () => (
            <ColumnFilterIcon />
        ),
        onFilterDropdownOpenChange: (visible): void => {
            if (visible) {
                limitInput();
            }
        },
    });
    return getColumnFilterProps(columnDataIndex);
}

const filterSearchWithCallback = (
    params: FilterProps,
    columnTitle: string,
    onSearch: (keys: string[], conf: FilterDropdownProps['confirm'], idx: string, setKeys?: (selectedKeys: Key[]) => void) => void,
    onReset: (clear: () => void, conf: FilterDropdownProps['confirm'], keys: string[], idx: string) => void,
): React.ReactNode => {
    const { setSelectedKeys, selectedKeys, confirm, clearFilters, dataIndex } = params;
    return (
        < div style={{ padding: 8 }} onKeyDown={(e): void => e.stopPropagation()}>
            <Input
                ref={(node): void => {
                    state.searchInput = node as null;
                }}
                placeholder={`${i18n.t('buttonText:Search')} ${i18n.t(`filterColumnName:${columnTitle}`)}`}
                value={selectedKeys[0]}
                onChange={(e): void => setSelectedKeys(e.target.value ? [e.target.value] : [])}
                onPressEnter={(): void => onSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                style={{ marginBottom: 8, display: 'block' }}
            />
            <ButtonGroup>
                <Button
                    type="primary"
                    onClick={(): void => onSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                    icon={<SearchOutlined />} size="small" style={{ marginRight: 8 }}
                >{i18n.t('buttonText:Search')}</Button>
                <Button
                    onClick={(): void => clearFilters && onReset(clearFilters, confirm, selectedKeys as string[], dataIndex)}
                    size="small"
                >{i18n.t('buttonText:Reset')}</Button>
            </ButtonGroup>
        </div >
    );
};

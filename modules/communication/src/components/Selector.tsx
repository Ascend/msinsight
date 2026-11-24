/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
import { Select } from '@insight/lib/components';
import { FilterLabel } from './filters';

interface SelectorOption { value: string; label: string }

export function Selector<T extends SelectorOption>({ label, value, options, onChange, loading = false, width = 210, optionRender, dropdownMatchSelectWidth }: {
    label: string; value: string; options: T[]; onChange: (value: string) => void; loading?: boolean; width?: number | string;
    optionRender?: (option: T) => JSX.Element; dropdownMatchSelectWidth?: boolean;
}): JSX.Element {
    return <FilterLabel>{label}<Select<T> width={width} aria-label={label} value={value || undefined} options={options}
        optionLabelProp="label" optionRender={optionRender} dropdownMatchSelectWidth={dropdownMatchSelectWidth}
        showSearch optionFilterProp="label" loading={loading} disabled={!options.length} onChange={onChange} /></FilterLabel>;
}
export const options = (values: string[]): Array<{ value: string; label: string }> => values.map(value => ({ value, label: value }));

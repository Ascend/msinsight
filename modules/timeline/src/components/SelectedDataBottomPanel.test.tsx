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

import { render } from '@testing-library/react';
import React from 'react';
import type { AscendSliceDetail } from '../entity/data';
import { ArgsData } from './SelectedDataBottomPanel';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string) => string } => ({
        t: (key: string): string => key,
    }),
}));

jest.mock('@insight/lib/icon', () => ({
    CaretDownIcon: (): JSX.Element => <span/>,
    AlarmIcon: (): JSX.Element => <span data-testid="alarm-icon"/>,
}));

let mockTooltipProps: any;

jest.mock('@insight/lib/components', () => ({
    Tooltip: (props: { children: React.ReactNode }): JSX.Element => {
        mockTooltipProps = props;
        return <>{props.children}</>;
    },
}));

jest.mock('@insight/lib/resize', () => ({
    ResizeTable: (): JSX.Element => <table/>,
    fetchColumnFilterProps: (): Record<string, never> => ({}),
}), { virtual: true });

const renderArgs = (args: Record<string, unknown>): { rows: HTMLElement[]; text: string } => {
    const detail = { args: JSON.stringify(args) } as AscendSliceDetail;
    const { container } = render(<ArgsData data={detail}/>);
    return {
        rows: Array.from(container.querySelectorAll('.key')),
        text: container.textContent ?? '',
    };
};

const warnedKeys = (rows: HTMLElement[]): string[] => {
    return rows
        .filter((row) => row.querySelector('[data-testid="alarm-icon"]') !== null)
        .map((row) => row.textContent ?? '');
};

describe('ArgsData ambiguous fields', () => {
    it('warns only on the fields listed by the backend and hides the marker', () => {
        const { rows, text } = renderArgs({
            notifyId: '0',
            taskType: 'AllReduce',
            'size(Byte)': '40',
            '_ambiguousKeys': ['notifyId', 'taskType'],
        });

        expect(text).not.toContain('_ambiguousKeys');
        expect(warnedKeys(rows)).toEqual(['notifyId', 'taskType']);
        expect(rows.length).toBe(3);
        expect(mockTooltipProps.placement).toEqual('right');
    });

    it('renders every field without a warning when the slice matches the task exactly', () => {
        const { rows, text } = renderArgs({
            operation: 'host to device',
            'size(B)': 1000,
            'bandwidth(GB/s)': 0.000426,
        });

        expect(text).toContain('host to device');
        expect(warnedKeys(rows)).toEqual([]);
    });

    it('warns on the dynamic PMU metric names reported by the backend', () => {
        const { rows } = renderArgs({
            modelId: '48',
            'hhh': 111,
            '_ambiguousKeys': ['hhh'],
        });

        expect(warnedKeys(rows)).toEqual(['hhh']);
    });
});

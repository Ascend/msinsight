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
import { act, cleanup, fireEvent, render } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import type { ColumnType } from 'antd/lib/table';
import { light } from '../theme/light';
import { ResizeTable, type ResizeTableRef } from './ResizeTable';

jest.mock('react-i18next', () => ({
    ...jest.requireActual('react-i18next'),
    useTranslation: () => ({ t: (key: string): string => key }),
}));

const filterColumn = (name: string): ColumnType<{ key: string }> => ({
    title: name,
    dataIndex: name,
    key: name,
    filterIcon: () => <span>Filter {name}</span>,
    filterDropdown: ({ setSelectedKeys, confirm, selectedKeys }) => <div>
        <span>Selected {name}: {selectedKeys.join(',')}</span>
        <button onClick={() => { setSelectedKeys(['match']); confirm(); }}>Apply {name}</button>
    </div>,
});

beforeEach(() => {
    Object.defineProperty(window, 'matchMedia', {
        writable: true,
        value: jest.fn().mockImplementation((query: string) => ({
            matches: false,
            media: query,
            addListener: jest.fn(),
            removeListener: jest.fn(),
        })),
    });
});

it('keeps newly added server filters controlled and supports clearing filters', () => {
    const errors = jest.spyOn(console, 'error').mockImplementation(() => {});
    const ref = React.createRef<ResizeTableRef>();
    const onChange = jest.fn();
    const first = filterColumn('first');
    const view = render(<ThemeProvider theme={light}>
        <ResizeTable ref={ref} columns={[first]} dataSource={[]} onChange={onChange} pagination={false} />
    </ThemeProvider>);
    fireEvent.click(view.getByText('Filter first'));
    fireEvent.click(view.getByText('Apply first'));
    expect(onChange).toHaveBeenLastCalledWith(expect.anything(), { first: ['match'] }, expect.anything(), expect.anything());
    view.rerender(<ThemeProvider theme={light}>
        <ResizeTable ref={ref} columns={[first, filterColumn('second')]} dataSource={[]} onChange={onChange} pagination={false} />
    </ThemeProvider>);
    fireEvent.click(view.getByText('Filter second'));
    expect(view.getByText('Selected second:')).toBeTruthy();
    act(() => ref.current?.clearAllFilters());
    fireEvent.click(view.getByText('Filter first'));
    expect(view.getByText('Selected first:')).toBeTruthy();
    cleanup();
    const messages = errors.mock.calls;
    errors.mockRestore();
    expect(messages).toEqual([]);
});

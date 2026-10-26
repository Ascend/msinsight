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
// eslint-disable-next-line import/named -- waitFor is re-exported from @testing-library/dom.
import { act, cleanup, fireEvent, render, waitFor } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import type { ColumnType } from 'antd/lib/table';
import { light } from '../theme/light';
import { ResizeTable, type ResizeTableRef } from './ResizeTable';
import { copyTableToClipboard } from '../utils';

jest.mock('../utils', () => ({ ...jest.requireActual('../utils'), copyTableToClipboard: jest.fn() }));

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

it('uses explicit clipboard columns without exporting actions or tooltip content', async () => {
    const columns = [
        { title: <span>Name tooltip</span>, dataIndex: 'name' },
        { title: 'Actions', key: 'action', render: () => <button>Jump</button> },
    ];
    const copyColumns = [{ title: 'Name', dataIndex: 'name' }];
    const rows = [{ key: 'one', name: 'aten::add' }];
    const view = render(<ThemeProvider theme={light}>
        <ResizeTable columns={columns} copyColumns={copyColumns} dataSource={rows} pagination={false} />
    </ThemeProvider>);
    fireEvent.click(view.container.querySelector('.exportTableBtn') as Element);
    await waitFor(() => expect(copyTableToClipboard).toHaveBeenCalledWith(copyColumns, rows));
});

it('keeps the existing clipboard columns when no override is provided', async () => {
    const columns = [{ title: 'Name', dataIndex: 'name' }];
    const rows = [{ key: 'one', name: 'aten::add' }];
    const view = render(<ThemeProvider theme={light}>
        <ResizeTable columns={columns} dataSource={rows} pagination={false} />
    </ThemeProvider>);
    fireEvent.click(view.container.querySelector('.exportTableBtn') as Element);
    await waitFor(() => expect(copyTableToClipboard).toHaveBeenCalledWith(columns, rows));
});

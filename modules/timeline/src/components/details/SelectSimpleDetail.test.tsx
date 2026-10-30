/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React from 'react';
import { message, Table } from 'antd';
import * as testingLibrary from '@testing-library/react';
import { SelectSimpleTabularDetail } from './SelectSimpleDetail';
import { useDetailUpdater } from './hooks';
import { parseColDef } from './utils';
import { slicesListDetail } from '../../insight/units/details';
import type { TableState } from './types';
import { getColumnSearchProps } from '../detailViews/Common';
import { nameKeywordsWithinLimit } from './nameKeywords';

let mockTableProps: any;
const { fireEvent, render, screen, waitFor } = testingLibrary;

jest.mock('../../insight/units/AscendUnit', () => ({ getSliceTimeDisplay: (value: number): string => String(value) }));
jest.mock('../charts/ChartInteractor/draw', () => ({ checkIsValidSlice: (): boolean => false }));
jest.mock('./hooks', () => ({ useDetailUpdater: jest.fn() }));
jest.mock('react-i18next', () => ({
    ...jest.requireActual('react-i18next'),
    useTranslation: () => ({ t: (key: string): string => key }),
}));

// 所有交互都使用真实公共表格，确保缓存列配置时仍能读到最新条件并保留拖动宽度。
jest.mock('@insight/lib/resize', () => ({
    ...jest.requireActual('@insight/lib/resize'),
    ResizeTable: (props: any): JSX.Element => {
        mockTableProps = props;
        const ReactForMock = require('react');
        return ReactForMock.createElement(jest.requireActual('@insight/lib/resize').ResizeTable, {
            ...props, virtual: false, scroll: undefined, rowKey: 'title', pagination: false,
        });
    },
}));

const makeRows = (): TableState['dataSource'] => [
    { title: 'MatMul_main', wallDuration: 2000000, selfTime: 1000000, occurrences: 2, avgWallDuration: 1000000, maxWallDuration: 1500000, minWallDuration: 500000 },
    { title: 'StreamWait', wallDuration: 3000000, selfTime: 3000000, occurrences: 1, avgWallDuration: 3000000, maxWallDuration: 3000000, minWallDuration: 3000000 },
    { title: 'Add', wallDuration: 4000000, selfTime: 2000000, occurrences: 2, avgWallDuration: 2000000, maxWallDuration: 2500000, minWallDuration: 1500000 },
    { title: 'Multiply', wallDuration: 1000000, selfTime: 1000000, occurrences: 1, avgWallDuration: 1000000, maxWallDuration: 1000000, minWallDuration: 1000000 },
];

type Mode = 'include' | 'exclude';
const modeName = (mode: Mode): string => mode === 'include' ? 'Include name keyword' : 'Exclude name keywords';
const switchMode = (mode: Mode): void => { fireEvent.click(screen.getByRole('radio', { name: modeName(mode) })); };
const input = (mode: Mode): HTMLElement => screen.getByRole('combobox', { name: modeName(mode) });
const enter = (text: string, mode: Mode = 'exclude'): void => {
    switchMode(mode);
    fireEvent.change(input(mode), { target: { value: text } });
};
const open = (): void => { fireEvent.click(screen.getByRole('button', { name: 'Name filter' })); };
const search = (): void => { fireEvent.click(screen.getByRole('button', { name: /Search$/ })); };
const reset = (): void => { fireEvent.click(screen.getByRole('button', { name: 'Reset' })); };
const remove = (keyword: string): void => {
    fireEvent.click(screen.getByTitle(keyword).querySelector('.ant-select-selection-item-remove') as Element);
};
const pressEnter = (mode: Mode): void => {
    fireEvent.keyDown(input(mode), { key: 'Enter', code: 'Enter', keyCode: 13, which: 13 });
};
const titles = (): string[] => mockTableProps.dataSource.map((row: any) => row.title);
const renderList = (): ReturnType<typeof render> => render(<SelectSimpleTabularDetail session={session} height={300} detail={slicesListDetail} />);

describe('Slice List name filters', () => {
    let state: TableState;
    beforeEach(() => {
        state = { dataSource: makeRows(), columns: parseColDef(slicesListDetail, session), loading: false };
        (useDetailUpdater as jest.Mock).mockImplementation(() => state);
    });

    it('excludes normalized keywords and updates totals, copy data and selected details; reset restores rows', async () => {
        const { container } = renderList();
        fireEvent.click(screen.getByText('MatMul_main'));
        expect(session.selectedMultiSlice).toContain('MatMul_main');
        open();
        enter('wait,');
        search();
        await waitFor(() => expect(titles()).toHaveLength(3));
        expect(session.selectedMultiSlice).toContain('MatMul_main');
        open();
        expect(screen.getByText('Include name keyword', { selector: 'span[title]' })).toHaveAttribute('title', 'Include mode help');
        expect(screen.getByText('Exclude name keywords', { selector: 'span[title]' })).toHaveAttribute('title', 'Exclude mode help');
        enter(' MatMul ,WAIT，matmul,  ,\n');
        expect(titles()).toHaveLength(3); // Drafts do not filter before Search.
        search();
        await waitFor(() => expect(titles()).toEqual(['Add', 'Multiply']));
        expect(session.selectedMultiSlice).toBe('');
        expect(mockTableProps.allowCopy).toBe(true);
        const cells = Array.from(container.querySelectorAll('.ant-table-summary td')).map(cell => cell.textContent);
        expect(cells).toEqual(['Totals', '5000000', '3000000', String(5000000 / 3), '2500000', '1000000', '3']);
        expect(container.querySelector('.ant-table-filter-trigger')).toHaveClass('active');
        open();
        reset();
        await waitFor(() => expect(titles()).toHaveLength(4));
        expect(container.querySelector('.ant-table-filter-trigger')).not.toHaveClass('active');
        fireEvent.click(screen.getByText('Add'));
        expect(session.selectedMultiSlice).toContain('Add');
    });

    it('keeps sorting and active exclusions when data refreshes', async () => {
        const view = renderList();
        open();
        enter('MATMUL,');
        search();
        fireEvent.click(screen.getByText('Wall Duration'));
        fireEvent.click(screen.getByText('Wall Duration'));
        expect(titles()).toEqual(['Add', 'StreamWait', 'Multiply']);
        state = { ...state, dataSource: [...makeRows(), { title: 'Mul_new', wallDuration: 6000000 }, { title: 'Matmul_new', wallDuration: 9000000 }] };
        view.rerender(<SelectSimpleTabularDetail session={session} height={301} detail={slicesListDetail} />);
        await waitFor(() => expect(titles()).toEqual(['Mul_new', 'Add', 'StreamWait', 'Multiply']));
    });

    it('retains both drafts but filters the complete selection using only the searched mode', async () => {
        renderList();
        open();
        enter('mul,add,', 'include');
        enter('matmul');
        switchMode('include');
        expect(screen.getByTitle('mul')).toBeInTheDocument();
        search();
        await waitFor(() => expect(titles()).toEqual(['MatMul_main', 'Add', 'Multiply']));
        open();
        switchMode('exclude');
        expect(input('exclude')).toHaveValue('');
        expect(screen.getByTitle('matmul')).toBeInTheDocument();
        search();
        await waitFor(() => expect(titles()).toEqual(['StreamWait', 'Add', 'Multiply']));
    });

    it('treats special characters literally and supports an empty result and tag removal', async () => {
        state = { ...state, dataSource: [{ title: 'op[0]' }, { title: 'op.1' }, { title: 'other' }] };
        const { container } = renderList();
        open();
        enter('[0]');
        pressEnter('exclude');
        search();
        await waitFor(() => expect(titles()).toEqual(['op.1', 'other']));
        open();
        enter('.,other,');
        search();
        await waitFor(() => expect(titles()).toEqual([]));
        expect(container.querySelector('.ant-table-summary')).toBeNull();
        open();
        remove('other');
        search();
        await waitFor(() => expect(titles()).toEqual(['other']));
    });

    it.each(['include', 'exclude'] as const)('notifies duplicates only on submission and once per paste in %s', async (mode) => {
        const info = jest.spyOn(message, 'info').mockImplementation(() => (() => {}) as any);
        renderList();
        open();
        enter('matmul', mode);
        pressEnter(mode);
        expect(info).not.toHaveBeenCalled();
        for (const [index, keyword] of ['matmul', ' MATMUL '].entries()) {
            enter(keyword, mode);
            expect(info).toHaveBeenCalledTimes(index);
            pressEnter(mode);
            expect(input(mode)).toHaveValue('');
            expect(info).toHaveBeenCalledTimes(index + 1);
        }
        expect(info).toHaveBeenLastCalledWith(expect.objectContaining({ content: 'Duplicate keyword ignored', duration: 2 }));
        info.mockClear();
        enter('matmul，', mode);
        expect(info).toHaveBeenCalledTimes(1);
        info.mockClear();
        enter('matmul,', mode);
        expect(info).toHaveBeenCalledTimes(1);
        info.mockClear();
        fireEvent.paste(input(mode), { clipboardData: { getData: () => ' MATMUL,wait\nWAIT,add,ADD' } });
        expect(info).toHaveBeenCalledTimes(1);
        expect(screen.getAllByTitle('matmul')).toHaveLength(1);
        expect(screen.getByTitle('wait')).toBeInTheDocument();
        info.mockClear();
        remove('add');
        expect(info).not.toHaveBeenCalled();
        search();
        await waitFor(() => expect(titles()).toEqual(mode === 'include' ? ['MatMul_main', 'StreamWait'] : ['Add', 'Multiply']));
    });

    it.each(['include', 'exclude'] as const)('does not erase unfinished text when deleting a tag in %s', async (mode) => {
        renderList();
        open();
        enter('matmul,add,', mode);
        enter('wait', mode);
        remove('matmul');
        expect(input(mode)).toHaveValue('wait');
        expect(screen.getByTitle('add')).toBeInTheDocument();
        search();
        await waitFor(() => expect(titles()).toEqual(mode === 'include' ? ['StreamWait', 'Add'] : ['MatMul_main', 'Multiply']));
    });

    it('preserves the existing Python API name search and reset', async () => {
        const setSearchText = jest.fn();
        const view = render(<Table rowKey="name" pagination={false} dataSource={[{ name: 'MatMul' }, { name: 'Add' }]}
            columns={[{
                title: 'Name',
                dataIndex: 'name',
                ...getColumnSearchProps({
                    dataIndex: 'name', searchText: '', searchedColumn: '', setSearchText, setSearchedColumn: jest.fn(),
                }),
            }]} />);
        const trigger = view.container.querySelector('.ant-table-filter-trigger') as Element;
        fireEvent.click(trigger);
        expect(screen.getAllByRole('textbox')).toHaveLength(1);
        fireEvent.change(screen.getByRole('textbox'), { target: { value: ' mat ' } });
        search();
        await waitFor(() => expect(screen.queryByRole('cell', { name: 'Add' })).not.toBeInTheDocument());
        expect(screen.getByRole('cell', { name: 'MatMul' })).toBeInTheDocument();
        expect(setSearchText).toHaveBeenLastCalledWith('mat');
        fireEvent.click(trigger);
        reset();
        await waitFor(() => expect(screen.getByRole('cell', { name: 'Add' })).toBeInTheDocument());
        expect(setSearchText).toHaveBeenLastCalledWith('');
    });

    it.each(['include', 'exclude'] as const)('enforces 20-keyword and 200-character limits for typing and paste in %s', (mode) => {
        const warning = jest.spyOn(message, 'warning').mockImplementation(() => (() => {}) as any);
        renderList();
        open();
        switchMode(mode);
        fireEvent.paste(input(mode), { clipboardData: { getData: () => Array.from({ length: 20 }, (_, index) => `k${index}`).join('\r\n') } });
        expect(screen.getByTitle('k19')).toBeInTheDocument();
        enter('extra', mode);
        expect(input(mode)).toHaveValue('');
        expect(warning).toHaveBeenCalledWith(expect.objectContaining({ content: 'Keyword limit exceeded' }));
        remove('k19');
        enter('extra', mode);
        expect(input(mode)).toHaveValue('extra');
        reset();
        open();
        warning.mockClear();
        enter('a'.repeat(200), mode);
        expect(input(mode)).toHaveValue('a'.repeat(200));
        enter('a'.repeat(201), mode);
        expect(input(mode)).toHaveValue('a'.repeat(200));
        expect(warning).toHaveBeenCalled();
        enter('', mode);
        warning.mockClear();
        fireEvent.paste(input(mode), { clipboardData: { getData: () => `${'a'.repeat(100)},${'b'.repeat(101)}` } });
        expect(screen.queryByTitle('a'.repeat(100))).not.toBeInTheDocument();
        expect(warning).toHaveBeenCalled();
        fireEvent.paste(input(mode), { clipboardData: { getData: () => `${'a'.repeat(100)},${'b'.repeat(100)}` } });
        expect(screen.getByTitle('a'.repeat(100))).toBeInTheDocument();
        expect(screen.getByTitle('b'.repeat(100))).toBeInTheDocument();
    });

    it('counts Unicode characters after trimming and case-insensitive deduplication', () => {
        expect(nameKeywordsWithinLimit(['😀'.repeat(200)])).toBe(true);
        expect(nameKeywordsWithinLimit(['😀'.repeat(201)])).toBe(false);
        expect(nameKeywordsWithinLimit([' abc ', 'ABC', '中'.repeat(197)])).toBe(true);
        expect(nameKeywordsWithinLimit(['abc', '中'.repeat(198)])).toBe(false);
    });

    it.each(['include', 'exclude'] as const)('resets only %s and preserves the other mode', async (mode) => {
        const { container } = renderList();
        open();
        enter('mul', 'include');
        enter('matmul');
        switchMode(mode);
        expect(container.querySelector('.ant-select-clear')).toBeNull();
        reset();
        await waitFor(() => expect(titles()).toHaveLength(4));
        expect(container.querySelector('.ant-table-filter-trigger')).not.toHaveClass('active');
        open();
        expect(input(mode)).toHaveValue('');
        expect(screen.queryByTitle(mode === 'include' ? 'mul' : 'matmul')).not.toBeInTheDocument();
        switchMode(mode === 'include' ? 'exclude' : 'include');
        expect(screen.getByTitle(mode === 'include' ? 'matmul' : 'mul')).toBeInTheDocument();
        search();
        await waitFor(() => expect(titles()).toEqual(mode === 'include' ? ['StreamWait', 'Add', 'Multiply'] : ['MatMul_main', 'Multiply']));
    });

    it.each(['include', 'exclude'] as const)('submits an empty %s condition on Enter and closes the popup', async (mode) => {
        renderList();
        open();
        enter('matmul,', mode);
        search();
        await waitFor(() => expect(titles()).toHaveLength(mode === 'include' ? 1 : 3));
        open();
        remove('matmul');
        pressEnter(mode);
        await waitFor(() => expect(titles()).toHaveLength(4));
        expect(input(mode).closest('.ant-dropdown')).toHaveClass('ant-dropdown-hidden');
    });

    it('dismisses on continuous resize without changing filters or losing resized widths when reopened', async () => {
        const { container } = renderList();
        open();
        enter('mul,', 'include');
        search();
        await waitFor(() => expect(titles()).toHaveLength(2));
        open();
        enter('wait,', 'include');
        enter('other', 'include');
        const handle = container.querySelector('th .resizor') as Element;
        // Give jsdom widths so the real drag code runs, rather than skipping below minThWidth.
        Object.defineProperty(handle.parentElement, 'offsetWidth', { configurable: true, value: 240 });
        Object.defineProperty(handle.parentElement?.nextElementSibling, 'offsetWidth', { configurable: true, value: 180 });
        fireEvent.mouseDown(handle, { clientX: 100 });
        [130, 150, 160].forEach(clientX => fireEvent.mouseMove(window, { clientX }));
        fireEvent.mouseUp(window, { clientX: 160 });
        expect(container.querySelector('col')).toHaveStyle({ width: '300px' });
        await waitFor(() => expect(input('include').closest('.ant-dropdown')).toHaveClass('ant-dropdown-hidden'));
        expect(titles()).toHaveLength(2);
        open();
        expect(container.querySelector('col')).toHaveStyle({ width: '300px' });
        expect(screen.getByTitle('mul')).toBeInTheDocument();
        expect(screen.queryByTitle('wait')).not.toBeInTheDocument();
        expect(input('include')).toHaveValue('');
        search();
        expect(container.querySelector('col')).toHaveStyle({ width: '300px' });
        open();
        reset();
        await waitFor(() => expect(titles()).toHaveLength(4));
        expect(container.querySelector('col')).toHaveStyle({ width: '300px' });
        open();
        enter('wait,', 'exclude');
        search();
        await waitFor(() => expect(titles()).toEqual(['MatMul_main', 'Add', 'Multiply']));
        fireEvent.click(screen.getByText('Wall Duration'));
        await waitFor(() => expect(titles()).toEqual(['Multiply', 'MatMul_main', 'Add']));
        expect(container.querySelector('col')).toHaveStyle({ width: '300px' });
    });
});

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
import * as testingLibrary from '@testing-library/react';
import type { Session } from '../entity/session';
import { CategorySearch, searchCategory } from './CategorySearch';
import jumpToUnitOperator from '../utils/jumpToUnitOperator';
const { act, fireEvent, render, screen } = testingLibrary;

jest.mock('mobx-react', () => ({ observer: (component: unknown): unknown => component }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string): string => key }) }));
jest.mock('@emotion/react', () => ({
    ...jest.requireActual('@emotion/react'),
    useTheme: () => ({ buttonColor: { enableClickColor: '#fff' } }),
}));
jest.mock('@insight/lib/icon', () => ({ SearchIcon: (): null => null }));
jest.mock('@ant-design/icons', () => ({
    LeftOutlined: () => <span>previous</span>,
    RightOutlined: () => <span>next</span>,
}));
jest.mock('../assets/images/insights/ic_close_filled.svg', () => ({ ReactComponent: (): null => null }));
jest.mock('antd', () => ({ message: { useMessage: () => [{ warning: jest.fn() }, null] } }));
jest.mock('../insight/units/utils', () => ({ getTimeOffset: (): number => 0 }));
jest.mock('../entity/data', () => ({ getCardSourceDbPaths: (): string[] => ['source.db'] }));
jest.mock('../utils/jumpToUnitOperator', () => ({ __esModule: true, default: jest.fn() }));
jest.mock('./base/StyledButton', () => ({
    CustomButton: ({ onClick, children }: any) => <button onClick={onClick}>{children}</button>,
    StyledButton: ({ onClick, children }: any) => <button onClick={onClick}>{children}</button>,
}));
jest.mock('@insight/lib/components', () => ({
    Tooltip: ({ children, title, open }: any) => <>{children}{open === true && <div data-testid="search-popup">{title}</div>}</>,
    Button: ({ children, icon, onClick, disabled }: any) => <button disabled={disabled} onClick={onClick}>{icon}{children}</button>,
    Input: ({ value, onChange, onPressEnter, disabled, size }: any) => (
        <input aria-label={size === 'large' ? 'search' : 'page'} value={value} disabled={disabled}
            onChange={onChange} onKeyDown={event => { if (event.key === 'Enter') { onPressEnter(); } }}/>
    ),
    Select: ({ value, onChange, options, disabled }: any) => (
        <select aria-label="rank" value={value} disabled={disabled} onChange={event => onChange(event.target.value)}>
            {options.map((option: any) => <option key={option.value} value={option.value}>{option.label}</option>)}
        </select>
    ),
}));

const deferred = <T extends unknown>(): { promise: Promise<T>; resolve: (value: T) => void } => {
    let resolve!: (value: T) => void;
    const promise = new Promise<T>(_resolve => { resolve = _resolve; });
    return { promise, resolve };
};
const countResult = (rankId: string, dbPath: string): any => ({ totalCount: 2, countList: [{ rankId, dbPath, count: 2 }] });
const sliceResult = (rankId: string, dbPath: string): any => ({ rankId, dbPath, tid: 'thread', id: 'slice', startTime: 10, duration: 2 });
const criteria = { content: 'operator', isMatchCase: true, isMatchExact: true, rankId: 'ALL' };
const createSession = (): Session => ({
    units: [{ isDisplay: true, metadata: { cardId: 'rank', dataSource: 'remote', dbPath: 'source.db' } }],
    lockUnit: [],
} as unknown as Session);

const originalRequest = window.request;
afterEach(() => {
    window.request = originalRequest;
    jest.clearAllMocks();
});

describe('CategorySearch request isolation', () => {
    it.each([
        ['cancelled identical criteria', true, 'operator'],
        ['superseded identical criteria', false, 'operator'],
        ['cancelled different criteria', true, 'another operator'],
    ])('keeps counts and loading state local for %s', async (_name, cancelA, content) => {
        const countA = deferred<any>();
        const countB = deferred<any>();
        const sliceB = deferred<any>();
        const request = jest.fn()
            .mockReturnValueOnce(countA.promise)
            .mockReturnValueOnce(countB.promise)
            .mockReturnValueOnce(sliceB.promise)
            .mockResolvedValue(sliceResult('rank-B', 'B.db'));
        window.request = request;
        const session = createSession();
        render(<CategorySearch session={session}/>);
        let aIsCurrent = true;
        const optionsA = { isCurrent: () => aIsCurrent, onSuccess: jest.fn(), onFailure: jest.fn() };
        let bIsCurrent = true;
        const optionsB = { isCurrent: () => bIsCurrent, onSuccess: jest.fn(), onFailure: jest.fn() };
        let searchA!: Promise<number>;
        let searchB!: Promise<number>;
        act(() => { searchA = searchCategory(session, criteria, optionsA); });
        expect(screen.getByTestId('search-popup')).toBeTruthy();
        aIsCurrent = !cancelA;
        act(() => { searchB = searchCategory(session, { ...criteria, content: content as string }, optionsB); });

        // A returns after B started but before B's counts: it must neither append to B nor clear B's spinner.
        await act(async () => { countA.resolve(countResult('rank-A', 'A.db')); await searchA; });
        expect((screen.getByLabelText('search') as HTMLInputElement).disabled).toBe(true);
        expect(request).toHaveBeenCalledTimes(2);
        expect(jumpToUnitOperator).not.toHaveBeenCalled();
        await act(async () => { countB.resolve(countResult('rank-B', 'B.db')); });
        expect(request).toHaveBeenNthCalledWith(3, 'remote', {
            command: 'search/slice',
            params: {
                rankId: 'rank-B',
                dbPath: 'B.db',
                searchContent: content,
                index: 1,
                isMatchCase: true,
                isMatchExact: true,
                metadataList: [],
            },
        });
        await act(async () => { sliceB.resolve(sliceResult('rank-B', 'B.db')); await searchB; });
        expect(jumpToUnitOperator).toHaveBeenCalledTimes(1);
        expect(jumpToUnitOperator).toHaveBeenCalledWith(expect.objectContaining({ cardId: 'rank-B', dbPath: 'B.db' }), expect.anything());
        expect((screen.getByLabelText('search') as HTMLInputElement).disabled).toBe(false);
        expect(screen.getByText('/ 2')).toBeTruthy();
        bIsCurrent = false; // Agent 调用已结束，用户仍可浏览已发布的搜索结果。
        await act(async () => { fireEvent.click(screen.getByText('next')); });
        expect(request).toHaveBeenNthCalledWith(4, 'remote', {
            command: 'search/slice',
            params: {
                rankId: 'rank-B',
                dbPath: 'B.db',
                searchContent: content,
                index: 2,
                isMatchCase: true,
                isMatchExact: true,
                metadataList: [],
            },
        });
        expect(optionsA.onFailure).toHaveBeenCalledTimes(1);
        expect(optionsB.onFailure).not.toHaveBeenCalled();
    });
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import { ThemeProvider } from '@emotion/react';
import { fireEvent, render } from '@testing-library/react';
import { CallStackViewer, getCallStackDisplayGroups, getCallStackHighlightParts } from '../CallStackViewer';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({
        t: (key: string, options?: { current?: number; total?: number }) => {
            if (key === 'callStackMatchPosition') {
                return `${options?.current ?? 0}/${options?.total ?? 0}`;
            }
            return key;
        },
    }),
}));

const groups = [
    { key: 'host', label: 'Host', lines: ['alloc()', 'Worker::Run()', 'free()'] },
    { key: 'device', label: 'Device', lines: ['kernel_launch'] },
];
const testTheme = {
    bgColor: '#fff',
    bgColorCommon: '#fff',
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
    borderColorLight: '#ddd',
    primaryColor: '#1677ff',
    primaryColorLight4: '#d6e4ff',
    primaryColorLight5: '#e6f4ff',
    textColorPrimary: '#111',
    textColorSecondary: '#666',
} as any;
const TestThemeProvider = ThemeProvider as React.ComponentType<{ theme: any; children?: React.ReactNode }>;

describe('CallStackViewer', () => {
    const renderViewer = (
        onKeyDown?: () => void,
        viewerGroups: typeof groups = groups,
    ): ReturnType<typeof render> => render(
        React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(
                'div',
                { onKeyDown },
                React.createElement(CallStackViewer, { groups: viewerGroups }),
            ),
        ),
    );

    it('reverses each group while keeping visible line numbers ascending', () => {
        const result = getCallStackDisplayGroups(groups, 'reverse');
        expect(result[0].lines.map(line => line.originalLineNumber)).toEqual([3, 2, 1]);
        expect(result[0].lines.map(line => line.displayLineNumber)).toEqual([1, 2, 3]);
        expect(result[1].lines[0].originalLineNumber).toBe(1);
    });

    it('does not mutate or filter display groups when finding text', () => {
        const result = getCallStackDisplayGroups(groups, 'forward');
        expect(result).toHaveLength(2);
        expect(result.flatMap(group => group.lines)).toHaveLength(4);
        expect(groups[0].lines).toEqual(['alloc()', 'Worker::Run()', 'free()']);
    });

    it('returns safe highlight parts for literal special characters', () => {
        expect(getCallStackHighlightParts('Worker::Run()', '::')).toEqual([
            { text: 'Worker', matched: false },
            { text: '::', matched: true },
            { text: 'Run()', matched: false },
        ]);
    });

    it('places the title in the toolbar and uses a concise find placeholder', () => {
        const singleGroup = [{ key: 'callStack', label: 'callStack', lines: ['alloc()'] }];
        const view = renderViewer(undefined, singleGroup);
        expect(view.getByText('callStackTitle')).toBeTruthy();
        expect(view.queryByText('callStack')).toBeNull();
        expect(view.getByRole('searchbox', { name: 'findCallStack' }).getAttribute('placeholder'))
            .toBe('findCallStackPlaceholder');
    });

    it('uses one button to switch order in both directions without reversing visible sequence numbers', () => {
        const view = renderViewer();
        expect(view.getAllByTestId('callStackLine').map(line => line.getAttribute('data-line-number')))
            .toEqual(['1', '2', '3', '1']);
        expect(view.getAllByRole('button', { name: /switchTo(Forward|Reverse)Order/ })).toHaveLength(1);
        fireEvent.click(view.getByRole('button', { name: 'switchToReverseOrder' }));
        expect(view.getAllByTestId('callStackLine').map(line => line.getAttribute('data-line-number')))
            .toEqual(['1', '2', '3', '1']);
        expect(view.getAllByTestId('callStackLine').map(line => line.getAttribute('data-original-line-number')))
            .toEqual(['3', '2', '1', '1']);
        fireEvent.click(view.getByRole('button', { name: 'switchToForwardOrder' }));
        expect(view.getAllByTestId('callStackLine').map(line => line.getAttribute('data-original-line-number')))
            .toEqual(['1', '2', '3', '1']);
    });

    it('finds and navigates matches without filtering lines', () => {
        const view = renderViewer();
        const searchInput = view.getByRole('searchbox', { name: 'findCallStack' });
        fireEvent.change(searchInput, { target: { value: 'run' } });
        expect(view.getByText('1/1')).toBeTruthy();
        expect(view.getAllByTestId('callStackLine')).toHaveLength(4);
        expect(view.getByText('Run', { selector: 'mark' }).getAttribute('data-current-match')).toBe('true');

        fireEvent.change(searchInput, { target: { value: 'missing' } });
        expect(view.getByText('0/0')).toBeTruthy();
        expect(view.getAllByTestId('callStackLine')).toHaveLength(4);
        fireEvent.click(view.getByRole('button', { name: 'clearCallStackSearch' }));
        expect(view.getAllByTestId('callStackLine')).toHaveLength(4);
    });

    it('uses Enter, Shift+Enter, and buttons for cyclic match navigation', () => {
        const view = renderViewer();
        const searchInput = view.getByRole('searchbox', { name: 'findCallStack' });
        fireEvent.change(searchInput, { target: { value: '()' } });
        expect(view.getByText('1/3')).toBeTruthy();
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        expect(view.getByText('2/3')).toBeTruthy();
        fireEvent.click(view.getByRole('button', { name: 'previousCallStackMatch' }));
        expect(view.getByText('1/3')).toBeTruthy();
        fireEvent.keyDown(searchInput, { key: 'Enter', shiftKey: true });
        expect(view.getByText('3/3')).toBeTruthy();
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        expect(view.getByText('1/3')).toBeTruthy();
    });

    it('does not navigate matches when Enter confirms an IME composition', () => {
        const view = renderViewer();
        const searchInput = view.getByRole('searchbox', { name: 'findCallStack' });
        fireEvent.change(searchInput, { target: { value: '()' } });
        expect(view.getByText('1/3')).toBeTruthy();

        fireEvent.compositionStart(searchInput);
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        expect(view.getByText('1/3')).toBeTruthy();

        fireEvent.compositionEnd(searchInput);
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        expect(view.getByText('2/3')).toBeTruthy();
    });

    it('keeps the current original match when switching stack order', () => {
        const view = renderViewer();
        const searchInput = view.getByRole('searchbox', { name: 'findCallStack' });
        const getCurrentOriginalLine = (): string | null | undefined => view.container
            .querySelector('mark[data-current-match="true"]')
            ?.closest('[data-original-line-number]')
            ?.getAttribute('data-original-line-number');

        fireEvent.change(searchInput, { target: { value: '()' } });
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        fireEvent.keyDown(searchInput, { key: 'Enter' });
        expect(view.getByText('3/3')).toBeTruthy();
        expect(getCurrentOriginalLine()).toBe('3');

        fireEvent.click(view.getByRole('button', { name: 'switchToReverseOrder' }));
        expect(view.getByText('1/3')).toBeTruthy();
        expect(getCurrentOriginalLine()).toBe('3');
    });

    it('does not reapply match scrolling when a content click rerenders equivalent groups', () => {
        const originalScrollIntoView = HTMLElement.prototype.scrollIntoView;
        const scrollIntoView = jest.fn();
        Object.defineProperty(HTMLElement.prototype, 'scrollIntoView', {
            configurable: true,
            value: scrollIntoView,
        });
        const RerenderOnContentClick = (): React.ReactElement => {
            const [, setRevision] = React.useState(0);
            const equivalentGroups = groups.map(group => ({ ...group, lines: [...group.lines] }));
            return React.createElement(
                TestThemeProvider,
                { theme: testTheme },
                React.createElement(
                    'div',
                    { onClick: (): void => setRevision(revision => revision + 1) },
                    React.createElement(CallStackViewer, { groups: equivalentGroups }),
                ),
            );
        };

        try {
            const view = render(React.createElement(RerenderOnContentClick));
            fireEvent.change(view.getByRole('searchbox', { name: 'findCallStack' }), { target: { value: '()' } });
            expect(view.getByText('1/3')).toBeTruthy();
            scrollIntoView.mockClear();

            fireEvent.click(view.getAllByTestId('callStackLine')[0]);
            expect(view.getByText('1/3')).toBeTruthy();
            expect(scrollIntoView).not.toHaveBeenCalled();
        } finally {
            Object.defineProperty(HTMLElement.prototype, 'scrollIntoView', {
                configurable: true,
                value: originalScrollIntoView,
            });
        }
    });

    it('keeps search keystrokes inside the viewer and handles missing groups', () => {
        const outerKeyDown = jest.fn();
        const view = renderViewer(outerKeyDown);
        fireEvent.keyDown(view.getByRole('searchbox', { name: 'findCallStack' }), { key: 'w' });
        expect(outerKeyDown).not.toHaveBeenCalled();

        view.rerender(React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(CallStackViewer, { groups: [] }),
        ));
        expect(view.getByRole('status').textContent).toBe('noCallStackData');
    });
});

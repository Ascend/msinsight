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
import { LifecycleGraphToolbar } from '../LifecycleGraphToolbar';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string) => key }),
}));

jest.mock('@insight/lib/components', () => ({
    Tooltip: ({ children, title }: { children: React.ReactNode; title: React.ReactNode }) => {
        const ReactModule = require('react');
        return ReactModule.createElement(ReactModule.Fragment, null, title, children);
    },
}), { virtual: true });

jest.mock('../tools', () => {
    const ReactModule = require('react');
    const span = ({ children }: { children?: React.ReactNode }): JSX.Element =>
        ReactModule.createElement('span', null, children);
    const div = ({ children }: { children?: React.ReactNode }): JSX.Element =>
        ReactModule.createElement('div', null, children);
    return {
        graphToolbarTooltipClassName: 'test-tooltip',
        GraphKeycap: span,
        GraphShortcutActions: span,
        GraphShortcutRow: div,
        GraphShortcutTip: div,
        GraphShortcutTitle: div,
        GraphToolbarTooltipStyle: () => null,
        GraphWheelCombo: span,
        GraphWheelIcon: () => ReactModule.createElement('span', { 'data-testid': 'wheel-icon' }),
    };
});

const testTheme = {
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
    contentBackgroundColor: '#fff',
    iconColor: '#666',
    primaryColor: '#1677ff',
} as any;
const TestThemeProvider = ThemeProvider as React.ComponentType<{ theme: any; children?: React.ReactNode }>;

describe('LifecycleGraphToolbar', () => {
    it('combines zoom-mode switching and reset in one operation area', () => {
        const onZoomModeChange = jest.fn();
        const onReset = jest.fn();
        const onMarkerManagementOpen = jest.fn();
        const view = render(React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(LifecycleGraphToolbar, {
                zoomMode: 'proportional',
                onZoomModeChange,
                onReset,
                onMarkerManagementOpen,
            }),
        ));

        expect(view.getByTestId('lifecycleGraphToolbar')).toBeTruthy();
        expect(view.getByText('currentZoomMode')).toBeTruthy();
        expect(view.getAllByText('W')).toHaveLength(2);
        expect(view.getAllByText('S')).toHaveLength(2);
        const zoomToggle = view.getByRole('button', { name: 'switchToHorizontalZoom' });
        expect(zoomToggle.getAttribute('data-current-zoom-mode')).toBe('proportional');
        expect(view.getByTestId('proportionalZoomIcon')).toBeTruthy();
        fireEvent.click(zoomToggle);
        expect(onZoomModeChange).toHaveBeenCalledWith('horizontal');
        fireEvent.click(view.getByRole('button', { name: 'resetView' }));
        expect(onReset).toHaveBeenCalledTimes(1);
        fireEvent.click(view.getByRole('button', { name: 'manageMemoryMarkers' }));
        expect(onMarkerManagementOpen).toHaveBeenCalledTimes(1);
    });

    it('shows the horizontal icon and switches back to proportional zoom', () => {
        const onZoomModeChange = jest.fn();
        const view = render(React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(LifecycleGraphToolbar, {
                zoomMode: 'horizontal',
                onZoomModeChange,
                onReset: jest.fn(),
            }),
        ));
        const zoomToggle = view.getByRole('button', { name: 'switchToProportionalZoom' });
        expect(view.getByTestId('horizontalZoomIcon')).toBeTruthy();
        fireEvent.click(zoomToggle);
        expect(onZoomModeChange).toHaveBeenCalledWith('proportional');
    });
});

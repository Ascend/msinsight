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
    borderColorLight: '#ddd',
    borderColorLighter: '#eee',
    boxShadow: '0 2px 8px rgba(0,0,0,.12)',
    contentBackgroundColor: '#fff',
    iconColor: '#666',
    primaryColor: '#1677ff',
    textColorPrimary: '#222',
    textColorSecondary: '#777',
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
        const markerButton = view.getByRole('button', { name: 'manageMemoryMarkers' });
        expect(markerButton.querySelector('[data-icon="flag"]')).toBeTruthy();
        fireEvent.click(markerButton);
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

    it('manages independent layers from a neutral floating panel', () => {
        const onLayerVisibilityChange = jest.fn();
        const onMarkerManagementClose = jest.fn();
        const view = render(React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(LifecycleGraphToolbar, {
                zoomMode: 'proportional',
                layerVisibility: { blocks: true, overview: false, markers: true },
                onZoomModeChange: jest.fn(),
                onReset: jest.fn(),
                onLayerVisibilityChange,
                onMarkerManagementClose,
            }),
        ));

        const layerButton = view.getByRole('button', { name: 'graphLayers' });
        expect(layerButton.querySelector('[data-icon="group"]')).toBeTruthy();
        fireEvent.click(layerButton);
        expect(view.getByTestId('lifecycleGraphLayerPanel')).toBeTruthy();
        expect(onMarkerManagementClose).toHaveBeenCalledTimes(1);
        expect(view.getByText('memoryBlockLayer')).toBeTruthy();
        expect(view.getByText('memoryOverviewLayer')).toBeTruthy();
        expect(view.getByText('memoryMarkerLayer')).toBeTruthy();
        expect(view.getAllByText('layerVisible')).toHaveLength(2);
        expect(view.getAllByText('layerHidden')).toHaveLength(1);

        fireEvent.click(view.getByText('memoryOverviewLayer').closest('button') as HTMLButtonElement);
        expect(onLayerVisibilityChange).toHaveBeenCalledWith('overview');
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(view.queryByTestId('lifecycleGraphLayerPanel')).toBeNull();
    });

    it('keeps marker management available with its layer hidden and opens the toolbar guide exclusively', () => {
        const onMarkerManagementOpen = jest.fn();
        const onMarkerManagementClose = jest.fn();
        const view = render(React.createElement(
            TestThemeProvider,
            { theme: testTheme },
            React.createElement(LifecycleGraphToolbar, {
                zoomMode: 'proportional',
                layerVisibility: { blocks: true, overview: true, markers: false },
                onZoomModeChange: jest.fn(),
                onReset: jest.fn(),
                onMarkerManagementOpen,
                onMarkerManagementClose,
            }),
        ));

        const markerButton = view.getByRole('button', { name: 'manageMemoryMarkers' }) as HTMLButtonElement;
        expect(markerButton.disabled).toBe(false);
        fireEvent.click(markerButton);
        expect(onMarkerManagementOpen).toHaveBeenCalledTimes(1);
        fireEvent.click(view.getByRole('button', { name: 'lifecycleGraphGuideTitle' }));
        expect(onMarkerManagementClose).toHaveBeenCalledTimes(1);
        const guide = view.getByTestId('lifecycleGraphInteractionGuide');
        expect(guide.parentElement).toBe(document.body);
        expect(view.getAllByTestId('lifecycleGuideSection')).toHaveLength(6);
        expect(view.getByTestId('lifecycleGuideZoomVisual')).toBeTruthy();
        expect(view.getByTestId('lifecycleGuidePanVisual')).toBeTruthy();
        expect(view.getByTestId('lifecycleGuideResetVisual')).toBeTruthy();
        expect(view.getByTestId('lifecycleGuideLayerVisual')).toBeTruthy();
        expect(view.getByTestId('lifecycleGuideDifferenceMarkerVisual')).toBeTruthy();
        expect(view.getByTestId('lifecycleGuideMarkerManagementVisual')).toBeTruthy();

        fireEvent.mouseDown(guide);
        expect(view.getByTestId('lifecycleGraphInteractionGuide')).toBeTruthy();
        fireEvent.mouseDown(document.body);
        expect(view.queryByTestId('lifecycleGraphInteractionGuide')).toBeNull();
    });
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import React from 'react';
import { ThemeProvider } from '@emotion/react';
import { act, fireEvent, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import { Session } from '../../../entity/session';
import { MemoryBlockDiagram } from '../MemoryBlockDiagram';

jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@insight/lib/utils', () => ({ safeJSONParse: (value: string) => JSON.parse(value) }), { virtual: true });
jest.mock('@insight/lib', () => ({ Spin: () => null }), { virtual: true });
jest.mock('@insight/lib/components', () => ({ Tooltip: ({ children }: { children: React.ReactNode }) => children }), { virtual: true });
jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerInitCanvas: jest.fn(),
    workerResizeCanvas: jest.fn(),
    workerTransform: jest.fn(),
    workerHoverItem: jest.fn(),
    workerClickItem: jest.fn(),
    workerSelectBlockById: jest.fn(),
    workerSetBlockGraphLayerVisibility: jest.fn(),
    workerSetMarkerHoverHighlight: jest.fn(),
}), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({ workerSelectItem: jest.fn() }), { virtual: true });
jest.mock('../../../leaksWorker/tools/color', () => ({ getColorStringByAddr: () => '#59A14F' }));
jest.mock('../LifecycleGraphToolbar', () => ({
    LifecycleGraphToolbar: ({ zoomMode, onZoomModeChange, onLayerVisibilityChange }: {
        zoomMode: 'proportional' | 'horizontal';
        onZoomModeChange: (mode: 'proportional' | 'horizontal') => void;
        onLayerVisibilityChange: (layer: 'blocks' | 'overview' | 'markers') => void;
    }) => {
        const ReactModule = require('react');
        return ReactModule.createElement('div', { 'data-testid': 'mock-lifecycle-toolbar', 'data-zoom-mode': zoomMode },
            ReactModule.createElement('button', {
                'data-testid': 'toggle-block-layer',
                onClick: () => onLayerVisibilityChange('blocks'),
            }),
            ReactModule.createElement('button', {
                'data-testid': 'toggle-overview-layer',
                onClick: () => onLayerVisibilityChange('overview'),
            }),
            ReactModule.createElement('button', {
                'data-testid': 'toggle-marker-layer',
                onClick: () => onLayerVisibilityChange('markers'),
            }),
            ReactModule.createElement('button', {
                'data-testid': 'toggle-zoom-mode',
                onClick: () => onZoomModeChange(zoomMode === 'proportional' ? 'horizontal' : 'proportional'),
            }),
        );
    },
    LifecycleZoomModeIcon: () => null,
    LifecycleZoomModeTooltip: () => null,
}));
jest.mock('../LifecycleMemoryMarkerManager', () => ({ LifecycleMemoryMarkerManager: () => null }));
jest.mock('../TimelineFlagIcon', () => ({ TimelineFlagIcon: () => null }));
jest.mock('../LifecycleMemoryMarkerOverlay', () => ({
    LifecycleMemoryMarkerOverlay: ({ blockPreview }: { blockPreview?: { memoryBytes: number } | null }) => {
        const ReactModule = require('react');
        return ReactModule.createElement('div', {
            'data-testid': 'marker-overlay',
            'data-block-preview': blockPreview?.memoryBytes ?? '',
        });
    },
}));
jest.mock('../tools', () => {
    const ReactModule = require('react');
    const Wrap = ({ children }: { children?: React.ReactNode }): JSX.Element => ReactModule.createElement('div', null, children);
    return {
        Axis: () => null,
        HoverItem: () => ReactModule.createElement('div', { 'data-testid': 'block-hover-item' }),
        Loading: () => null,
        MarkLineBlock: () => null,
        graphToolbarTooltipClassName: 'tooltip',
        GraphKeycap: Wrap,
        GraphMouseIcon: () => null,
        GraphShortcutActions: Wrap,
        GraphShortcutTip: Wrap,
        GraphShortcutTitle: Wrap,
        GraphToolbar: Wrap,
        GraphToolbarTooltipStyle: () => null,
    };
});

const theme = { bgColorCommon: '#fff', bgColorLight: '#f5f5f5', borderColor: '#ccc', textColorPrimary: '#111' } as any;

describe('MemoryBlockDiagram Block Flag shortcut', () => {
    beforeEach(() => {
        jest.clearAllMocks();
    });

    it('restores a hidden Block Flag without losing its presentation', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot'; session.deviceId = '0'; session.eventType = 'malloc';
        session.addLifecycleMemoryMarker(500, 'block-flag', 'block', '#59A14F', 42);
        session.updateLifecycleMemoryMarkerPresentation('block-flag', { name: 'Peak', hidden: true });
        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);
        const canvas = view.container.querySelector('canvas') as HTMLCanvasElement;
        runInAction(() => {
            session.leaksWorkerInfo.hoverItem = {
                id: 42,
                addr: '0x2a',
                _startTimestamp: 0,
                _endTimestamp: 1,
                size: 1,
                path: [[0, 500]],
            };
        });

        fireEvent.mouseEnter(canvas);
        expect(view.getByTestId('marker-overlay').getAttribute('data-block-preview')).toBe('500');
        expect(view.getByTestId('blockMarkerShortcutHint').getAttribute('aria-label'))
            .toContain('restoreHoveredBlockMarkerHint');
        fireEvent.keyDown(canvas, { key: 'k' });

        expect(session.getLifecycleMemoryMarkers()).toEqual([expect.objectContaining({
            id: 'block-flag', name: 'Peak', hidden: false, color: '#59A14F', blockId: 42,
        })]);
        expect(view.getByTestId('marker-overlay').getAttribute('data-block-preview')).toBe('');
        expect(view.getByTestId('blockMarkerShortcutHint').getAttribute('aria-label'))
            .toContain('removeHoveredBlockMarkerHint');
    });

    it('disables block hit interactions while the block layer is hidden and restores them when shown', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot'; session.deviceId = '0'; session.eventType = 'malloc';
        const hoveredBlock = {
            id: 42,
            addr: '0x2a',
            _startTimestamp: 0,
            _endTimestamp: 1,
            size: 1,
            path: [[0, 500]],
        };
        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);
        const canvas = view.container.querySelector('canvas') as HTMLCanvasElement;
        const blockWorker = jest.requireMock('@/leaksWorker/blockWorker/worker') as {
            workerHoverItem: jest.Mock;
            workerClickItem: jest.Mock;
            workerSetBlockGraphLayerVisibility: jest.Mock;
        };
        runInAction(() => {
            session.leaksWorkerInfo.hoverItem = hoveredBlock;
        });

        fireEvent.mouseEnter(canvas);
        expect(view.getByTestId('block-hover-item')).toBeTruthy();
        expect(view.getByTestId('blockMarkerShortcutHint')).toBeTruthy();

        fireEvent.click(view.getByTestId('toggle-block-layer'));

        expect(view.getByTestId('blockDiagramSection').getAttribute('data-block-layer-visible')).toBe('false');
        expect(session.leaksWorkerInfo.hoverItem).toBeNull();
        expect(view.queryByTestId('block-hover-item')).toBeNull();
        expect(view.queryByTestId('blockMarkerShortcutHint')).toBeNull();
        expect(blockWorker.workerSetBlockGraphLayerVisibility).toHaveBeenLastCalledWith({
            visibility: { blocks: false, overview: true },
        });

        jest.clearAllMocks();
        fireEvent.mouseMove(canvas, { clientX: 12, clientY: 18 });
        fireEvent.mouseDown(canvas, { button: 0, clientX: 12, clientY: 18 });
        fireEvent.click(canvas, { clientX: 12, clientY: 18 });
        expect(blockWorker.workerHoverItem).not.toHaveBeenCalled();
        expect(blockWorker.workerClickItem).not.toHaveBeenCalled();

        runInAction(() => {
            session.leaksWorkerInfo.hoverItem = hoveredBlock;
        });
        fireEvent.keyDown(canvas, { key: 'k' });
        expect(session.getLifecycleMemoryMarkers()).toEqual([]);
        expect(view.queryByTestId('blockMarkerShortcutHint')).toBeNull();

        runInAction(() => {
            session.leaksWorkerInfo.hoverItem = null;
        });
        fireEvent.click(view.getByTestId('toggle-block-layer'));
        expect(view.getByTestId('blockDiagramSection').getAttribute('data-block-layer-visible')).toBe('true');

        jest.clearAllMocks();
        fireEvent.mouseMove(canvas, { clientX: 16, clientY: 20 });
        fireEvent.mouseDown(canvas, { button: 0, clientX: 16, clientY: 20 });
        fireEvent.click(canvas, { clientX: 16, clientY: 20 });
        expect(blockWorker.workerHoverItem).toHaveBeenCalled();
        expect(blockWorker.workerClickItem).toHaveBeenCalled();
    });

    it('shows the allocation line legend only while the overview layer is visible', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot'; session.deviceId = '0'; session.eventType = 'malloc';
        session.allocationData.allocationLineAvailability = {
            reservedLine: true,
            processUsedLine: false,
            deviceUsedLine: true,
        };
        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);

        expect(view.getByTestId('allocationLineLegend')).toBeTruthy();
        expect(view.getByText('reservedLineLegend')).toBeTruthy();
        expect(view.queryByText('processUsedLineLegend')).toBeNull();
        expect(view.getByText('deviceUsedLineLegend')).toBeTruthy();

        fireEvent.click(view.getByTestId('toggle-overview-layer'));
        expect(view.queryByTestId('allocationLineLegend')).toBeNull();
    });

    it('labels the reserved allocation line as host used for HOST event types', () => {
        const session = new Session();
        session.module = 'leaks'; session.fileHash = 'host'; session.deviceId = 'cpu'; session.eventType = 'HOST';
        session.allocationData.allocationLineAvailability = {
            reservedLine: true,
            processUsedLine: true,
            deviceUsedLine: false,
        };
        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);

        expect(view.getByText('hostUsedLineLegend')).toBeTruthy();
        expect(view.queryByText('reservedLineLegend')).toBeNull();
        expect(view.getByText('processUsedLineLegend')).toBeTruthy();
        expect(view.queryByText('deviceUsedLineLegend')).toBeNull();
    });

    it('restores MemScope lifecycle view state when the imported data context changes', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot-a'; session.deviceId = '0'; session.eventType = 'malloc';
        runInAction(() => {
            session.leaksWorkerInfo.hoverItem = {
                id: 42,
                addr: '0x2a',
                _startTimestamp: 0,
                _endTimestamp: 1,
                size: 1,
                path: [[0, 500]],
            };
        });
        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);
        const section = view.getByTestId('blockDiagramSection');
        const blockWorker = jest.requireMock('@/leaksWorker/blockWorker/worker') as {
            workerSetBlockGraphLayerVisibility: jest.Mock;
            workerSetMarkerHoverHighlight: jest.Mock;
            workerTransform: jest.Mock;
        };

        fireEvent.click(view.getByTestId('toggle-block-layer'));
        fireEvent.click(view.getByTestId('toggle-overview-layer'));
        fireEvent.click(view.getByTestId('toggle-marker-layer'));
        fireEvent.click(view.getByTestId('toggle-zoom-mode'));

        expect(section.getAttribute('data-block-layer-visible')).toBe('false');
        expect(section.getAttribute('data-overview-layer-visible')).toBe('false');
        expect(section.getAttribute('data-marker-layer-visible')).toBe('false');
        expect(view.getByTestId('mock-lifecycle-toolbar').getAttribute('data-zoom-mode')).toBe('horizontal');

        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = { x: 12, y: 34, scaleX: 2, scaleY: 3 };
        });

        jest.clearAllMocks();
        act(() => {
            runInAction(() => {
                session.fileHash = 'snapshot-b';
            });
        });

        expect(section.getAttribute('data-block-layer-visible')).toBe('true');
        expect(section.getAttribute('data-overview-layer-visible')).toBe('true');
        expect(section.getAttribute('data-marker-layer-visible')).toBe('true');
        expect(view.getByTestId('mock-lifecycle-toolbar').getAttribute('data-zoom-mode')).toBe('proportional');
        expect(blockWorker.workerSetBlockGraphLayerVisibility).toHaveBeenLastCalledWith({
            visibility: { blocks: true, overview: true },
        });
        expect(blockWorker.workerTransform).toHaveBeenLastCalledWith({
            transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
        });
        expect(blockWorker.workerSetMarkerHoverHighlight).toHaveBeenLastCalledWith({ active: false });
        expect(session.leaksWorkerInfo.renderOptions.transform).toEqual({ x: 0, y: 0, scaleX: 1, scaleY: 1 });
        expect(session.leaksWorkerInfo.hoverItem).toBeNull();
    });

    it('synchronizes default lifecycle view state when MemScope remounts with a reused Worker', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot-b'; session.deviceId = '0'; session.eventType = 'malloc';
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = { x: -40, y: 25, scaleX: 4, scaleY: 5 };
        });
        const blockWorker = jest.requireMock('@/leaksWorker/blockWorker/worker') as {
            workerSetBlockGraphLayerVisibility: jest.Mock;
            workerTransform: jest.Mock;
        };

        const view = render(<ThemeProvider theme={theme}><MemoryBlockDiagram session={session} /></ThemeProvider>);

        expect(view.getByTestId('blockDiagramSection').getAttribute('data-overview-layer-visible')).toBe('true');
        expect(blockWorker.workerSetBlockGraphLayerVisibility).toHaveBeenLastCalledWith({
            visibility: { blocks: true, overview: true },
        });
        expect(blockWorker.workerTransform).toHaveBeenLastCalledWith({
            transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
        });
        expect(session.leaksWorkerInfo.renderOptions.transform).toEqual({ x: 0, y: 0, scaleX: 1, scaleY: 1 });
    });
});

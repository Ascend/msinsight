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
import { fireEvent, render } from '@testing-library/react';
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
    workerSetMarkerHoverHighlight: jest.fn(),
}), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({ workerSelectItem: jest.fn() }), { virtual: true });
jest.mock('../../../leaksWorker/tools/color', () => ({ getColorStringByAddr: () => '#59A14F' }));
jest.mock('../LifecycleGraphToolbar', () => ({
    LifecycleGraphToolbar: () => null,
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
        HoverItem: () => null,
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
    it('restores a hidden Block Flag without losing its presentation', () => {
        const session = new Session();
        session.module = 'memsnapshot'; session.fileHash = 'snapshot'; session.deviceId = '0'; session.eventType = 'malloc';
        session.addLifecycleMemoryMarker(500, 'block-flag', 'block', '#59A14F', 42);
        session.updateLifecycleMemoryMarkerPresentation('block-flag', { name: 'Peak', hidden: true });
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
        const canvas = view.container.querySelector('canvas') as HTMLCanvasElement;

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
});

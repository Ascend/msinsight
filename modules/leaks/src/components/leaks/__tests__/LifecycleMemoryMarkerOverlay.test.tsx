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
import { act, fireEvent, render, within } from '@testing-library/react';
import { runInAction } from 'mobx';
import { Session } from '../../../entity/session';
import { LifecycleMemoryMarkerOverlay } from '../LifecycleMemoryMarkerOverlay';

jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string) => key }),
}));

jest.mock('@insight/lib/utils', () => ({
    safeJSONParse: (value: string) => JSON.parse(value),
}), { virtual: true });

const theme = {
    bgColorCommon: '#fff',
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
    borderColorLight: '#ddd',
    dangerColor: '#f00',
    primaryColor: '#1677ff',
    textColorPrimary: '#111',
    textColorSecondary: '#666',
} as any;

const createSession = (): Session => {
    const session = new Session();
    session.module = 'memsnapshot';
    session.fileHash = 'snapshot';
    session.deviceId = '0';
    session.eventType = 'malloc';
    runInAction(() => {
        session.leaksWorkerInfo.sizeInfo = {
            minSize: 0,
            maxSize: 1_000,
            minTimestamp: 0,
            maxTimestamp: 1,
        };
        session.leaksWorkerInfo.renderOptions.viewport = { width: 400, height: 200 };
        session.leaksWorkerInfo.renderOptions.zoom.y = 0.2;
    });
    return session;
};

describe('LifecycleMemoryMarkerOverlay', () => {
    it('previews, creates, and deletes axis flags', () => {
        const session = createSession();
        let sequence = 0;
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerOverlay
                session={session}
                onCreateMarker={(memoryBytes): void => {
                    sequence += 1;
                    session.addLifecycleMemoryMarker(memoryBytes, `flag-${sequence}`);
                }}
            />
        </ThemeProvider>);
        const axis = view.getByRole('button', { name: 'addMemoryMarkerFromAxis' });
        axis.getBoundingClientRect = () => ({
            top: 0,
            bottom: 200,
            left: 0,
            right: 30,
            width: 30,
            height: 200,
            x: 0,
            y: 0,
            toJSON: () => ({}),
        });

        fireEvent.mouseMove(axis, { clientY: 100 });
        expect(view.getByTestId('memoryMarkerAxisPreviewLine').style.top).toBe('100px');
        fireEvent.click(axis, { clientY: 100, detail: 1 });
        fireEvent.click(axis, { clientY: 50, detail: 1 });
        expect(session.getLifecycleMemoryMarkers().map(marker => marker.memoryBytes)).toEqual([500, 750]);
        expect(view.getAllByRole('button', { name: /Flag \d/ })).toHaveLength(2);

        fireEvent.click(view.container.querySelector('[data-marker-delete]') as HTMLButtonElement);
        expect(session.getLifecycleMemoryMarkers()).toHaveLength(1);
    });

    it('keeps a flag attached to its memory value after viewport transforms', () => {
        const session = createSession();
        session.addLifecycleMemoryMarker(500, 'flag-1');
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerOverlay session={session} onCreateMarker={(): void => undefined} />
        </ThemeProvider>);
        const flag = view.getByRole('button', { name: /Flag 1/ });
        expect(flag.parentElement?.style.top).toBe('100px');

        act(() => {
            runInAction(() => {
                session.leaksWorkerInfo.renderOptions.transform.y = -20;
                session.leaksWorkerInfo.renderOptions.transform.scaleY = 2;
            });
        });
        expect(view.getByRole('button', { name: /Flag 1/ }).parentElement?.style.top).toBe('20px');
    });

    it('projects a block hover as a colored flag and dashed-line preview', () => {
        const session = createSession();
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerOverlay
                session={session}
                onCreateMarker={(): void => undefined}
                blockPreview={{ memoryBytes: 500, color: '#59A14F' }}
            />
        </ThemeProvider>);
        expect(view.getByTestId('memoryMarkerAxisPreviewLine').style.top).toBe('100px');
        expect(view.getByTestId('memoryMarkerAxisPreviewFlag').getAttribute('data-preview-source')).toBe('block');
        expect(view.getByTestId('memoryMarkerAxisPreviewFlag').style.color).toBe('rgb(89, 161, 79)');
    });

    it('keeps adjacent differences visible and highlights both linked endpoints', () => {
        const session = createSession();
        session.addLifecycleMemoryMarker(250, 'flag-1', 'block', '#4C7DFF', 101);
        session.addLifecycleMemoryMarker(750, 'flag-2', 'block', '#FF7A45', 202);
        const onMarkerHoverChange = jest.fn();
        const onGapHoverChange = jest.fn();
        const view = render(<ThemeProvider theme={theme}>
            <LifecycleMemoryMarkerOverlay
                session={session}
                onCreateMarker={(): void => undefined}
                onMarkerHoverChange={onMarkerHoverChange}
                onGapHoverChange={onGapHoverChange}
            />
        </ThemeProvider>);

        const gap = view.getByTestId('memoryMarkerGapSegment');
        expect(view.getByTestId('memoryMarkerInlineGapValue').textContent).toMatch(/^Δ /);

        fireEvent.mouseEnter(gap);
        expect(onGapHoverChange).toHaveBeenLastCalledWith(true, expect.arrayContaining([101, 202]));
        expect(view.container.querySelectorAll('[data-emphasized="true"]')).toHaveLength(4);

        fireEvent.mouseLeave(gap);
        expect(onGapHoverChange).toHaveBeenLastCalledWith(false, []);
        expect(view.container.querySelectorAll('[data-emphasized="true"]')).toHaveLength(0);

        const flag = view.getByRole('button', { name: /Flag 1/ });
        fireEvent.mouseEnter(flag);
        expect(onMarkerHoverChange).toHaveBeenLastCalledWith(expect.objectContaining({ blockId: 101 }));
        expect(view.container.querySelectorAll('[data-emphasized="true"]')).toHaveLength(2);
        const details = flag.parentElement?.querySelector('[data-marker-details]') as HTMLElement;
        expect(within(details).getAllByTestId('memoryMarkerRelationFlag')).toHaveLength(2);
        expect(within(details).getAllByTestId('memoryMarkerRelationBaseline')).toHaveLength(2);
        expect(within(details).getByTestId('memoryMarkerRelationGap').textContent).toMatch(/^[↑↓] /);
    });
});

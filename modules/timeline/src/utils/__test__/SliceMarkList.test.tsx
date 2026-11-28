/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * -------------------------------------------------------------------------
 */

import { ThemeProvider } from '@emotion/react';
import { act, fireEvent, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import React from 'react';
import { Session } from '../../entity/session';
import { SliceMarkList } from '../../components/SliceMarkList';
import type { SliceMark } from '../sliceMark';
import jumpToUnitOperator from '../jumpToUnitOperator';

jest.mock('../jumpToUnitOperator', () => ({
    __esModule: true,
    default: jest.fn(),
}));

const theme = {
    borderColor: '#d9d9d9',
    contentBackgroundColor: '#ffffff',
    boxShadow: 'none',
    iconColor: '#222222',
    bgColorLight: '#f4f6fa',
    textColorPrimary: '#111111',
    primaryColor: '#3478f6',
};

class TestPointerEvent extends MouseEvent {
    readonly pointerId: number;

    constructor(type: string, init: PointerEventInit) {
        super(type, { bubbles: true, cancelable: true, ...init });
        this.pointerId = init.pointerId ?? 0;
    }
}

Object.defineProperty(window, 'PointerEvent', { configurable: true, value: TestPointerEvent });

const activeObservers: Array<{ callback: () => void }> = [];
class MockResizeObserver {
    private readonly entry: { callback: () => void };

    constructor(callback: () => void) {
        this.entry = { callback };
        activeObservers.push(this.entry);
    }

    observe(): void {}

    unobserve(): void {}

    disconnect(): void {
        const index = activeObservers.indexOf(this.entry);
        if (index >= 0) {
            activeObservers.splice(index, 1);
        }
    }
}
(globalThis as Record<string, unknown>).ResizeObserver = MockResizeObserver;
const triggerResize = (): void => activeObservers.forEach((observer): void => observer.callback());

const createMark = (key: '1' | '2', name: string): SliceMark => ({
    key,
    name,
    operator: {
        cardId: 'card-0',
        tid: '7',
        pid: '11',
        depth: 0,
        duration: 1,
        name,
        timestamp: 10,
    },
});

describe('SliceMarkList', () => {
    it('hides an empty list and supports jump and delete actions', () => {
        const session = new Session();
        const { rerender, queryByTestId, getByTestId } = render(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        expect(queryByTestId('timeline-slice-mark-list')).toBeNull();

        act(() => {
            runInAction(() => {
                session.sliceMarks.set('1', createMark('1', 'matmul'));
                session.sliceMarks.set('2', createMark('2', 'relu'));
            });
        });
        rerender(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        expect(getByTestId('timeline-slice-mark-1')).toHaveTextContent('1matmul');
        fireEvent.click(getByTestId('timeline-slice-mark-1'));
        expect(jumpToUnitOperator).toHaveBeenCalledWith(session.sliceMarks.get('1')?.operator);

        fireEvent.click(getByTestId('timeline-slice-mark-delete-1'));
        expect(session.sliceMarks.has('1')).toBe(false);
    });
    it('moves the panel by its handle without leaving the timeline container', () => {
        const session = new Session();
        runInAction(() => {
            session.sliceMarks.set('1', createMark('1', 'matmul'));
        });
        const { getByTestId } = render(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        const panel = getByTestId('timeline-slice-mark-list');
        const parent = panel.parentElement as HTMLElement;
        jest.spyOn(parent, 'getBoundingClientRect').mockReturnValue({
            x: 0, y: 0, left: 0, top: 0, right: 300, bottom: 200, width: 300, height: 200, toJSON: () => ({}),
        });
        jest.spyOn(panel, 'getBoundingClientRect').mockReturnValue({
            x: 100, y: 80, left: 100, top: 80, right: 220, bottom: 140, width: 120, height: 60, toJSON: () => ({}),
        });
        const handle = getByTestId('timeline-slice-mark-drag-handle');

        fireEvent(handle, new TestPointerEvent('pointerdown', { button: 0, pointerId: 1, clientX: 110, clientY: 90 }));
        fireEvent(handle, new TestPointerEvent('pointermove', { pointerId: 1, clientX: 400, clientY: 300 }));
        fireEvent(handle, new TestPointerEvent('pointerup', { pointerId: 1 }));

        expect(panel).toHaveStyle({ left: '180px', top: '140px', right: 'auto', bottom: 'auto' });
    });
    it('keeps the drag handle visible when marks are added after moving the panel to the top', () => {
        const session = new Session();
        runInAction(() => {
            session.sliceMarks.set('1', createMark('1', 'matmul'));
        });
        const { getByTestId, rerender } = render(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        const panel = getByTestId('timeline-slice-mark-list');
        const parent = panel.parentElement as HTMLElement;
        jest.spyOn(parent, 'getBoundingClientRect').mockReturnValue({
            x: 0, y: 0, left: 0, top: 0, right: 300, bottom: 200, width: 300, height: 200, toJSON: () => ({}),
        });
        jest.spyOn(panel, 'getBoundingClientRect').mockReturnValue({
            x: 100, y: 80, left: 100, top: 80, right: 220, bottom: 140, width: 120, height: 60, toJSON: () => ({}),
        });
        const handle = getByTestId('timeline-slice-mark-drag-handle');
        fireEvent(handle, new TestPointerEvent('pointerdown', { button: 0, pointerId: 1, clientX: 110, clientY: 90 }));
        fireEvent(handle, new TestPointerEvent('pointermove', { pointerId: 1, clientX: 110, clientY: 0 }));
        fireEvent(handle, new TestPointerEvent('pointerup', { pointerId: 1 }));

        act(() => {
            runInAction(() => {
                session.sliceMarks.set('2', createMark('2', 'relu'));
            });
        });
        rerender(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);

        expect(panel).toHaveStyle({ left: '100px', top: '0', right: 'auto', bottom: 'auto' });
        expect(getByTestId('timeline-slice-mark-drag-handle')).toBeVisible();
    });
    it('moves the panel upward when new marks would overflow the bottom edge', () => {
        const session = new Session();
        runInAction(() => {
            session.sliceMarks.set('1', createMark('1', 'matmul'));
        });
        const { getByTestId, rerender } = render(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        const panel = getByTestId('timeline-slice-mark-list');
        const parent = panel.parentElement as HTMLElement;
        let panelHeight = 60;
        jest.spyOn(parent, 'getBoundingClientRect').mockReturnValue({
            x: 0, y: 0, left: 0, top: 0, right: 300, bottom: 200, width: 300, height: 200, toJSON: () => ({}),
        });
        jest.spyOn(panel, 'getBoundingClientRect').mockImplementation(() => ({
            x: 100,
            y: 140,
            left: 100,
            top: 140,
            right: 220,
            bottom: 140 + panelHeight,
            width: 120,
            height: panelHeight,
            toJSON: () => ({}),
        }));
        const handle = getByTestId('timeline-slice-mark-drag-handle');
        fireEvent(handle, new TestPointerEvent('pointerdown', { button: 0, pointerId: 1, clientX: 110, clientY: 150 }));
        fireEvent(handle, new TestPointerEvent('pointermove', { pointerId: 1, clientX: 110, clientY: 150 }));
        fireEvent(handle, new TestPointerEvent('pointerup', { pointerId: 1 }));
        panelHeight = 90;

        act(() => {
            runInAction(() => {
                session.sliceMarks.set('2', createMark('2', 'relu'));
            });
        });
        rerender(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);

        expect(panel).toHaveStyle({ left: '100px', top: '110px', right: 'auto', bottom: 'auto' });
        expect(getByTestId('timeline-slice-mark-2')).toBeVisible();
    });
    it('re-clamps the panel back into view when the timeline container shrinks', () => {
        const session = new Session();
        runInAction(() => {
            session.sliceMarks.set('1', createMark('1', 'matmul'));
        });
        const { getByTestId } = render(<ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>);
        const panel = getByTestId('timeline-slice-mark-list');
        const parent = panel.parentElement as HTMLElement;
        let parentHeight = 200;
        jest.spyOn(parent, 'getBoundingClientRect').mockImplementation(() => ({
            x: 0, y: 0, left: 0, top: 0, right: 300, bottom: parentHeight, width: 300, height: parentHeight, toJSON: () => ({}),
        }));
        jest.spyOn(panel, 'getBoundingClientRect').mockReturnValue({
            x: 100, y: 140, left: 100, top: 140, right: 220, bottom: 200, width: 120, height: 60, toJSON: () => ({}),
        });
        const handle = getByTestId('timeline-slice-mark-drag-handle');
        fireEvent(handle, new TestPointerEvent('pointerdown', { button: 0, pointerId: 1, clientX: 110, clientY: 150 }));
        fireEvent(handle, new TestPointerEvent('pointermove', { pointerId: 1, clientX: 110, clientY: 260 }));
        fireEvent(handle, new TestPointerEvent('pointerup', { pointerId: 1 }));
        expect(panel).toHaveStyle({ top: '140px' });

        parentHeight = 100;
        act(() => {
            triggerResize();
        });

        expect(panel).toHaveStyle({ top: '40px' });
    });
    it('does not let a panel mousedown reach the timeline selection handler', () => {
        const session = new Session();
        runInAction(() => {
            session.sliceMarks.set('1', createMark('1', 'matmul'));
        });
        const onMouseDown = jest.fn();
        const { getByTestId } = render(
            <div onMouseDown={onMouseDown}>
                <ThemeProvider theme={theme}><SliceMarkList session={session} /></ThemeProvider>
            </div>);
        fireEvent.mouseDown(getByTestId('timeline-slice-mark-delete-1'));
        expect(onMouseDown).not.toHaveBeenCalled();
    });
});

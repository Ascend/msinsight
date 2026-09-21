/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React, { useState, useMemo as mockUseMemo } from 'react';
import { ThemeProvider } from '@emotion/react';
import { act } from 'react-dom/test-utils';
import { fireEvent, render } from '@testing-library/react';
import MemSnapshotSliceOverview from '../MemSnapshotSliceOverview';
import { Painter } from '../../leaksWorker/blockWorker/nativeCanvas/Painter';

jest.mock('antd', () => ({ Button: ({ children, onClick, 'aria-label': label, title }: any) => <button aria-label={label} title={title} onClick={onClick}>{children}</button> }));
jest.mock('@insight/lib/resize', () => ({
    ResizeTable: ({ columns, dataSource, onChange, onRow, rowClassName }: any) => {
        // Match the project's serialized-column cache, including retained render closures.
        const cachedColumns = mockUseMemo(() => columns, [JSON.stringify(columns)]);
        return <table>
            <thead><tr>{cachedColumns.map((column: any) => <th key={column.key}
                aria-sort={column.sortOrder === 'ascend' ? 'ascending' : column.sortOrder === 'descend' ? 'descending' : 'none'}>
                <button onClick={() => onChange({}, {}, { field: column.dataIndex, order: column.sortOrder === 'ascend' ? 'descend' : 'ascend' })}>{column.title}</button>
            </th>)}</tr></thead>
            <tbody>{dataSource.map((row: any) => <tr key={row.index} className={rowClassName?.(row)} {...onRow?.(row)}>{cachedColumns.map((column: any) =>
                <td key={column.key}>{column.render ? column.render(row[column.dataIndex], row) : row[column.dataIndex]}</td>)}</tr>)}</tbody>
        </table>;
    },
}), { virtual: true });
jest.mock('../../leaksWorker/blockWorker/nativeCanvas/Painter', () => ({
    Painter: jest.fn().mockImplementation(() => ({
        initialize: jest.fn().mockResolvedValue(undefined),
        setAllocationLines: jest.fn(),
        render: jest.fn().mockResolvedValue(undefined),
    })),
}));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@insight/lib/utils', () => ({ safeJSONParse: JSON.parse }), { virtual: true });
const theme = {
    bgColorCommon: '#fff',
    bgColorLight: '#eee',
    bgColorLighter: '#fff',
    borderColor: '#ccc',
    borderColorLighter: '#aaa',
    primaryColor: '#1677ff',
    textColorSecondary: '#666',
} as any;
const deviceSlices = {
    eventCount: 300,
    sliceCount: 3,
    readySlices: [1, 2],
    slices: [
        { index: 0, startEventId: 0, endEventId: 99, ready: false },
        { index: 1, startEventId: 100, endEventId: 199, ready: true },
        { index: 2, startEventId: 200, endEventId: 299, ready: true },
    ],
};
const overviewData = {
    1: { allocations: [{ timestamp: 100, totalSize: 10 }, { timestamp: 199, totalSize: 20 }] },
    2: { allocations: [{ timestamp: 200, totalSize: 30 }, { timestamp: 299, totalSize: 15 }] },
} as any;
const Harness = ({ onRangeChange = jest.fn() }: { onRangeChange?: (range: [number, number]) => void }): React.ReactElement => {
    const [selected, setSelected] = useState(1);
    const [range, setRange] = useState<[number, number]>([100, 199]);
    return <ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices} overviewData={overviewData}
        selectedSliceIndex={selected} selectedRange={range}
        onSelectSlice={(index): void => { setSelected(index); setRange([index * 100, index * 100 + 99]); }}
        onRangeChange={(next): void => { setRange(next); onRangeChange(next); }} />
    </ThemeProvider>;
};

let nextFrame: FrameRequestCallback | undefined;
const flushFrame = (): void => {
    act(() => {
        const callback = nextFrame;
        nextFrame = undefined;
        callback?.(0);
    });
};

const openPreviewWindow = (view: ReturnType<typeof render>, windowNumber = 2): void => {
    fireEvent.click(view.getByRole('button', { name: 'overviewExpand' }));
    const row = view.queryByText(`snapshotWindow ${windowNumber}`, { selector: '.window-identity' });
    if (row) fireEvent.click(row);
};

describe('MemSnapshotSliceOverview', () => {
    beforeAll(() => {
        window.matchMedia = () => ({ matches: false, addListener: () => {}, removeListener: () => {} } as unknown as MediaQueryList);
        HTMLDialogElement.prototype.showModal = function (): void { this.setAttribute('open', ''); };
        HTMLDialogElement.prototype.close = function (): void { this.removeAttribute('open'); };
        Element.prototype.scrollIntoView = () => {};
        window.PointerEvent = MouseEvent as typeof PointerEvent;
        HTMLElement.prototype.setPointerCapture = jest.fn();
        HTMLElement.prototype.hasPointerCapture = () => true;
        HTMLElement.prototype.releasePointerCapture = jest.fn();
        HTMLElement.prototype.getBoundingClientRect = () => ({ left: 0, top: 0, right: 1000, bottom: 400, width: 1000, height: 400 } as DOMRect);
    });
    beforeEach(() => {
        nextFrame = undefined;
        window.requestAnimationFrame = jest.fn(callback => { nextFrame = callback; return 1; });
        window.cancelAnimationFrame = jest.fn(() => { nextFrame = undefined; });
        (Painter as jest.Mock).mockImplementation(() => ({
            initialize: jest.fn().mockResolvedValue(undefined),
            setAllocationLines: jest.fn(),
            render: jest.fn().mockResolvedValue(undefined),
        }));
    });
    afterEach(() => { jest.useRealTimers(); });

    it('keeps compact range changes and peak markers', () => {
        jest.useFakeTimers();
        const changed = jest.fn();
        const view = render(<Harness onRangeChange={changed} />);
        expect(view.queryByRole('dialog')).toBeNull();
        expect(view.getAllByTestId('globalPeakMarker')).toHaveLength(1);
        expect(view.getByRole('button', { name: /snapshotWindow 2 · overviewPeak/ }).hasAttribute('title')).toBe(false);
        fireEvent.keyDown(view.getByRole('slider', { name: 'overviewRangeStart' }), { key: 'ArrowRight' });
        act(() => { jest.advanceTimersByTime(150); });
        expect(changed).toHaveBeenCalledWith([101, 199]);
    });

    it('opens the global entry at the complete timeline and keeps activation controls in the leading slot', () => {
        const view = render(<Harness />);
        expect(view.getAllByRole('button', { name: 'overviewExpand' })).toHaveLength(1);
        fireEvent.click(view.getByRole('button', { name: 'overviewExpand' }));
        const track = view.getByTestId('expandedSliceOverview');
        expect(track.getAttribute('data-range-start')).toBe('0');
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(track.getAttribute('data-value-min')).toBe('0');
        expect(Number(track.getAttribute('data-value-max'))).toBeCloseTo(30 * 1.08);
        const active = view.getByRole('img', { name: 'overviewActiveWindow' });
        const action = view.getByRole('button', { name: 'overviewActivateWindow · 3' });
        expect(active.parentElement?.className).toBe('window-action-slot');
        expect(action.parentElement?.className).toBe('window-action-slot');
        expect(action.parentElement?.nextElementSibling?.textContent).toBe('snapshotWindow 3');
        expect(view.getByRole('separator').querySelectorAll('button')).toHaveLength(0);
        fireEvent.click(action);
        expect(view.getByRole('img', { name: 'overviewActiveWindow' }).parentElement?.nextElementSibling?.textContent).toBe('snapshotWindow 3');
        fireEvent.click(view.getByRole('button', { name: 'overviewActivateWindow · 2' }));
        expect(view.getByRole('img', { name: 'overviewActiveWindow' }).parentElement?.nextElementSibling?.textContent).toBe('snapshotWindow 2');
    });

    it('links a continuous navigator to the enlarged range across window boundaries', () => {
        const changed = jest.fn();
        const view = render(<Harness onRangeChange={changed} />);
        openPreviewWindow(view);
        const track = view.getByTestId('expandedSliceOverview');
        const navigator = view.getByTestId('overviewNavigator');
        expect(navigator.querySelectorAll('canvas')).toHaveLength(1);
        expect(track.querySelectorAll('canvas')).toHaveLength(1);
        expect(track.getAttribute('data-range-start')).toBe('100');
        expect(track.getAttribute('data-range-end')).toBe('199');
        fireEvent.keyDown(view.getByRole('slider', { name: 'overviewRangeEnd' }), { key: 'ArrowRight' });
        flushFrame();
        expect(track.getAttribute('data-range-end')).toBe('202');
        expect(changed).not.toHaveBeenCalled();
        fireEvent.keyDown(view.getByTestId('overviewCanvas'), { key: 'd' });
        flushFrame();
        expect(Number(track.getAttribute('data-range-start'))).toBeGreaterThan(100);
        const before = Number(track.getAttribute('data-range-end')) - Number(track.getAttribute('data-range-start'));
        fireEvent.wheel(view.getByTestId('overviewCanvas'), { deltaY: -10 });
        flushFrame();
        expect(Number(track.getAttribute('data-range-end')) - Number(track.getAttribute('data-range-start'))).toBeLessThan(before);
        expect(Number(view.getByRole('slider', { name: 'overviewRangeStart' }).getAttribute('aria-valuenow')))
            .toBe(Number(track.getAttribute('data-range-start')));
        expect(changed).not.toHaveBeenCalled();
    });

    it('removes the toolbar and keeps the navigator compact', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        expect(view.queryByRole('button', { name: 'overviewFitAll' })).toBeNull();
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('302%');
        const navigator = view.getByTestId('overviewNavigator');
        expect((navigator.firstElementChild as HTMLElement).style.height).toBe('48px');
        const track = view.getByTestId('expandedSliceOverview');
        expect((track.firstElementChild as HTMLElement).style.height).toBe('348px');
    });

    it('pans the detail by dragging, clamps to the timeline and releases the drag', () => {
        const changed = jest.fn();
        const view = render(<Harness onRangeChange={changed} />);
        openPreviewWindow(view);
        const canvas = view.getByTestId('overviewCanvas');
        const track = view.getByTestId('expandedSliceOverview');
        canvas.scrollTop = 80;
        fireEvent.pointerDown(canvas, { button: 0, clientX: 600, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 400, clientY: 160, pointerId: 1 });
        flushFrame();
        expect(Number(track.getAttribute('data-range-start'))).toBeCloseTo(119.8);
        expect(Number(track.getAttribute('data-range-end'))).toBeCloseTo(218.8);
        expect(canvas.getAttribute('data-dragging')).toBe('true');
        fireEvent.pointerMove(canvas, { clientX: -2000, clientY: 160, pointerId: 1 });
        flushFrame();
        expect(track.getAttribute('data-range-end')).toBe('299');
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 600, clientY: 200, pointerId: 1 });
        flushFrame();
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(canvas.getAttribute('data-dragging')).toBe('false');
        expect(changed).not.toHaveBeenCalled();
    });

    it('collapses the table and browses without updating main data until activation', () => {
        const select = jest.fn();
        const range = jest.fn();
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices}
            overviewData={overviewData} selectedSliceIndex={1} onSelectSlice={select} onRangeChange={range} /></ThemeProvider>);
        openPreviewWindow(view, 3);
        const table = view.getByRole('table');
        expect(table.querySelector('tbody td .window-identity')?.textContent).toBe('snapshotWindow 2');
        expect(view.getByRole('columnheader', { name: 'snapshotWindow' }).getAttribute('aria-sort')).toBe('ascending');
        fireEvent.click(view.getByRole('button', { name: 'overviewWindowMemoryPeak' }));
        fireEvent.click(view.getByRole('button', { name: 'overviewWindowMemoryPeak' }));
        expect(table.querySelector('tbody td .window-identity')?.textContent).toBe('snapshotWindow 3');
        fireEvent.click(view.getByRole('button', { name: 'snapshotWindow' }));
        expect(table.querySelector('tbody td .window-identity')?.textContent).toBe('snapshotWindow 2');
        fireEvent.click(table.querySelectorAll('tbody tr')[0]);
        expect(view.getByTestId('expandedSliceOverview').getAttribute('data-range-start')).toBe('100');
        fireEvent.click(table.querySelectorAll('tbody tr')[1]);
        expect(view.getByTestId('expandedSliceOverview').getAttribute('data-range-start')).toBe('200');
        expect(select).not.toHaveBeenCalled();
        expect(range).not.toHaveBeenCalled();
        fireEvent.click(view.getByRole('button', { name: 'overviewActivateWindow · 3' }));
        expect(select).toHaveBeenCalledTimes(1);
        expect(select).toHaveBeenCalledWith(2);
        expect(range).not.toHaveBeenCalled();
    });

    it('retains the popup, zoom and sidebar state across close and reopen', () => {
        const view = render(<Harness />);
        const opener = view.getAllByRole('button', { name: /overviewExpand/ })[0];
        fireEvent.click(opener);
        const dialog = view.getByRole('dialog');
        fireEvent.wheel(view.getByTestId('overviewCanvas'), { deltaY: -10, clientX: 500, clientY: 200 });
        flushFrame();
        const track = view.getByTestId('expandedSliceOverview');
        const start = track.getAttribute('data-range-start');
        fireEvent.keyDown(view.getByRole('separator', { name: 'overviewResizePanels' }), { key: 'Home' });
        flushFrame();
        expect(view.queryByRole('table')).toBeNull();
        expect(view.getByTestId('overviewCanvas').style.display).not.toBe('none');
        fireEvent.click(view.getByRole('button', { name: 'overviewClose' }));
        expect(dialog.isConnected).toBe(true);
        expect(dialog.hasAttribute('open')).toBe(false);
        fireEvent.click(opener);
        expect(view.getByRole('dialog')).toBe(dialog);
        expect(view.queryByRole('table')).toBeNull();
        expect(track.getAttribute('data-range-start')).toBe(start);
        fireEvent.click(view.getByRole('button', { name: 'overviewShowTable' }));
        expect(view.getByRole('table')).toBeDefined();
    });

    it('coalesces wheel bursts, reuses painters and keeps the canvas backing size stable', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const track = view.getByTestId('expandedSliceOverview');
        const canvas = track.querySelector('canvas') as HTMLCanvasElement;
        const initialWidth = canvas.width;
        const initialHeight = canvas.height;
        const constructors = (Painter as jest.Mock).mock.calls.length;
        const start = track.getAttribute('data-range-start');
        for (let index = 0; index < 10; index++) fireEvent.wheel(view.getByTestId('overviewCanvas'), { deltaY: -1, clientX: 500, clientY: 200 });
        expect(window.requestAnimationFrame).toHaveBeenCalledTimes(1);
        expect(track.getAttribute('data-range-start')).toBe(start);
        flushFrame();
        expect(track.getAttribute('data-range-start')).not.toBe(start);
        expect(Number(track.getAttribute('data-range-end')) - Number(track.getAttribute('data-range-start'))).toBeCloseTo(99 / 1.15 ** 10);
        expect(canvas.width).toBe(initialWidth);
        expect(canvas.height).toBe(initialHeight);
        expect((Painter as jest.Mock).mock.calls.length).toBe(constructors);
    });

    it('pans the vertical viewport without resizing and flushes the last drag on release', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const canvas = view.getByTestId('overviewCanvas');
        const track = view.getByTestId('expandedSliceOverview');
        fireEvent.wheel(canvas, { deltaY: -1, clientX: 500, clientY: 200 });
        flushFrame();
        const min = Number(track.getAttribute('data-value-min'));
        const span = Number(track.getAttribute('data-value-max')) - min;
        fireEvent.pointerDown(canvas, { button: 0, clientX: 600, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 550, clientY: 210, pointerId: 1 });
        expect(Number(track.getAttribute('data-value-min'))).toBe(min);
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        expect(Number(track.getAttribute('data-value-min'))).toBeGreaterThan(min);
        expect(Number(track.getAttribute('data-value-max')) - Number(track.getAttribute('data-value-min'))).toBeCloseTo(span);
        expect(nextFrame).toBeUndefined();
    });

    it('uses the whole timeline as 100% and resets it with keyboard, buttons and middle click', () => {
        const changed = jest.fn();
        const view = render(<Harness onRangeChange={changed} />);
        openPreviewWindow(view);
        expect(view.getAllByTestId('overviewWindowBoundary')).toHaveLength(2);
        const chart = view.getByTestId('overviewCanvas');
        const track = view.getByTestId('expandedSliceOverview');
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('302%');
        fireEvent.pointerDown(view.getByTestId('overviewZoomControls'), { button: 0 });
        expect(chart.getAttribute('data-dragging')).toBe('false');
        fireEvent.keyDown(view.getByTestId('overviewCanvas'), { key: 'r' });
        expect(track.getAttribute('data-range-start')).toBe('0');
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('100%');
        fireEvent.click(view.getByRole('button', { name: 'overviewZoomIn' }));
        flushFrame();
        expect(Number(track.getAttribute('data-range-start'))).toBeGreaterThan(0);
        expect(Number(track.getAttribute('data-value-min'))).toBeGreaterThan(0);
        fireEvent.pointerDown(chart, { button: 1 });
        expect(track.getAttribute('data-range-start')).toBe('0');
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(track.getAttribute('data-value-min')).toBe('0');
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('100%');
        fireEvent.click(view.getByRole('button', { name: 'overviewZoomIn' }));
        flushFrame();
        fireEvent.click(view.getByRole('button', { name: 'overviewReset' }));
        expect(track.getAttribute('data-range-start')).toBe('0');
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('100%');
        expect(changed).not.toHaveBeenCalled();
    });

    it('labels only global maxima including ties and locates their default window without switching main data', () => {
        const select = jest.fn();
        const changed = jest.fn();
        const data = {
            ...overviewData,
            1: {
                allocations: [
                    { timestamp: 100, totalSize: 30 }, { timestamp: 120, totalSize: 30 },
                    { timestamp: 130, totalSize: 10 }, { timestamp: 150, totalSize: 30 }, { timestamp: 199, totalSize: 20 },
                ],
            },
        };
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices}
            overviewData={data} selectedSliceIndex={1} onSelectSlice={select} onRangeChange={changed} /></ThemeProvider>);
        openPreviewWindow(view);
        const navigator = view.getByTestId('overviewNavigator');
        const peaks = navigator.querySelectorAll('button');
        expect(peaks).toHaveLength(3);
        expect(navigator.querySelectorAll('[data-testid="globalPeakPoint"]')).toHaveLength(3);
        expect(Array.from(peaks).every(peak => peak.textContent === '')).toBe(true);
        expect(view.getByTestId('overviewPeakLegend').textContent).toBe('overviewGlobalPeakPoint');
        expect((peaks[2] as HTMLElement).style.left).toBe(`${200 / 299 * 100}%`);
        fireEvent.keyDown(view.getByTestId('overviewCanvas'), { key: 'r' });
        fireEvent.click(view.getByRole('button', { name: 'overviewZoomIn' }));
        flushFrame();
        fireEvent.click(peaks[2]);
        const track = view.getByTestId('expandedSliceOverview');
        expect(track.getAttribute('data-range-start')).toBe('200');
        expect(track.getAttribute('data-range-end')).toBe('299');
        expect(track.getAttribute('data-value-min')).toBe('0');
        expect(view.getByTestId('overviewZoomControls').textContent).toContain('302%');
        expect(select).not.toHaveBeenCalled();
        expect(changed).not.toHaveBeenCalled();
    });

    it('shows zoom controls only during zoom and hides them two seconds after the last action', () => {
        jest.useFakeTimers();
        window.requestAnimationFrame = jest.fn(callback => { nextFrame = callback; return 1; });
        window.cancelAnimationFrame = jest.fn(() => { nextFrame = undefined; });
        const view = render(<Harness />);
        openPreviewWindow(view);
        expect(view.queryByRole('button', { name: 'overviewZoomIn' })).toBeNull();
        fireEvent.wheel(view.getByTestId('overviewCanvas'), { deltaY: -1 });
        flushFrame();
        expect(view.getByRole('button', { name: 'overviewZoomIn' })).toBeDefined();
        act(() => { jest.advanceTimersByTime(1500); });
        fireEvent.click(view.getByRole('button', { name: 'overviewZoomIn' }));
        flushFrame();
        act(() => { jest.advanceTimersByTime(1500); });
        expect(view.getByRole('button', { name: 'overviewZoomIn' })).toBeDefined();
        act(() => { jest.advanceTimersByTime(500); });
        expect(view.queryByRole('button', { name: 'overviewZoomIn' })).toBeNull();
        fireEvent.keyDown(view.getByTestId('overviewCanvas'), { key: 'd' });
        flushFrame();
        expect(view.queryByRole('button', { name: 'overviewZoomIn' })).toBeNull();
    });

    it('keeps focused zoom controls visible and restarts hiding only after focus leaves', () => {
        jest.useFakeTimers();
        window.requestAnimationFrame = jest.fn(callback => { nextFrame = callback; return 1; });
        window.cancelAnimationFrame = jest.fn(() => { nextFrame = undefined; });
        const view = render(<Harness />);
        openPreviewWindow(view);
        const canvas = view.getByTestId('overviewCanvas');
        fireEvent.wheel(canvas, { deltaY: -1 });
        flushFrame();
        const zoomIn = view.getByRole('button', { name: 'overviewZoomIn' });
        const zoomOut = view.getByRole('button', { name: 'overviewZoomOut' });
        act(() => { jest.advanceTimersByTime(1500); zoomIn.focus(); });
        act(() => { jest.advanceTimersByTime(3000); });
        expect(document.activeElement).toBe(zoomIn);
        expect(view.getByRole('button', { name: 'overviewZoomIn' })).toBe(zoomIn);
        act(() => { zoomOut.focus(); jest.advanceTimersByTime(3000); });
        expect(document.activeElement).toBe(zoomOut);
        expect(view.getByRole('button', { name: 'overviewZoomOut' })).toBe(zoomOut);
        act(() => { canvas.focus(); });
        act(() => { jest.advanceTimersByTime(1999); });
        expect(view.getByRole('button', { name: 'overviewZoomIn' })).toBe(zoomIn);
        act(() => { jest.advanceTimersByTime(1); });
        expect(view.queryByRole('button', { name: 'overviewZoomIn' })).toBeNull();
        expect(document.activeElement).toBe(canvas);
    });

    it('names the dialog and chart by their content without restoring a visible title', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        expect(view.getByRole('dialog', { name: 'overviewExpandedTitle' })).toBeDefined();
        expect(view.getByLabelText('overviewChart')).toBe(view.getByTestId('overviewCanvas'));
        expect(view.queryByText('overviewExpandedTitle')).toBeNull();
    });

    it('marks maxima tied at displayed precision across windows and distinct plateaus', () => {
        const gb = 1024 ** 3;
        const data = {
            1: {
                allocations: [
                    { timestamp: 100, totalSize: 37.5121 * gb }, { timestamp: 101, totalSize: 37.5123 * gb },
                    { timestamp: 110, totalSize: 35 * gb }, { timestamp: 120, totalSize: 37.5122 * gb },
                    { timestamp: 199, totalSize: 37.5114 * gb },
                ],
            },
            2: { allocations: [{ timestamp: 200, totalSize: 37.51215 * gb }, { timestamp: 299, totalSize: 30 * gb }] },
        } as any;
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices}
            overviewData={data} selectedSliceIndex={1} onSelectSlice={jest.fn()} onRangeChange={jest.fn()} /></ThemeProvider>);
        expect(view.getAllByTestId('globalPeakMarker')).toHaveLength(3);
        fireEvent.click(view.getByRole('button', { name: 'overviewExpand' }));
        const markers = view.getByTestId('overviewNavigator').querySelectorAll('[data-testid="globalPeakMarker"]');
        expect(markers).toHaveLength(3);
        expect(Array.from(markers).map(marker => marker.getAttribute('aria-label'))).toEqual([
            expect.stringContaining('overviewEvent: 101'), expect.stringContaining('overviewEvent: 120'), expect.stringContaining('overviewEvent: 200'),
        ]);
    });

    it('uses spaced vertical labels for dense windows and permits a narrow selected range', () => {
        const slices = Array.from({ length: 300 }, (_, index) => ({ index, startEventId: index * 100, endEventId: index * 100 + 99, ready: true }));
        const model = { slices, eventCount: 30000, sliceCount: 300, readySlices: slices.map(slice => slice.index) };
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={model}
            overviewData={overviewData} selectedSliceIndex={1} onSelectSlice={jest.fn()} onRangeChange={jest.fn()} /></ThemeProvider>);
        openPreviewWindow(view);
        const labels = view.getAllByTestId('navigatorWindowLabel');
        expect(labels.length).toBeLessThanOrEqual(64);
        expect(labels.every(label => label.getAttribute('data-vertical') === 'true')).toBe(true);
        expect(view.getAllByTestId('overviewWindowBoundary')).toHaveLength(299);
        const handle = view.getByRole('slider', { name: 'overviewRangeStart' });
        expect(Number(handle.getAttribute('aria-valuemax'))).toBeGreaterThanOrEqual(Number(handle.getAttribute('aria-valuenow')));
        expect(view.getByTestId('overviewNavigator').querySelectorAll('button[data-testid="globalPeakMarker"]')).toHaveLength(1);
    });

    it('distinguishes the preview selection from the active main window and forbids reactivation', () => {
        const select = jest.fn();
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices}
            overviewData={overviewData} selectedSliceIndex={1} onSelectSlice={select} onRangeChange={jest.fn()} /></ThemeProvider>);
        openPreviewWindow(view);
        expect(view.queryByText('overviewPeakRanking')).toBeNull();
        expect(view.getByRole('img', { name: 'overviewActiveWindow' })).toBeDefined();
        expect(view.queryByRole('button', { name: 'overviewActivateWindow · 2' })).toBeNull();
        const table = view.getByRole('table');
        const rows = table.querySelectorAll('tbody tr');
        expect(rows[0].className).toBe('overview-selected-row');
        fireEvent.click(rows[1].querySelectorAll('td')[2]);
        expect(rows[1].className).toBe('overview-selected-row');
        expect(rows[0].className).toBe('');
        expect(rows[0].querySelector('[aria-label="overviewActiveWindow"]')).not.toBeNull();
        expect(select).not.toHaveBeenCalled();
        fireEvent.click(view.getByRole('button', { name: 'overviewActivateWindow · 3' }));
        expect(select).toHaveBeenCalledTimes(1);
        expect(select).toHaveBeenCalledWith(2);
    });

    it('keeps activation inside the same popup and follows the active window prop', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const dialog = view.getByRole('dialog');
        const track = view.getByTestId('expandedSliceOverview');
        const start = track.getAttribute('data-range-start');
        fireEvent.click(view.getByRole('button', { name: 'overviewActivateWindow · 3' }));
        expect(view.getByRole('dialog')).toBe(dialog);
        expect(dialog.hasAttribute('open')).toBe(true);
        expect(view.queryByRole('button', { name: 'overviewActivateWindow · 3' })).toBeNull();
        expect(view.getByRole('button', { name: 'overviewActivateWindow · 2' })).toBeDefined();
        expect(view.getByRole('img', { name: 'overviewActiveWindow' }).closest('tr')?.textContent).toContain('snapshotWindow 3');
        expect(track.getAttribute('data-range-start')).toBe(start);
    });

    it('allows small zoom-dependent overscan and no vertical blank panning at minimum zoom', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const canvas = view.getByTestId('overviewCanvas');
        const track = view.getByTestId('expandedSliceOverview');
        const valueMax = Number(track.getAttribute('data-value-max'));
        fireEvent.pointerDown(canvas, { button: 0, clientX: 500, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 500, clientY: -1000, pointerId: 1 });
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        expect(Number(track.getAttribute('data-value-min'))).toBe(0);
        fireEvent.pointerDown(canvas, { button: 0, clientX: 500, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 500, clientY: 2000, pointerId: 1 });
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        expect(Number(track.getAttribute('data-value-max'))).toBe(valueMax);
        fireEvent.wheel(canvas, { deltaY: -1, clientX: 500, clientY: 200 });
        flushFrame();
        const span = Number(track.getAttribute('data-value-max')) - Number(track.getAttribute('data-value-min'));
        const padding = Math.min(span * 0.08, (valueMax - span) * 0.08);
        fireEvent.pointerDown(canvas, { button: 0, clientX: 500, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 500, clientY: -1000, pointerId: 1 });
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        expect(Number(track.getAttribute('data-value-min'))).toBeCloseTo(-padding);
        fireEvent.pointerDown(canvas, { button: 0, clientX: 500, clientY: 200, pointerId: 1 });
        fireEvent.pointerMove(canvas, { clientX: 500, clientY: 2000, pointerId: 1 });
        fireEvent.pointerUp(canvas, { pointerId: 1 });
        expect(Number(track.getAttribute('data-value-max'))).toBeCloseTo(valueMax + padding);
        fireEvent.wheel(canvas, { deltaY: 1, clientX: 500, clientY: 200 });
        flushFrame();
        expect(Number(track.getAttribute('data-value-min'))).toBe(0);
        expect(Number(track.getAttribute('data-value-max'))).toBe(valueMax);
    });

    it('resizes panels by dragging or keyboard and retains the width when hiding and reopening the table', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const divider = view.getByRole('separator', { name: 'overviewResizePanels' });
        const panel = view.getByTestId('overviewRankingPane');
        fireEvent.pointerDown(divider, { button: 0, clientX: 600, pointerId: 1 });
        fireEvent.pointerMove(divider, { clientX: 480, pointerId: 1 });
        fireEvent.pointerUp(divider, { pointerId: 1 });
        expect(panel.style.width).toBe('430px');
        fireEvent.keyDown(divider, { key: 'ArrowRight' });
        flushFrame();
        expect(panel.style.width).toBe('406px');
        fireEvent.keyDown(view.getByRole('separator', { name: 'overviewResizePanels' }), { key: 'Home' });
        flushFrame();
        expect(panel.style.display).toBe('none');
        fireEvent.click(view.getByRole('button', { name: 'overviewShowTable' }));
        expect(panel.style.width).toBe('406px');
        fireEvent.keyDown(divider, { key: 'Home' });
        flushFrame();
        expect(panel.style.display).toBe('none');
        expect(view.getByRole('button', { name: 'overviewShowTable' })).toBeDefined();
        fireEvent.keyDown(divider, { key: 'End' });
        flushFrame();
        expect(panel.style.width).toBe('980px');
        expect(view.getByTestId('overviewChartPane').style.display).toBe('none');
        fireEvent.click(view.getByRole('button', { name: 'overviewShowChart' }));
        expect(panel.style.width).toBe('406px');
        expect(view.getByTestId('overviewChartPane').style.display).not.toBe('none');
    });

    it('collapses either panel at its drag threshold and permits dragging the collapsed edge open', () => {
        const view = render(<Harness />);
        openPreviewWindow(view);
        const divider = view.getByRole('separator', { name: 'overviewResizePanels' });
        const table = view.getByTestId('overviewRankingPane');
        const chart = view.getByTestId('overviewChartPane');
        const drag = (from: number, to: number): void => {
            fireEvent.pointerDown(divider, { button: 0, clientX: from, pointerId: 1 });
            fireEvent.pointerMove(divider, { clientX: to, pointerId: 1 });
            fireEvent.pointerUp(divider, { pointerId: 1 });
        };
        drag(600, 800);
        expect(table.style.display).toBe('none');
        expect(view.getByRole('button', { name: 'overviewShowTable' })).toBeDefined();
        drag(980, 680);
        expect(table.style.display).not.toBe('none');
        expect(table.style.width).toBe('300px');
        drag(680, 120);
        expect(chart.style.display).toBe('none');
        expect(view.getByRole('button', { name: 'overviewShowChart' })).toBeDefined();
        drag(0, 250);
        expect(chart.style.display).not.toBe('none');
        expect(table.style.width).toBe('730px');
        fireEvent.keyDown(divider, { key: 'End' });
        flushFrame();
        expect(chart.style.display).toBe('none');
        fireEvent.doubleClick(divider);
        expect(chart.style.display).not.toBe('none');
        expect(table.style.width).toBe('310px');
    });

    it('handles missing data without inventing curves or peaks', () => {
        const view = render(<ThemeProvider theme={theme}><MemSnapshotSliceOverview deviceSlices={deviceSlices}
            overviewData={{}} selectedSliceIndex={1} onSelectSlice={jest.fn()} onRangeChange={jest.fn()} /></ThemeProvider>);
        openPreviewWindow(view);
        expect(view.container.querySelectorAll('polyline')).toHaveLength(0);
        expect(view.getByRole('table').querySelectorAll('tbody tr')).toHaveLength(0);
    });
});

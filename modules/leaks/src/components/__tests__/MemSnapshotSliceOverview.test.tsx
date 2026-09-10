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
import MemSnapshotSliceOverview from '../MemSnapshotSliceOverview';

jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
const theme = {
    bgColorCommon: '#fff',
    bgColorLight: '#f5f5f5',
    borderColor: '#ccc',
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
    1: { allocations: [{ timestamp: 100, totalSize: 10 }, { timestamp: 199, totalSize: 20 }], minTimestamp: 100, maxTimestamp: 199 },
    2: { allocations: [{ timestamp: 200, totalSize: 30 }, { timestamp: 299, totalSize: 15 }], minTimestamp: 200, maxTimestamp: 299 },
};

describe('MemSnapshotSliceOverview', () => {
    afterEach(() => {
        jest.useRealTimers();
    });

    it('renders pending placeholders and only gives the selected ready slice an interactive zoom', () => {
        jest.useFakeTimers();
        const onSelectSlice = jest.fn();
        const onRangeChange = jest.fn();
        const view = render(<ThemeProvider theme={theme}>
            <MemSnapshotSliceOverview
                deviceSlices={deviceSlices}
                overviewData={overviewData as any}
                selectedSliceIndex={2}
                onSelectSlice={onSelectSlice}
                onRangeChange={onRangeChange}
            />
        </ThemeProvider>);

        expect(view.getAllByText('pending')).toHaveLength(1);
        const rangeHandles = view.getAllByRole('slider');
        expect(rangeHandles).toHaveLength(2);
        rangeHandles.forEach(handle => expect(handle.classList.contains('slice-range-handle')).toBe(true));
        expect(view.container.querySelectorAll('.slice-trend-area')).toHaveLength(2);
        expect(view.container.querySelectorAll('polyline')).toHaveLength(2);
        expect(view.container.querySelector('.slice-trend-area')?.getAttribute('points')).toBe(
            '0,36 0,23.333333333333336 100,12.666666666666668 100,36',
        );
        expect(view.getAllByTestId('sliceAxisCoordinate').map(item => item.textContent)).toEqual([
            '0',
            '100',
            '200',
            '299',
        ]);

        const blocks = view.getAllByRole('button');
        expect(blocks[0].getAttribute('data-ready')).toBe('false');
        expect(blocks[0].getAttribute('aria-disabled')).toBe('true');
        expect(blocks[0].getAttribute('tabindex')).toBe('-1');
        expect(blocks[2].getAttribute('data-selected')).toBe('true');
        fireEvent.click(blocks[0]);
        fireEvent.keyDown(blocks[0], { key: 'Enter' });
        expect(onSelectSlice).not.toHaveBeenCalled();

        fireEvent.keyDown(view.getByRole('slider', { name: 'Range start' }), { key: 'ArrowRight' });
        expect(onRangeChange).not.toHaveBeenCalled();
        jest.advanceTimersByTime(150);
        expect(onRangeChange).toHaveBeenCalledWith([201, 299]);
    });

    it('supports keyboard switching between slice windows', () => {
        const onSelectSlice = jest.fn();
        const view = render(<ThemeProvider theme={theme}>
            <MemSnapshotSliceOverview
                deviceSlices={deviceSlices}
                overviewData={overviewData as any}
                selectedSliceIndex={1}
                onSelectSlice={onSelectSlice}
                onRangeChange={jest.fn()}
            />
        </ThemeProvider>);

        fireEvent.keyDown(view.getAllByRole('button')[2], { key: 'Enter' });
        expect(onSelectSlice).toHaveBeenCalledWith(2);
    });

    it('allocates slice widths according to their event counts', () => {
        const unevenSlices = {
            eventCount: 120,
            sliceCount: 2,
            readySlices: [0, 1],
            slices: [
                { index: 0, startEventId: 0, endEventId: 99, ready: true },
                { index: 1, startEventId: 100, endEventId: 119, ready: true },
            ],
        };

        const view = render(<ThemeProvider theme={theme}>
            <MemSnapshotSliceOverview
                deviceSlices={unevenSlices}
                overviewData={{}}
                selectedSliceIndex={0}
                onSelectSlice={jest.fn()}
                onRangeChange={jest.fn()}
            />
        </ThemeProvider>);
        const blocks = view.getAllByRole('button');

        expect(blocks[0].style.flexGrow).toBe('100');
        expect(blocks[1].style.flexGrow).toBe('20');
    });

    it('calculates the maximum size without spreading large allocation arrays', () => {
        const pointCount = 200000;
        const largeOverviewData = {
            0: {
                allocations: Array.from({ length: pointCount }, (_, index) => ({
                    timestamp: index,
                    totalSize: index,
                })),
                minTimestamp: 0,
                maxTimestamp: pointCount - 1,
            },
        };
        const singleSlice = {
            eventCount: pointCount,
            sliceCount: 1,
            readySlices: [0],
            slices: [{ index: 0, startEventId: 0, endEventId: pointCount - 1, ready: true }],
        };

        const view = render(<ThemeProvider theme={theme}>
            <MemSnapshotSliceOverview
                deviceSlices={singleSlice}
                overviewData={largeOverviewData as any}
                selectedSliceIndex={0}
                onSelectSlice={jest.fn()}
                onRangeChange={jest.fn()}
            />
        </ThemeProvider>);

        expect(view.getAllByRole('slider')).toHaveLength(2);
    });
});

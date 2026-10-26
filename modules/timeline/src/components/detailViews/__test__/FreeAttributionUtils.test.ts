/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import type { FreeAttributionRangeMode } from '../../../api/interface';
import { freeAttributionTimeRange } from '../FreeAttributionUtils';

interface RangeCase {
    name: string;
    range: [number, number] | undefined;
    offset: number;
    expected: { rangeMode: FreeAttributionRangeMode; startTime: number; endTime: number };
}

it.each<RangeCase>([
    { name: 'no selection with a fractional offset', range: undefined, offset: 20.5, expected: { rangeMode: 'all', startTime: 0, endTime: 0 } },
    { name: 'original equal-boundary sentinel', range: [50, 50], offset: -100, expected: { rangeMode: 'all', startTime: 0, endTime: 0 } },
    { name: 'finite selection touching raw zero', range: [0, 100], offset: -100, expected: { rangeMode: 'range', startTime: 0, endTime: 0 } },
    { name: 'selection wholly before raw zero', range: [0, 100], offset: -101, expected: { rangeMode: 'empty', startTime: 0, endTime: 0 } },
    { name: 'fractional end before raw zero', range: [0, 100], offset: -100.5, expected: { rangeMode: 'empty', startTime: 0, endTime: 0 } },
    { name: 'partial intersection after a negative offset', range: [0, 100], offset: -99.5, expected: { rangeMode: 'range', startTime: 0, endTime: 1 } },
    { name: 'positive offset applied before clipping', range: [-20, 30], offset: 10, expected: { rangeMode: 'range', startTime: 0, endTime: 40 } },
    { name: 'negative display range crossing raw zero', range: [-200, -100], offset: 150, expected: { rangeMode: 'range', startTime: 0, endTime: 50 } },
    { name: 'fractional boundaries rounded outwards', range: [10.5, 20.5], offset: 1.25, expected: { rangeMode: 'range', startTime: 11, endTime: 22 } },
    { name: 'ordinary positive range', range: [0, 100], offset: 20, expected: { rangeMode: 'range', startTime: 20, endTime: 120 } },
])('distinguishes $name', ({ range, offset, expected }) => {
    expect(freeAttributionTimeRange(range, offset)).toEqual(expected);
});

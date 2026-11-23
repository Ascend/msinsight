/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { drawFilledLineIntervals, findFilledLineInterval, findFilledLineValueRange } from './filledLineUtils';

describe('filled line series mode', () => {
    const data = [
        [0, 10, 20],
        [1, 30, 5],
    ];

    it('uses the accumulated value for existing stacked counters', () => {
        expect(findFilledLineValueRange(data, 'stacked')).toEqual([0, 42]);
    });

    it('uses each independent value for an overlay counter', () => {
        expect(findFilledLineValueRange(data, 'overlay')).toEqual([0, 36]);
    });
});

describe('filled line explicit intervals', () => {
    const data = [[10, 4], [30, 0], [40, 6]];
    const ends = [20, 40, 50];

    const makeContext = (): CanvasRenderingContext2D => ({
        beginPath: jest.fn(),
        moveTo: jest.fn(),
        lineTo: jest.fn(),
        closePath: jest.fn(),
        fill: jest.fn(),
        stroke: jest.fn(),
        setLineDash: jest.fn(),
    }) as unknown as CanvasRenderingContext2D;

    it('finds a value throughout its measured interval, including the final interval', () => {
        expect(findFilledLineInterval(data, ends, 10)).toBe(data[0]);
        expect(findFilledLineInterval(data, ends, 19.9)).toBe(data[0]);
        expect(findFilledLineInterval(data, ends, 49.9)).toBe(data[2]);
    });

    it('leaves gaps and the exclusive final endpoint without a tooltip', () => {
        [0, 20, 29.9, 50, 51, NaN].forEach(time => {
            expect(findFilledLineInterval(data, ends, time)).toBeUndefined();
        });
        expect(findFilledLineInterval([], [], 10)).toBeUndefined();
    });

    it('chooses the next interval at a shared endpoint and retains a valid zero', () => {
        expect(findFilledLineInterval(data, ends, 30)).toBe(data[1]);
        expect(findFilledLineInterval(data, ends, 40)).toBe(data[2]);
    });

    it('rejects malformed values and missing or nonpositive interval ends', () => {
        expect(findFilledLineInterval([[10, NaN]], [20], 15)).toBeUndefined();
        expect(findFilledLineInterval([[10, 1]], [], 15)).toBeUndefined();
        expect(findFilledLineInterval([[10, 1]], [10], 10)).toBeUndefined();
        const context = makeContext();
        drawFilledLineIntervals({
            context,
            dataList: [[10, NaN], [20, 1], [30, 2]],
            intervalEnds: [15, 20],
            xScale: value => value,
            yScale: value => 100 - value,
            minHeight: 0,
            palette: ['blue'],
            seriesMode: 'stacked',
            isIE: false,
        });
        expect(context.fill).not.toHaveBeenCalled();
    });

    it('fills adjoining steps in one path and ends each path at the actual measurement boundary', () => {
        const context = makeContext();
        drawFilledLineIntervals({
            context,
            dataList: data,
            intervalEnds: ends,
            xScale: value => value + 0.5,
            yScale: value => 100 - value,
            minHeight: 0,
            palette: ['blue'],
            seriesMode: 'stacked',
            isIE: false,
        });
        expect((context.moveTo as jest.Mock).mock.calls).toEqual([[10.5, 100], [30.5, 100]]);
        expect((context.lineTo as jest.Mock).mock.calls).toEqual([
            [10.5, 95], [20.5, 95], [20.5, 100],
            [30.5, 99], [40.5, 99], [40.5, 93], [50.5, 93], [50.5, 100],
        ]);
        expect(context.fill).toHaveBeenCalledTimes(2);
    });

    it('supports overlay intervals without stacking or joining neighbouring segments', () => {
        const context = makeContext();
        drawFilledLineIntervals({
            context,
            dataList: [[10, 2, 5], [30, 7, 3]],
            intervalEnds: [20, 40],
            xScale: value => value,
            yScale: value => 100 - value,
            minHeight: 0,
            palette: ['blue', 'red'],
            seriesMode: 'overlay',
            isIE: false,
        });
        expect((context.moveTo as jest.Mock).mock.calls).toEqual([[10, 98], [10, 95], [30, 93], [30, 97]]);
        expect((context.lineTo as jest.Mock).mock.calls).toEqual([[20, 98], [20, 95], [40, 93], [40, 97]]);
        expect(context.stroke).toHaveBeenCalledTimes(4);
        expect(context.fill).not.toHaveBeenCalled();
    });
});

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

import {
    FULL_TIME_RANGE,
    hasActiveTimeRange,
    normalizeTimeAnalysisRange,
} from '@/features/overview/model/timeRange';

describe('overview time range', () => {
    it.each([
        [[100.9, 200.1], { startTime: 100, endTime: 201 }],
        [[-10.5, 20.1], { startTime: 0, endTime: 21 }],
        [[10, 20, 30], { startTime: 10, endTime: 20 }],
        [null, FULL_TIME_RANGE],
        [[100], FULL_TIME_RANGE],
        [[200, 100], FULL_TIME_RANGE],
        [[Number.NaN, 200], FULL_TIME_RANGE],
        [[100, Number.POSITIVE_INFINITY], FULL_TIME_RANGE],
    ])('normalizes range %#', (value, expected) => {
        expect(normalizeTimeAnalysisRange(value)).toEqual(expected);
    });

    it.each([
        [{ startTime: 10, endTime: 11 }, true],
        [FULL_TIME_RANGE, false],
        [{ startTime: 10, endTime: 10 }, false],
    ])('reports active state for %o', (range, expected) => {
        expect(hasActiveTimeRange(range)).toBe(expected);
    });
});

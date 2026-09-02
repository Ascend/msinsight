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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { getBarCenterTimestamp, getBarTimeBounds, getSeamlessBarBounds } from '../barBounds';

describe('StackedBarChart bar bounds', () => {
    it('snaps adjacent fractional bars so no background pixel remains between them', () => {
        const first = getSeamlessBarBounds(10.25, 5);
        const second = getSeamlessBarBounds(15.25, 5);

        expect(first[0] + first[1]).toBeGreaterThanOrEqual(second[0]);
    });

    it('keeps a real multi-pixel data gap visible', () => {
        const first = getSeamlessBarBounds(10.25, 5);
        const second = getSeamlessBarBounds(18.25, 5);

        expect(first[0] + first[1]).toBeLessThan(second[0]);
    });

    it('uses the timestamp as the bucket start for LLC-aligned bars', () => {
        expect(getBarTimeBounds(100, 20, 'start')).toEqual([100, 120]);
        expect(getBarCenterTimestamp(100, 20, 'start')).toBe(110);
    });

    it('preserves center positioning as the default for existing stacked bars', () => {
        expect(getBarTimeBounds(100, 20)).toEqual([90, 110]);
        expect(getBarCenterTimestamp(100, 20)).toBe(100);
    });
});

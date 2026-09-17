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

import { getThreadingBucketWidthNs, mapThreadingCounterData, THREADING_STATE_COLORS } from '../threadingAnalysis';
import { findContinuousPoint, smoothContinuousValues, splitContinuousSegments } from '../../../components/charts/continuousArea';

describe('threading analysis counter mapping', () => {
    it.each([0, -1, undefined, null, NaN, Infinity])('uses a positive width fallback for %s', (width) => {
        expect(getThreadingBucketWidthNs(width)).toBe(500_000_000);
    });

    it('keeps a valid declared width', () => {
        expect(getThreadingBucketWidthNs(100_000_000)).toBe(100_000_000);
    });

    it.each([undefined, null, NaN, Infinity])('skips a bucket with invalid state duration %s', (unknownSeconds) => {
        expect(mapThreadingCounterData([{
            timestamp: 0,
            value: {
                Active: 25,
                'Sync Wait': 25,
                Preemption: 25,
                Unknown: 25,
                activeSeconds: 0.1,
                syncWaitSeconds: 0.1,
                preemptionSeconds: 0.1,
                unknownSeconds,
                bucketWidthNs: 400_000_000,
            } as never,
        }], 0)).toEqual([]);
    });
    it('uses the state palette defined by the GUI requirement', () => {
        expect(THREADING_STATE_COLORS).toEqual(['#818CF8', '#FB923C', '#C90000', '#94A3B8']);
    });

    it('maps state percentages, durations and timeline offset', () => {
        const result = mapThreadingCounterData([{
            timestamp: 1_000_000_000,
            value: {
                Active: 30,
                'Sync Wait': 45,
                Preemption: 15,
                Unknown: 10,
                activeSeconds: 0.09,
                syncWaitSeconds: 0.135,
                preemptionSeconds: 0.045,
                unknownSeconds: 0.03,
                bucketWidthNs: 500_000_000,
            },
        }], 250_000_000);

        expect(result).toEqual([{
            timestamp: 750_000_000,
            values: [30, 45, 15, 10],
            stateSeconds: [0.09, 0.135, 0.045, 0.03],
            bucketWidthNs: 500_000_000,
        }]);
    });

    it('skips buckets with missing numeric fields', () => {
        const result = mapThreadingCounterData([{
            timestamp: 10,
            value: {} as never,
        }], 0);

        expect(result).toEqual([]);
    });

    it('smooths chart boundaries without mutating raw tooltip values', () => {
        const raw = [
            { timestamp: 0, values: [20, 40, 30, 10] },
            { timestamp: 1, values: [70, 10, 10, 10] },
            { timestamp: 2, values: [20, 40, 30, 10] },
            { timestamp: 3, values: [70, 10, 10, 10] },
            { timestamp: 4, values: [20, 40, 30, 10] },
        ];

        const smoothed = smoothContinuousValues(raw);

        expect(raw[2].values).toEqual([20, 40, 30, 10]);
        expect(smoothed[2].values[0]).toBeCloseTo(41.0526);
        expect(smoothed[2].values.reduce((sum, value) => sum + value, 0)).toBeCloseTo(100);
        expect(smoothed.map(item => item.timestamp)).toEqual(raw.map(item => item.timestamp));
    });
});

describe('continuous state intervals', () => {
    const points = [0, 100, 300].map(timestamp => ({ timestamp, values: [25, 25, 25, 25] }));

    it('handles empty and single-point segments', () => {
        expect(splitContinuousSegments([], 100)).toEqual([]);
        expect(splitContinuousSegments(points.slice(0, 1), 100)).toEqual([[points[0]]]);
        expect(smoothContinuousValues(points.slice(0, 1))).toEqual([points[0]]);
    });

    it('splits a missing bucket and preserves the exact tolerance boundary', () => {
        expect(splitContinuousSegments(points, 100)).toEqual([[points[0], points[1]], [points[2]]]);
        expect(splitContinuousSegments([{ timestamp: 0 }, { timestamp: 150 }], 100)).toHaveLength(1);
        expect(splitContinuousSegments([{ timestamp: 0 }, { timestamp: 151 }], 100)).toHaveLength(2);
    });

    it('hits the next bucket on its start but not a gap or the final exclusive end', () => {
        expect(findContinuousPoint(0, points, 100)).toBe(points[0]);
        expect(findContinuousPoint(100, points, 100)).toBe(points[1]);
        expect(findContinuousPoint(200, points, 100)).toBeUndefined();
        expect(findContinuousPoint(250, points, 100)).toBeUndefined();
        expect(findContinuousPoint(399, points, 100)).toBe(points[2]);
        expect(findContinuousPoint(400, points, 100)).toBeUndefined();
        expect(findContinuousPoint(-1, points, 100)).toBeUndefined();
    });
});

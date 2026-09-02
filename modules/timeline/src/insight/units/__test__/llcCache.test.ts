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

import { DEFAULT_LLC_BUCKET_WIDTH_NS, getLlcBucketWidthNs, LLC_CACHE_COLORS, mapLlcCacheCounterData } from '../llcCache';

describe('LLC cache counter mapping', () => {
    it('stacks misses below hits with their documented colors', () => {
        expect(LLC_CACHE_COLORS).toEqual(['#EF4444', '#22C55E']);
    });

    it('maps stable LLC counts, rates, bucket width and timeline offset', () => {
        const result = mapLlcCacheCounterData([{
            timestamp: 1_000_000_000,
            value: {
                hits: 900,
                misses: 100,
                totalAccesses: 1000,
                hitRate: 90,
                missRate: 10,
                bucketWidthNs: 500_000_000,
            },
        }], 250_000_000, 100_000_000);

        expect(result).toEqual([{
            timestamp: 750_000_000,
            values: [100, 900],
            hits: 900,
            misses: 100,
            totalAccesses: 1000,
            hitRate: 90,
            missRate: 10,
            bucketWidthNs: 500_000_000,
        }]);
    });

    it('derives totals and rates when optional canonical values are absent', () => {
        const result = mapLlcCacheCounterData([{
            timestamp: 10,
            value: { hits: 3, misses: 1 } as never,
        }], 0, 200);

        expect(result[0].totalAccesses).toBe(4);
        expect(result[0].hitRate).toBe(75);
        expect(result[0].missRate).toBe(25);
        expect(result[0].values).toEqual([1, 3]);
        expect(result[0].bucketWidthNs).toBe(200);
    });

    it('normalizes inconsistent canonical rates without replacing the absolute chart counts', () => {
        const result = mapLlcCacheCounterData([{
            timestamp: 10,
            value: {
                hits: 8,
                misses: 2,
                totalAccesses: 10,
                hitRate: 40,
                missRate: 10,
                bucketWidthNs: 20,
            },
        }], 0, 20);

        expect(result[0].values).toEqual([2, 8]);
        expect(result[0].hitRate).toBe(80);
        expect(result[0].missRate).toBe(20);
    });

    it('falls back to a positive bucket width when metadata and data contain zero', () => {
        expect(getLlcBucketWidthNs(0, 0)).toBe(DEFAULT_LLC_BUCKET_WIDTH_NS);
        expect(getLlcBucketWidthNs(undefined, 200)).toBe(200);
    });
});

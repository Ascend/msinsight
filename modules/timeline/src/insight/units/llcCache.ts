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

import type { StackedBarData } from '../../entity/chart';

export const LLC_CACHE_METRIC_GROUP = 'llc_cache';
export const LLC_CACHE_COLORS = ['#EF4444', '#22C55E'];
export const DEFAULT_LLC_BUCKET_WIDTH_NS = 500_000_000;

export interface LlcCacheCounterValue {
    hits: number;
    misses: number;
    totalAccesses: number;
    hitRate: number;
    missRate: number;
    bucketWidthNs: number;
}

export interface LlcCacheCounterData {
    timestamp: number;
    value: LlcCacheCounterValue;
}

export interface LlcCacheStackedBarData extends StackedBarData {
    hits: number;
    misses: number;
    totalAccesses: number;
    hitRate: number;
    missRate: number;
    bucketWidthNs: number;
}

export const getLlcBucketWidthNs = (bucketWidthNs: unknown, fallbackBucketWidthNs: unknown): number => {
    const parsedBucketWidth = Number(bucketWidthNs);
    if (Number.isFinite(parsedBucketWidth) && parsedBucketWidth > 0) {
        return parsedBucketWidth;
    }
    const parsedFallback = Number(fallbackBucketWidthNs);
    return Number.isFinite(parsedFallback) && parsedFallback > 0
        ? parsedFallback
        : DEFAULT_LLC_BUCKET_WIDTH_NS;
};

export const mapLlcCacheCounterData = (
    data: LlcCacheCounterData[], timestampOffset: number, fallbackBucketWidthNs: number,
): LlcCacheStackedBarData[] => data.map(({ timestamp, value }) => {
    const hits = Math.max(0, Number(value.hits) || 0);
    const misses = Math.max(0, Number(value.misses) || 0);
    const totalAccesses = Math.max(0, Number(value.totalAccesses) || hits + misses);
    const rawHitRate = totalAccesses > 0
        ? Number.isFinite(Number(value.hitRate)) ? Number(value.hitRate) : hits * 100 / totalAccesses
        : 0;
    const rawMissRate = totalAccesses > 0
        ? Number.isFinite(Number(value.missRate)) ? Number(value.missRate) : misses * 100 / totalAccesses
        : 0;
    const rateTotal = Math.max(0, rawHitRate) + Math.max(0, rawMissRate);
    const hitRate = rateTotal > 0 ? Math.max(0, rawHitRate) * 100 / rateTotal : 0;
    const missRate = rateTotal > 0 ? Math.max(0, rawMissRate) * 100 / rateTotal : 0;
    return {
        timestamp: timestamp - timestampOffset,
        values: [misses, hits],
        hits,
        misses,
        totalAccesses,
        hitRate,
        missRate,
        bucketWidthNs: getLlcBucketWidthNs(value.bucketWidthNs, fallbackBucketWidthNs),
    };
});

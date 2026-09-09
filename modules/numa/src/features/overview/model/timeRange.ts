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

export interface MetricTimeRange {
    startTime: number;
    endTime: number;
}

export const FULL_TIME_RANGE: MetricTimeRange = { startTime: 0, endTime: 0 };

export function normalizeTimeAnalysisRange(value: unknown): MetricTimeRange {
    if (!Array.isArray(value) || value.length < 2) return FULL_TIME_RANGE;
    const startTime = Math.max(0, Math.floor(Number(value[0])));
    const endTime = Math.max(0, Math.ceil(Number(value[1])));
    if (!Number.isSafeInteger(startTime) || !Number.isSafeInteger(endTime) || startTime >= endTime) {
        return FULL_TIME_RANGE;
    }
    return { startTime, endTime };
}

export function hasActiveTimeRange(range: MetricTimeRange): boolean {
    return range.startTime < range.endTime;
}

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

export const THREADING_STATE_NAMES = ['Active', 'Sync Wait', 'Preemption', 'Unknown'] as const;
export const THREADING_STATE_COLORS = ['#818CF8', '#FB923C', '#C90000', '#94A3B8'];
export const DEFAULT_THREADING_BUCKET_WIDTH_NS = 500_000_000;
export const THREADING_STATE_METRIC_GROUP = 'thread_state';
export const getThreadingBucketWidthNs = (value: unknown, fallback = DEFAULT_THREADING_BUCKET_WIDTH_NS): number => {
    const width = Number(value);
    return Number.isFinite(width) && width > 0 ? width : fallback;
};
const THREADING_STATE_DURATION_NAMES = [
    'activeSeconds', 'syncWaitSeconds', 'preemptionSeconds', 'unknownSeconds',
] as const;

export interface ThreadingCounterValue {
    Active: number;
    'Sync Wait': number;
    Preemption: number;
    Unknown: number;
    activeSeconds: number;
    syncWaitSeconds: number;
    preemptionSeconds: number;
    unknownSeconds: number;
    bucketWidthNs: number;
}

export interface ThreadingCounterData {
    timestamp: number;
    value: ThreadingCounterValue;
}

export interface ThreadingStackedBarData extends StackedBarData {
    stateSeconds: number[];
    bucketWidthNs: number;
}

const isValidMetricValue = (value: unknown): boolean =>
    value !== undefined && value !== null && Number.isFinite(Number(value));

const isCompleteThreadingCounterValue = (value: ThreadingCounterValue): boolean =>
    value !== undefined &&
    THREADING_STATE_NAMES.every(name => isValidMetricValue(value[name])) &&
    THREADING_STATE_DURATION_NAMES.every(name => isValidMetricValue(value[name]));

export const mapThreadingCounterData = (
    data: ThreadingCounterData[], timestampOffset: number, bucketWidthNs?: number,
): ThreadingStackedBarData[] => data
    .filter(({ value }) => isCompleteThreadingCounterValue(value))
    .map(({ timestamp, value }) => ({
        timestamp: timestamp - timestampOffset,
        values: THREADING_STATE_NAMES.map(name => Number(value[name]) || 0),
        stateSeconds: [
            Number(value.activeSeconds) || 0,
            Number(value.syncWaitSeconds) || 0,
            Number(value.preemptionSeconds) || 0,
            Number(value.unknownSeconds) || 0,
        ],
        bucketWidthNs: getThreadingBucketWidthNs(bucketWidthNs, getThreadingBucketWidthNs(value.bucketWidthNs)),
    }));

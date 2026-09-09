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

import type { Metric } from './types';

export function formatMetricValue(value: number, unit: string): string {
    const digits = Math.abs(value) >= 100 ? 0 : Math.abs(value) >= 10 ? 1 : 2;
    return `${value.toFixed(digits)}${unit ? ` ${unit}` : ''}`;
}

export function formatMetric(metric: Metric, noDataLabel: string): string {
    return metric.hasValue === false ? noDataLabel : formatMetricValue(metric.value, metric.unit);
}

export function formatTimelineTimestamp(timestamp: number, origin: number): string {
    const relativeNs = Math.max(0, timestamp - origin);
    const hours = Math.floor(relativeNs / 3_600_000_000_000);
    const minutes = Math.floor(relativeNs % 3_600_000_000_000 / 60_000_000_000);
    const seconds = Math.floor(relativeNs % 60_000_000_000 / 1_000_000_000);
    const milliseconds = Math.floor(relativeNs % 1_000_000_000 / 1_000_000);
    const microseconds = Math.floor(relativeNs % 1_000_000 / 1_000);
    const nanoseconds = Math.floor(relativeNs % 1_000);
    const lowerUnits = [milliseconds, microseconds, nanoseconds].map(value => value.toString().padStart(3, '0'));
    const clock = [minutes, seconds].map(value => value.toString().padStart(2, '0'));
    if (hours > 0) clock.unshift(hours.toString().padStart(2, '0'));
    return `${clock.join(':')}.${lowerUnits.join('.')}`;
}

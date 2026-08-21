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

const MILLISECONDS_PER_MINUTE = 60 * 1000;
const MILLISECONDS_PER_HOUR = 60 * MILLISECONDS_PER_MINUTE;
const MILLISECONDS_PER_DAY = 24 * MILLISECONDS_PER_HOUR;
const MILLISECOND_TIMESTAMP_THRESHOLD = 1_000_000_000_000;

export type RelativeTimeValue = Date | number | string;

/** Returns a new array sorted by date from newest to oldest, preserving order for invalid dates. */
export const sortByTimeDescending = <T>(items: T[], getTime: (item: T) => RelativeTimeValue | undefined): T[] => {
    return items
        .map((item, index) => ({ item, index, timestamp: parseOptionalTimestamp(getTime(item)) }))
        .sort((left, right) => {
            if (left.timestamp === null && right.timestamp === null) return left.index - right.index;
            if (left.timestamp === null) return 1;
            if (right.timestamp === null) return -1;
            return right.timestamp - left.timestamp || left.index - right.index;
        })
        .map(({ item }) => item);
};

/** Formats a date as a compact elapsed time, preserving values that are not valid dates. */
export const formatRelativeTime = (value: RelativeTimeValue, now: RelativeTimeValue = Date.now()): string => {
    const timestamp = parseTimestamp(value);
    const currentTimestamp = parseTimestamp(now);
    if (timestamp === null || currentTimestamp === null) return String(value);

    const elapsed = Math.max(0, currentTimestamp - timestamp);
    if (elapsed < MILLISECONDS_PER_MINUTE) return '<1m';
    if (elapsed < MILLISECONDS_PER_HOUR) return `${Math.floor(elapsed / MILLISECONDS_PER_MINUTE)}m`;
    if (elapsed < MILLISECONDS_PER_DAY) return `${Math.floor(elapsed / MILLISECONDS_PER_HOUR)}h`;
    return `${Math.floor(elapsed / MILLISECONDS_PER_DAY)}d`;
};

const parseTimestamp = (value: RelativeTimeValue): number | null => {
    if (value instanceof Date) {
        const timestamp = value.getTime();
        return Number.isFinite(timestamp) ? timestamp : null;
    }
    if (typeof value === 'number') {
        if (!Number.isFinite(value)) return null;
        return value < MILLISECOND_TIMESTAMP_THRESHOLD ? value * 1000 : value;
    }
    const timestamp = Date.parse(value);
    return Number.isFinite(timestamp) ? timestamp : null;
};

const parseOptionalTimestamp = (value: RelativeTimeValue | undefined): number | null => {
    return value === undefined ? null : parseTimestamp(value);
};

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

export const getSeamlessBarBounds = (start: number, width: number): [number, number] => {
    const snappedStart = Math.floor(start);
    const snappedEnd = Math.ceil(start + width);
    return [snappedStart, Math.max(1, snappedEnd - snappedStart)];
};

export type BarTimestampPosition = 'center' | 'start';

export const getBarTimeBounds = (
    timestamp: number, width: number, position: BarTimestampPosition = 'center',
): [number, number] => position === 'start'
    ? [timestamp, timestamp + width]
    : [timestamp - (width / 2), timestamp + (width / 2)];

export const getBarCenterTimestamp = (
    timestamp: number, width: number, position: BarTimestampPosition = 'center',
): number => position === 'start' ? timestamp + (width / 2) : timestamp;

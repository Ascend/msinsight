/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

import type { FreeAttributionDetail, FreeAttributionRangeMode, OpDetail } from '../../api/interface';
import type { Session } from '../../entity/session';
import { getTimeOffset, type OffsetSide } from '../../insight/units/offset';

export const freeAttributionTimeRange = (range: [number, number] | undefined, deviceOffset: number): {
    rangeMode: FreeAttributionRangeMode; startTime: number; endTime: number;
} => {
    // Preserve the original full-data sentinel before applying offsets or clipping.
    if (range === undefined || range[0] === range[1]) {
        return { rangeMode: 'all', startTime: 0, endTime: 0 };
    }
    const rawStart = range[0] + deviceOffset;
    const rawEnd = range[1] + deviceOffset;
    // Check before rounding: ceil(-0.5) is zero but the interval is still empty.
    if (rawEnd < 0) {
        return { rangeMode: 'empty', startTime: 0, endTime: 0 };
    }
    return {
        rangeMode: 'range',
        startTime: Math.max(0, Math.floor(rawStart)),
        endTime: Math.max(0, Math.ceil(rawEnd)),
    };
};

/** Numeric columns use microseconds; event identities retain their original nanoseconds. */
export const freeTimeUs = (ns: number | undefined): string => ns === undefined
    ? '--'
    : (ns / 1000).toFixed(3).replace(/\.?0+$/, '');

export const freeTargetStart = (session: Session, target?: OpDetail | null, side: OffsetSide = 'host'): string =>
    target === undefined || target === null
        ? '--'
        : freeTimeUs(target.timestamp - getTimeOffset(session, { ...target, offsetSide: side }));

export const freeApiCandidates = (record: FreeAttributionDetail): OpDetail[] => {
    if (record.hostApiTarget) return [record.hostApiTarget];
    return record.candidateApiTargets ?? [];
};

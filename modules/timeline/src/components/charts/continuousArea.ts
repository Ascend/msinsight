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

const CONTINUOUS_SMOOTH_WEIGHTS = [1, 2, 3, 4, 5, 4, 3, 2, 1];
const CONTINUOUS_SMOOTH_RADIUS = Math.floor(CONTINUOUS_SMOOTH_WEIGHTS.length / 2);

export const splitContinuousSegments = <T extends { timestamp: number }>(data: T[], bucketWidthNs: number): T[][] =>
    data.reduce<T[][]>((segments, item) => {
        const current = segments[segments.length - 1];
        const previous = current?.[current.length - 1];
        if (previous === undefined || item.timestamp - previous.timestamp > bucketWidthNs * 1.5) {
            segments.push([item]);
        } else {
            current.push(item);
        }
        return segments;
    }, []);

export const findContinuousPoint = <T extends { timestamp: number }>(
    timestamp: number, data: T[], bucketWidthNs: number,
): T | undefined => data.slice().reverse().find(item =>
    timestamp >= item.timestamp && timestamp < item.timestamp + bucketWidthNs);

export const smoothContinuousValues = <T extends { timestamp: number; values: number[] }>(segment: T[]): T[] => {
    return segment.map((item, index) => {
        const values = item.values.map((_, stateIndex) => {
            let weightedValue = 0;
            let appliedWeight = 0;
            CONTINUOUS_SMOOTH_WEIGHTS.forEach((weight, weightIndex) => {
                const neighbor = segment[index + weightIndex - CONTINUOUS_SMOOTH_RADIUS];
                if (neighbor === undefined) {
                    return;
                }
                weightedValue += (neighbor.values[stateIndex] ?? 0) * weight;
                appliedWeight += weight;
            });
            return appliedWeight === 0 ? item.values[stateIndex] : weightedValue / appliedWeight;
        });
        return { ...item, values };
    });
};

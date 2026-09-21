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
import type { CounterMetaData } from '../../entity/data';

const UNIT_ONLY_COUNTER_TYPES = new Set([
    '%',
    'B',
    'Byte',
    'B/s',
    'Byte/s',
    'Packet',
    'Packet/s',
    'Pkt',
    'Pkt/s',
    'Retry',
    'Rty',
    'Cycle',
    'Level',
    'Hz',
    'KHz',
    'Mhz',
    'MHz',
    'GHz',
    'ns',
    'us',
    'ms',
    's',
]);

const UNIT_IN_BRACKETS_REGEXP = /\(([^()]+)\)\s*$/;

const extractUnitFromDataType = (dataType: string): string | undefined => {
    const trimmedDataType = dataType.trim();
    const bracketMatch = trimmedDataType.match(UNIT_IN_BRACKETS_REGEXP);
    if (bracketMatch?.[1] !== undefined) {
        return bracketMatch[1].trim();
    }
    return UNIT_ONLY_COUNTER_TYPES.has(trimmedDataType) ? trimmedDataType : undefined;
};

const getCounterUnit = (dataType: string[]): string | undefined => {
    const units = dataType.map(extractUnitFromDataType);
    if (units.length === 0 || units.some(unit => unit === undefined)) {
        return undefined;
    }
    const [firstUnit] = units;
    return units.every(unit => unit === firstUnit) ? firstUnit : undefined;
};

const hasUnitSuffix = (name: string, unit: string): boolean => {
    const trimmedName = name.trim();
    if (trimmedName.endsWith(`(${unit})`)) {
        return true;
    }
    return UNIT_IN_BRACKETS_REGEXP.test(trimmedName);
};

export const getCounterLaneDisplayName = (metadata: CounterMetaData): string => {
    const unit = getCounterUnit(metadata.dataType);
    if (unit === undefined || hasUnitSuffix(metadata.threadName, unit)) {
        return metadata.threadName;
    }
    return `${metadata.threadName} (${unit})`;
};

export const getCounterSeriesMode = (metadata: CounterMetaData): 'stacked' | 'overlay' => {
    // Rx and Tx are independent measurements. Stacking them would imply a value that does not exist.
    return metadata.metaType === 'UB' && metadata.dataType.length > 1 ? 'overlay' : 'stacked';
};

export const getCounterLegend = (metadata: CounterMetaData): string[] | undefined => {
    return getCounterSeriesMode(metadata) === 'overlay' ? metadata.dataType : undefined;
};

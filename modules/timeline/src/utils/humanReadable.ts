/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { clamp } from 'lodash';
import type { TimeStamp } from '../entity/common';
import { adaptTime, getLastValue, isLowerUnit } from './adaptTimeForLength';
import type { GetLength, GetPadder, Time, TimeOptions } from './adaptTimeForLength';

export type Readable = (inputs: number[]) => { value: number[]; unit: string };

const MEMUNIT = [
    'KB',
    'MB',
    'GB',
    'TB',
    'PB',
    'EB',
    'ZB',
    'YB',
    'BB',
];

const KILO = 1024;

/**
 * Output readable memory data, e.g. 204800KB => 200MB
 * @param inputKB numbers in KB
 * @returns numbers that readable
 */
export const getReadableMem: Readable = (inputKB) => {
    if (inputKB.length === 0) {
        return {
            value: [0],
            unit: MEMUNIT[0],
        };
    }
    let data = Math.max(...inputKB);
    let unitIndex = 0;
    const zoom = (): void => {
        if (data > KILO && unitIndex < (MEMUNIT.length - 1)) {
            data /= KILO;
            unitIndex += 1;
            zoom();
        }
    };
    zoom();
    return {
        value: inputKB.map(n => n / Math.pow(KILO, unitIndex)),
        unit: MEMUNIT[unitIndex],
    };
};

export function formatMemoryData(data: number): string {
    const item = getReadableMem([data]);
    return `${item.value[0].toFixed(2)} ${item.unit}`;
}

export const getTextualDuration = (endTimeAll: number | undefined): string => {
    if (endTimeAll === undefined) {
        return '00:00:00.000';
    }
    const time = endTimeAll;
    const hour = Math.floor(time / 3600000);
    const minute = Math.floor(time % 3600000 / 60000);
    const second = Math.floor(time % 60000 / 1000);
    const millisecond = time % 1000;
    let mesc = '';
    if (millisecond < 10) {
        mesc = `00${millisecond}`;
    } else if (millisecond < 100) {
        mesc = `0${millisecond}`;
    } else {
        mesc = `${millisecond}`;
    }

    return `${hour < 10 ? (`0${hour}`) : hour}:${minute < 10 ? (`0${minute}`) : minute}:${second < 10 ? (`0${second}`) : second}.${mesc}`;
};

/**
 * Converts a fixed-point number to percent string.
 *
 * @param numPercent the number to be displayed as a percent string
 * @param digits the number of digits to appear after the decimal point, should be between 0 to 20 inclusive
 * @returns a percent string representation of the given number
 */
export const toPercent = (numPercent: number, digits?: number): string => {
    return `${(numPercent * 100).toFixed(digits)}%`;
};

const DEFAULT_DURATION_OPTIONS: TimeOptions = {
    precision: 'ns',
    maxChars: Number.MAX_SAFE_INTEGER,
};
export const getDuration = (time: TimeStamp, options: TimeOptions = DEFAULT_DURATION_OPTIONS): string => {
    const getLength: GetLength = (timeItem) => {
        const timeLength = timeItem?.length ?? 0;
        const timeUnitLength = timeItem?.unit.length ?? 0;
        return timeLength + timeUnitLength + 1;
    };
    const getPadder: GetPadder = (splitTime, emptyPadder, originPadStartIdx, timesIdx, originLength) => {
        let padStartIdx = originPadStartIdx;
        let length = originLength;
        const handleEmptyPadder = (index: number): void => {
            if (emptyPadder[index] !== undefined) {
                splitTime.unshift(emptyPadder[index]);
            }
        };
        if (options?.segments !== undefined) {
            if (padStartIdx !== 0) {
                splitTime.unshift(...emptyPadder.slice(-padStartIdx).map(item => item));
            }
        } else {
            if (padStartIdx === Infinity) {
                return [splitTime, timesIdx];
            }
            do {
                handleEmptyPadder(padStartIdx);
                (length as number) += getLength(emptyPadder[padStartIdx]);
                padStartIdx--;
            } while ((length as number) < (options.maxChars as number) - 2 && padStartIdx >= 0);
        }

        return [splitTime, timesIdx];
    };
    const { splitTime: duration, tail } = adaptTime(time, { getLength, getPadder, ...options });
    const removeInvalidNum = (times: Time[]): Time[] => {
        for (let i = 0; i < times.length; i++) {
            if (times[i].value !== 0 || i === times.length - 1) {
                return times.slice(i);
            }
        }
        return times;
    };
    const validTime = removeInvalidNum(duration);
    return validTime.map((item, index) => `${getLastValue(index === validTime.length - 1, item.value, tail)}${item.unit}`).join(' ');
};

const DEFAULT_TIMESTAMP_OPTIONS: TimeOptions = {
    precision: 'ms',
    segments: 6,
};

const getLength = (time?: Time): number => {
    let padder = 2;
    if (time?.unit === 'ms') {
        padder = 3;
    }
    const length = time?.length ?? 0;
    const padderLength = time?.value === 0 ? padder : 0;
    return length + Math.max(0, padderLength - length);
};

export const getTimestamp = (originTime: TimeStamp, options: TimeOptions = DEFAULT_TIMESTAMP_OPTIONS): string => {
    let time = originTime;
    const sign = time < 0 ? '-' : '';
    time = Math.abs(time);
    const mode = options?.maxChars === undefined
        ? { segments: 6, ...options }
        : options;
    const getPadder: GetPadder = (splitTime, originEmptyPadder, originPadStartIdx, originTimesIdx, originLength) => {
        let emptyPadder = originEmptyPadder;
        let padStartIdx = originPadStartIdx;
        let timesIdx = originTimesIdx;
        let length = originLength;
        let resTime: Time[] = [];
        const handleEmptyPadder = (index: number): void => {
            if (emptyPadder[index] !== undefined) {
                splitTime.unshift(emptyPadder[index]);
            }
        };
        if (padStartIdx === Infinity || (mode.maxChars as number) === Infinity) {
            return [resTime, timesIdx];
        }
        if (mode?.segments !== undefined) {
            emptyPadder = emptyPadder.reverse();
            for (let i = 0; i <= padStartIdx || isLowerUnit(splitTime[0]) || splitTime[0]?.unit === 's'; i++) {
                handleEmptyPadder(i);
            }
            resTime = splitTime.slice(0, mode.segments);
            timesIdx = clamp(splitTime.length - resTime.length, 0, splitTime.length) - timesIdx;
        } else {
            do {
                handleEmptyPadder(padStartIdx);
                (length as number) += getLength(emptyPadder[padStartIdx]);
                padStartIdx--;
            } while ((length as number) < (mode.maxChars as number) - 2 && padStartIdx >= 0);

            resTime = splitTime;
        }
        return [resTime, timesIdx];
    };
    const { splitTime: timestamp } = adaptTime(time, { getLength, getPadder, ...mode });
    let separator: ':' | '.' = ':';
    let maxLength = 2;
    return sign + timestamp.reduce((acc, item, index) => {
        if (isLowerUnit(item)) {
            maxLength = 3;
            separator = '.';
        }
        const isInvalidHour = item.unit === 'hour' && item.value === 0;
        const splitter = index === 0 || isInvalidHour ? '' : separator;
        const res = isInvalidHour ? '' : `${acc}${splitter}${item.value.toString().padStart(maxLength, '0')}`;
        return res;
    }, '');
};

/**
 * toLocalTimeString
 * @param timestamp timestamp
 */
export function toLocalTimeString(timestamp: number): string {
    const date = new Date(timestamp);
    return `${date.toLocaleDateString('zh', { hour12: false }).replaceAll('/', '-')
    } ${date.toLocaleTimeString('zh', { hour12: false }).replaceAll('/', '-')}`;
}

/**
 * Converts a number to thousand separated form. eg: 12345678 => 12,345,678
 */
export function thousandSeparated(n: number): string {
    return Math.floor(n).toLocaleString('en-US');
}

/**
 * Transform camel-case field to frontend readable string
 * @param camelCaseStr eg: sampleString
 * @returns eg: Sample String
 */
export function transformCamelCaseToReadable(camelCaseStr: string): string {
    if (camelCaseStr.length < 2 || !/^[a-zA-Z]+$/.test(camelCaseStr)) {
        throw new Error('Invalid camel case string for func transformCamelCaseToReadable()! Input string must be pure letters whose length is over 2.');
    }
    const initialUpperStr = camelCaseStr[0].toUpperCase() + camelCaseStr.substring(1);
    const pattern = /[A-Z][a-z]+/g;
    const readableStr = initialUpperStr.match(pattern);
    return readableStr?.join(' ') as string;
}

export const nsToMs = (numPercent: number, digits?: number): string => {
    return `${(numPercent / 1e6).toFixed(digits)}`;
};

export function formatDigit(input: number): string {
    if (input >= 10) {
        return input.toFixed(2);
    } else {
        return input.toPrecision(4);
    }
}

export function formatTimestamp(timestamp: number, format: string = 'YYYY-MM-DD HH:mm:ss'): string {
    const date = new Date(timestamp);
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    const milliseconds = String(date.getMilliseconds()).padStart(3, '0');

    // 替换格式中的占位符
    return format
        .replace('YYYY', year.toString())
        .replace('MM', month)
        .replace('DD', day)
        .replace('HH', hours)
        .replace('mm', minutes)
        .replace('ss', seconds)
        .replace('SSS', milliseconds);
}

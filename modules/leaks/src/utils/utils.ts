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
import { safeJSONParse } from '@insight/lib/utils';
export const chartResize = (ins: echarts.ECharts | null | undefined): void => {
    // ehcarts的custom在渲染超大量数据时会出现残影残留，通过手动触发resize可以消除残影的影响
    if (!ins) {
        return;
    }
    const width = ins.getWidth();
    ins.resize({
        width: width + 1,
    });
    ins.resize({
        width: 'auto',
    });
};
export const generateJsonShow = (text: string): string => {
    if (text === '{}' || text === '') return text;
    const jsonObj = safeJSONParse(text);
    if (jsonObj === null || typeof jsonObj === 'number') return text;
    let res = '';
    Object.keys(jsonObj).forEach((key: string) => { res += `${key}:${jsonObj[key]}\n`; });
    return res;
};
export const convertNanoseconds = (totalNs: number): string => {
    const seconds = Math.floor(totalNs / 1e9);
    const remainingAfterSeconds = totalNs % 1e9;
    const milliseconds = Math.floor(remainingAfterSeconds / 1e6);
    const remainingAfterMilliseconds = remainingAfterSeconds % 1e6;
    const microseconds = Math.floor(remainingAfterMilliseconds / 1e3);
    const nanoseconds = remainingAfterMilliseconds % 1e3;
    return `${seconds}s ${milliseconds.toString().padStart(3, '0')}ms ${microseconds.toString().padStart(3, '0')}us ${nanoseconds.toString().padStart(3, '0')}ns`;
};

const BASE_TIME = 1000 * 1000;
export const formatTime = (time: number, module: string = 'leaks'): string => {
    if (module === 'leaks') {
        return (time / BASE_TIME).toFixed(3);
    }
    return time.toFixed(3);
};

// 单位定义（按 1024 进制）
const units = ['B', 'KB', 'MB', 'GB', 'TB'];
const thresholds = [
    1,
    1024,
    1024 * 1024,
    1024 * 1024 * 1024,
    1024 * 1024 * 1024 * 1024,
];

export const formatBytes = (bytes: number): string => {
    const symbol = bytes < 1 ? -1 : 1;
    // 找到合适的单位
    let unitIndex = 0;
    for (let i = thresholds.length - 1; i >= 0; i--) {
        if (bytes * symbol >= thresholds[i]) {
            unitIndex = i;
            break;
        }
    }

    const value = (bytes / thresholds[unitIndex]).toFixed(3);
    return `${value} ${units[unitIndex]}`;
};

export const addAddressOffset = (address: string, offset: number): string => {
    try {
        const value = BigInt(address) + BigInt(Math.trunc(offset));
        if (/^0x/i.test(address)) {
            return `0x${value.toString(16)}`;
        }
        return value.toString();
    } catch (_error) {
        return address;
    }
};

export const convertBytesToMBytes = (bytes: number): number => bytes / 1024 / 1024;

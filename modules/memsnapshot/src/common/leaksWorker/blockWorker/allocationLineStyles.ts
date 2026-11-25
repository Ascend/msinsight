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

export const RESERVED_LINE_STYLE = {
    key: 'reservedLine',
    color: '#0052D9',
    webglColor: [0, 0.32, 0.85, 1] as const,
    labelKey: 'reservedLineLegend',
} as const;

export const PROCESS_USED_LINE_STYLE = {
    key: 'processUsedLine',
    color: '#ED7B2F',
    webglColor: [0.93, 0.48, 0.18, 1] as const,
    labelKey: 'processUsedLineLegend',
} as const;

export const DEVICE_USED_LINE_STYLE = {
    key: 'deviceUsedLine',
    color: '#00A870',
    webglColor: [0, 0.66, 0.44, 1] as const,
    labelKey: 'deviceUsedLineLegend',
} as const;

export const ALLOCATION_LINE_STYLES = [
    RESERVED_LINE_STYLE,
    PROCESS_USED_LINE_STYLE,
    DEVICE_USED_LINE_STYLE,
] as const;

export type AllocationLineKey = typeof ALLOCATION_LINE_STYLES[number]['key'];

export const DEFAULT_ALLOCATION_LINE_VISIBILITY: AllocationLineVisibility = {
    reservedLine: true,
    processUsedLine: true,
    deviceUsedLine: true,
};

export const isAllocationLineVisible = (
    visibility: AllocationLineVisibility | undefined,
    key: AllocationLineKey,
): boolean => visibility?.[key] !== false;

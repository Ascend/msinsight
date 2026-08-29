/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
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

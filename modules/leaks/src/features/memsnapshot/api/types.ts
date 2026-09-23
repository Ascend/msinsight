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
import type { ReservedPoint, ProcessUsedPoint, DeviceUsedPoint } from '../../../shared/api/types';

export interface AllocationLinePaginationTotal {
    reservedLine: number;
}
export interface AllocationLineData {
    minTimestamp: number;
    maxTimestamp: number;
    reservedLine?: ReservedPoint[];
    processUsedLine?: ProcessUsedPoint[];
    deviceUsedLine?: DeviceUsedPoint[];
    total?: AllocationLinePaginationTotal;
}
export interface LeakStatsParam {
    deviceId: string;
    startTimestamp: number;
    endTimestamp: number;
    sliceIndex?: number;
}
export interface LeakStatsData {
    totalSize: number;
    maxSize: number;
    minSize: number;
}
export interface EvenItem {
    id: number;
    blockId: number;
    size: number;
    address: string;
    action: string;
    stream: number;
}
export interface EventList {
    total: number;
    data: EvenItem[];
}

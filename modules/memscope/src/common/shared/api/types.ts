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
export interface BarData {
    blockData: BlockData;
    allocationData: AllocationData;
    isDark: boolean;
    getNewGraphData: any;
};
export interface BlockData {
    minTimestamp: number;
    maxTimestamp: number;
    minSize: number;
    maxSize: number;
    blocks: Block[];
};
export interface Block {
    id: number;
    addr: string;
    size: number;
    startTimestamp: number;
    endTimestamp: number;
    owner: string;
    attr: string;
    path?: number[][];
}
export interface AllocationData {
    minTimestamp: number;
    maxTimestamp: number;
    maxSize?: number;
    allocations: Allocation[];
    reservedLine?: ReservedPoint[];
    processUsedLine?: ProcessUsedPoint[];
    deviceUsedLine?: DeviceUsedPoint[];
    total?: AllocationPaginationTotal;
}
export interface AllocationPaginationTotal {
    allocations: number;
}
export interface Allocation {
    id?: number;
    timestamp: number;
    totalSize: number;
}
export interface ReservedPoint {
    timestamp: number;
    reservedSize: number;
}
export interface ProcessUsedPoint {
    timestamp: number;
    processUsed: number;
}
export interface DeviceUsedPoint {
    timestamp: number;
    deviceUsed: number;
}
export interface GraphParam {
    deviceId: string;
    eventType: string;
    relativeTime?: boolean;
    startTimestamp?: number;
    endTimestamp?: number;
    currentPage?: number;
    pageSize?: number;
    sliceIndex?: number;
}
export interface ThreShold {
    perT: number | null;
    valueT: number | null;
}
export interface BlockParam {
    deviceId: string;
    eventType: string;
    isTable: boolean;
    relativeTime?: boolean;
    startTimestamp?: number;
    endTimestamp?: number;
    orderBy?: string;
    desc?: boolean | string;
    currentPage?: number;
    pageSize?: number;
    filters?: { [key: string]: string };
    rangeFilters?: { [key: string]: number[] };
    lazyUsedThreshold?: ThreShold;
    delayedFreeThreshold?: ThreShold;
    longIdleThreshold?: ThreShold;
    onlyInefficient?: boolean;
    onlyUnreleasedInRange?: boolean;
    sliceIndex?: number;
}
export interface EventParam {
    deviceId: string;
    relativeTime?: boolean;
    startTimestamp?: number;
    endTimestamp?: number;
    orderBy?: string;
    desc?: boolean;
    currentPage?: number;
    pageSize?: number;
    filters?: { [key: string]: string };
    rangeFilters?: { [key: string]: number[] };
    isTable?: boolean;
    startEventIdx?: number;
    endEventIdx?: number;
    sliceIndex?: number;
}
interface TableHead {
    name: string;
    key: string;
    sortable: boolean;
    searchable: boolean;
}
interface TableDetail {
    id: number;
    event: string;
    eventType: string;
    name: string;
    timestamp: number;
    processId: number;
    threadId: number;
    deviceId: string;
    ptr: string;
    attr: string;
}
export interface BlocksTableData {
    headers: TableHead[];
    blocks: TableDetail[];
    total: number;
}
export interface EventsTableData {
    headers: TableHead[];
    events: TableDetail[];
    total: number;
}

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
    allocations: Allocation[];
}
export interface Allocation {
    id: number;
    timestamp: number;
    totalSize: number;
    reservedSize?: number;
}
export interface GraphParam {
    deviceId: string;
    eventType: string;
    relativeTime?: boolean;
    startTimestamp?: number;
    endTimestamp?: number;
}
export interface FuncParam {
    deviceId: string;
    threadId: number;
    relativeTime?: boolean;
    startTimestamp?: number;
    endTimestamp?: number;
    allowTrim: boolean;
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
}
export interface LeakStatsParam {
    deviceId: string;
    startTimestamp: number;
    endTimestamp: number;
}
export interface LeakStatsData {
    totalSize: number;
    maxSize: number;
    minSize: number;
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
}
export interface DetailData {
    size: number;
    name: string;
    subNodes?: DetailData[];
}
export interface Trace {
    depth: number;
    endTimestamp: number;
    startTimestamp: number;
    func: string;
}
export interface FuncData {
    minTimestamp: number;
    maxTimestamp: number;
    traces: Trace[];
    maxDepth: number;
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

/**
 * 获取块图信息
 * @param params 查询条件
 * @returns 查询结果
 */
export const getBlocksGraphData = async (params: BlockParam): Promise<RenderData> => {
    return window.request({ command: 'Memory/leaks/blocks', params: { ...params } });
};

export const getSnapshotBlocks = async (params: BlockParam): Promise<RenderData> => {
    return window.request({ command: 'Memory/snapshot/blocks', params: { ...params } });
};

/**
 * 获取缩略轴数据
 * @param params 查询条件
 * @returns 查询结果
 */
export const getLeaksAllocationsData = async (params: GraphParam): Promise<AllocationData> => {
    return window.request({ command: 'Memory/leaks/allocations', params: { ...params } });
};

export const getSnapshotAllocations = async (params: GraphParam): Promise<AllocationData> => {
    return window.request({ command: 'Memory/snapshot/allocations', params: { ...params } });
};

/**
 * 获取内存拆解详情
 * @param params 查询条件
 * @returns 查询结果
 */
export const getMemoryDetailData = async (deviceId: string, timestamp: number, eventType: string): Promise<DetailData> => {
    return window.request({ command: 'Memory/leaks/details', params: { deviceId, timestamp, eventType, relativeTime: true } });
};
/**
 * 获取函数调用详情
 * @param params 查询条件
 * @returns 查询结果
 */
export const getFuncData = async (params: FuncParam): Promise<FuncData> => {
    return window.request({ command: 'Memory/leaks/traces', params: { ...params } });
};
/**
 * 获取内存详情表（内存块视图）
 * @param params 查询条件
 * @returns 查询结果
 */
export const getBlockDetails = async (params: BlockParam): Promise<BlocksTableData> => {
    return window.request({ command: 'Memory/leaks/blocks', params: { ...params } });
};

export const getSnapshotBlockTable = async (params: BlockParam): Promise<BlocksTableData> => {
    return window.request({ command: 'Memory/snapshot/blocks', params: { ...params } });
};

export const getSnapshotLeakStats = async (params: LeakStatsParam): Promise<LeakStatsData> => {
    return window.request({ command: 'Memory/snapshot/leakStats', params: { ...params } });
};

/**
 * 获取内存详情表（内存事件视图）
 * @param params 查询条件
 * @returns 查询结果
 */
export const getEventDetails = async (params: EventParam): Promise<EventsTableData> => {
    return window.request({ command: 'Memory/leaks/events', params: { ...params } });
};

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

export const getSnapshotEvent = async (params: EventParam): Promise<EventsTableData> => {
    return window.request({ command: 'Memory/snapshot/events', params: { ...params } });
};

export const getMemoryStateData = async (params: { eventId: number; deviceId: string }): Promise<{ segments: Segment[] }> => {
    return window.request({ command: 'Memory/snapshot/state', params: { ...params } });
};

export const getSnapshotDetail = async (params: { id: number; type: string; deviceId: string }): Promise<{ [key: string]: any }> => {
    return window.request({ command: 'Memory/snapshot/detail', params: { ...params } });
};

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
import type {
    AllocationData, BlockParam, BlocksTableData, EventParam, EventsTableData, GraphParam,
} from '../../../shared/api/types';
import type { AllocationLineData, LeakStatsData, LeakStatsParam } from './types';

export const getSnapshotBlocks = async (params: BlockParam): Promise<RenderData> => {
    return window.request({ command: 'Memory/snapshot/blocks', params: { ...params } });
};

export const getSnapshotAllocations = async (params: GraphParam): Promise<AllocationData> => {
    return window.request({ command: 'Memory/snapshot/allocations', params: { ...params } });
};

export const getSnapshotAllocationLines = async (params: GraphParam): Promise<AllocationLineData> => {
    return window.request({ command: 'Memory/snapshot/allocationLines', params: { ...params } });
};

export const getSnapshotBlockTable = async (params: BlockParam): Promise<BlocksTableData> => {
    return window.request({ command: 'Memory/snapshot/blocks', params: { ...params } });
};

export const getSnapshotLeakStats = async (params: LeakStatsParam): Promise<LeakStatsData> => {
    return window.request({ command: 'Memory/snapshot/leakStats', params: { ...params } });
};

export const getSnapshotEvent = async (params: EventParam): Promise<EventsTableData> => {
    return window.request({ command: 'Memory/snapshot/events', params: { ...params } });
};

export const getMemoryStateData = async (params: { eventId: number; deviceId: string; sliceIndex?: number }): Promise<{ segments: Segment[] }> => {
    return window.request({ command: 'Memory/snapshot/state', params: { ...params } });
};

export const getSnapshotDetail = async (params: {
    id: number;
    type: string;
    deviceId: string;
    eventId?: number;
    segmentAddress?: string;
    stream?: number;
    sliceIndex?: number;
}): Promise<{ [key: string]: any }> => {
    return window.request({ command: 'Memory/snapshot/detail', params: { ...params } });
};

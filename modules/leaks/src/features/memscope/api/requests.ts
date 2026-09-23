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
import type { DetailData, FuncData, FuncParam } from './types';

export const getBlocksGraphData = async (params: BlockParam): Promise<RenderData> => {
    return window.request({ command: 'Memory/leaks/blocks', params: { ...params } });
};

export const getLeaksAllocationsData = async (params: GraphParam): Promise<AllocationData> => {
    return window.request({ command: 'Memory/leaks/allocations', params: { ...params } });
};

export const getMemoryDetailData = async (deviceId: string, timestamp: number, eventType: string): Promise<DetailData> => {
    return window.request({ command: 'Memory/leaks/details', params: { deviceId, timestamp, eventType, relativeTime: true } });
};

export const getFuncData = async (params: FuncParam): Promise<FuncData> => {
    return window.request({ command: 'Memory/leaks/traces', params: { ...params } });
};

export const getBlockDetails = async (params: BlockParam): Promise<BlocksTableData> => {
    return window.request({ command: 'Memory/leaks/blocks', params: { ...params } });
};

export const getEventDetails = async (params: EventParam): Promise<EventsTableData> => {
    return window.request({ command: 'Memory/leaks/events', params: { ...params } });
};

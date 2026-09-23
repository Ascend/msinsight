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
export type {
    BarData,
    BlockData,
    Block,
    AllocationData,
    AllocationPaginationTotal,
    Allocation,
    ReservedPoint,
    ProcessUsedPoint,
    DeviceUsedPoint,
    GraphParam,
    ThreShold,
    BlockParam,
    EventParam,
    BlocksTableData,
    EventsTableData,
} from '../shared/api/types';

export type {
    FuncParam,
    DetailData,
    Trace,
    FuncData,
} from '../features/memscope/api/types';

export type {
    AllocationLinePaginationTotal,
    AllocationLineData,
    LeakStatsParam,
    LeakStatsData,
    EvenItem,
    EventList,
} from '../features/memsnapshot/api/types';

// Keep existing callers on the same interface while feature implementations are separated.
export {
    getBlocksGraphData,
    getLeaksAllocationsData,
    getMemoryDetailData,
    getFuncData,
    getBlockDetails,
    getEventDetails,
} from '../features/memscope/api/requests';

export {
    getSnapshotBlocks,
    getSnapshotAllocations,
    getSnapshotAllocationLines,
    getSnapshotBlockTable,
    getSnapshotLeakStats,
    getSnapshotEvent,
    getMemoryStateData,
    getSnapshotDetail,
} from '../features/memsnapshot/api/requests';

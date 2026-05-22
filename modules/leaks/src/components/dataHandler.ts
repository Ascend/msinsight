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
import { workerSetMemoryBlockData, workerTransform } from '@/leaksWorker/blockWorker/worker';
import {
    getMemoryDetailData, getFuncData, getBlockDetails, getEventDetails,
    FuncParam, type BlockParam, EventParam,
    ThreShold,
    getBlocksGraphData,
    getLeaksAllocationsData,
    getSnapshotBlocks,
    getSnapshotBlockTable,
    getSnapshotEvent,
    getSnapshotAllocations,
    getSnapshotLeakStats,
} from '../utils/RequestUtils';
import { message } from 'antd';
import { runInAction } from 'mobx';

export const getFuncNewData = async (session: any, startTimestamp?: number, endTimestamp?: number): Promise<void> => {
    try {
        const funcParam: FuncParam = { deviceId: session.deviceId, relativeTime: true, threadId: session.threadId, allowTrim: session.allowTrim };
        if (startTimestamp !== undefined && endTimestamp !== undefined) {
            funcParam.startTimestamp = startTimestamp;
            funcParam.endTimestamp = endTimestamp;
        }
        const funcData = await getFuncData(funcParam);

        runInAction(() => {
            session.funcData = funcData;
            session.funcOptions = [...new Set(funcData.traces.map(trace => trace.func))].map(func => ({ label: func, value: func }));
            const funcSet = new Set(session.searchFunc);
            session.searchFunc = funcSet.size ? session.funcOptions.filter((item: any) => funcSet.has(item.value)).map((i: any) => i.value) : [];
            if (startTimestamp !== undefined && endTimestamp !== undefined) {
                session.maxTime = endTimestamp;
                session.minTime = startTimestamp;
            } else {
                session.maxTime = funcData.maxTimestamp;
                session.minTime = funcData.minTimestamp;
            }
            session.maxDepth = funcData.maxDepth;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};
export const getBarNewData = async (session: any, startTimestamp?: number, endTimestamp?: number): Promise<void> => {
    const getBlocksRequest = session.module === 'leaks' ? getBlocksGraphData : getSnapshotBlocks;
    const getAllocationRequest = session.module === 'leaks' ? getLeaksAllocationsData : getSnapshotAllocations;
    runInAction(() => {
        session.loadingBlocks = true;
    });
    try {
        const param: BlockParam = { deviceId: session.deviceId, relativeTime: true, eventType: session.eventType, isTable: false };
        const blockData = await getBlocksRequest(param);
        const transform = { x: 0, y: 0, scale: 1 };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        workerSetMemoryBlockData({ data: blockData });
        const allocationData = await getAllocationRequest(param);
        runInAction(() => {
            session.allocationData = allocationData;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};
export const getNewDetailData = async (session: any): Promise<void> => {
    try {
        const memoryData = await getMemoryDetailData(session.deviceId, session.memoryStamp, session.eventType);
        runInAction(() => {
            session.memoryData = memoryData;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};
const handleThreshold = (blockParam: any, session: any): void => {
    const { lazyUsedThreshold, delayedFreeThreshold, longIdleThreshold } = session;
    if (lazyUsedThreshold.valueT === null && lazyUsedThreshold.perT === null && delayedFreeThreshold.valueT === null &&
        delayedFreeThreshold.perT === null && longIdleThreshold.valueT === null && longIdleThreshold.perT === null
    ) return;
    const threshold: { [key: string]: ThreShold } = {};
    if (lazyUsedThreshold.valueT !== null || lazyUsedThreshold.perT !== null) {
        blockParam.lazyUsedThreshold = { perT: null, valueT: null };
        threshold.lazyUsedThreshold = lazyUsedThreshold;
    }
    if (delayedFreeThreshold.valueT !== null || delayedFreeThreshold.perT !== null) {
        blockParam.delayedFreeThreshold = { perT: null, valueT: null };
        threshold.delayedFreeThreshold = delayedFreeThreshold;
    }
    if (longIdleThreshold.valueT !== null || longIdleThreshold.perT !== null) {
        blockParam.longIdleThreshold = { perT: null, valueT: null };
        threshold.longIdleThreshold = longIdleThreshold;
    }
    Object.keys(threshold).forEach((key) => {
        if (threshold[key].valueT !== null) {
            blockParam[key].valueT = threshold[key].valueT;
        } else {
            blockParam[key].valueT = 0;
        }
        if (threshold[key].perT !== null) {
            blockParam[key].perT = threshold[key].perT;
        } else {
            blockParam[key].perT = 0;
        }
    });
};
export const getBlockTableData = async (session: any): Promise<void> => {
    const request = session.module === 'leaks' ? getBlockDetails : getSnapshotBlockTable;
    try {
        const blockParam: BlockParam = {
            deviceId: session.deviceId,
            relativeTime: true,
            eventType: session.eventType,
            isTable: true,
            startTimestamp: session.minTime,
            endTimestamp: session.maxTime,
            currentPage: session.blocksCurrentPage,
            pageSize: session.blocksPageSize,
        };
        if (session.blocksOrder !== '') {
            blockParam.desc = session.blocksOrder;
            blockParam.orderBy = session.blocksOrderBy;
        }
        if (Object.keys(session.blocksFilters).length > 0) {
            blockParam.filters = session.blocksFilters;
        }
        if (Object.keys(session.blocksRangeFilters).length > 0) {
            blockParam.rangeFilters = session.blocksRangeFilters;
        }
        if (session.onlyInefficient) {
            blockParam.onlyInefficient = true;
        }
        if (session.autoFilterPotentialLeaks) {
            blockParam.onlyUnreleasedInRange = true;
        }
        handleThreshold(blockParam, session);
        const blockTableData = await request(blockParam);
        runInAction(() => {
            session.blocksTableData = blockTableData.blocks;
            session.blocksTableHeader = blockTableData.headers;
            session.blocksTotal = blockTableData.total;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};
export const getPotentialLeakStats = async (session: any): Promise<void> => {
    if (session.module !== 'memsnapshot' || session.deviceId === '' || session.maxTime === 0 || session.maxTime === undefined) return;
    const requestId = session.leakStats.requestId + 1;
    runInAction(() => {
        session.leakStats.loading = true;
        session.leakStats.error = false;
        session.leakStats.requestId = requestId;
    });
    try {
        const leakStats = await getSnapshotLeakStats({
            deviceId: session.deviceId,
            startTimestamp: session.minTime,
            endTimestamp: session.maxTime,
        });
        runInAction(() => {
            if (session.leakStats.requestId !== requestId) return;
            session.leakStats = { ...leakStats, loading: false, error: false, requestId };
        });
    } catch (error: any) {
        runInAction(() => {
            if (session.leakStats.requestId !== requestId) return;
            session.leakStats.loading = false;
            session.leakStats.error = true;
        });
        message.error(error.message);
    }
};
export const getEventTableData = async (session: any): Promise<void> => {
    const request = session.module === 'leaks' ? getEventDetails : getSnapshotEvent;
    try {
        const eventParam: EventParam = {
            deviceId: session.deviceId,
            relativeTime: true,
            startTimestamp: session.minTime,
            endTimestamp: session.maxTime,
            currentPage: session.eventsCurrentPage,
            pageSize: session.eventsPageSize,
            isTable: true,
        };
        if (session.eventsOrder !== '') {
            eventParam.desc = session.eventsOrder;
            eventParam.orderBy = session.eventsOrderBy;
        }
        if (Object.keys(session.eventsFilters).length > 0) {
            eventParam.filters = session.eventsFilters;
        }
        if (Object.keys(session.eventsRangeFilters).length > 0) {
            eventParam.rangeFilters = session.eventsRangeFilters;
        }
        const eventTableData = await request(eventParam);
        runInAction(() => {
            session.eventsTableData = eventTableData.events;
            session.eventsTableHeader = eventTableData.headers;
            session.eventsTotal = eventTableData.total;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};

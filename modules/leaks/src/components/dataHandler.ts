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
import {
    workerLoadMemoryBlockCache,
    workerSetMemoryBlockData,
    workerSetReservedLine,
    workerTransform,
} from '@/leaksWorker/blockWorker/worker';
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
import { ensureOpfsOrWaitForFallbackApproval } from './opfsFallback';
import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from './blockLoadState';

const funcDataRequestSeqMap = new WeakMap<object, number>();
const barDataRequestSeqMap = new WeakMap<object, number>();

const createBlockPathCacheHash = (fileHash: string, deviceId: string, eventType: string): string =>
    fileHash ? `${fileHash}-${deviceId}-${eventType}` : '';

export const getFuncNewData = async (
    session: any,
    startTimestamp?: number,
    endTimestamp?: number,
    shouldApply: () => boolean = () => true,
): Promise<void> => {
    const requestSeq = (funcDataRequestSeqMap.get(session) ?? 0) + 1;
    funcDataRequestSeqMap.set(session, requestSeq);
    const isLatestRequest = (): boolean => funcDataRequestSeqMap.get(session) === requestSeq && shouldApply();

    runInAction(() => {
        session.loadingFunc = true;
    });
    try {
        const funcParam: FuncParam = { deviceId: session.deviceId, relativeTime: true, threadId: session.threadId, allowTrim: session.allowTrim };
        if (startTimestamp !== undefined && endTimestamp !== undefined) {
            funcParam.startTimestamp = startTimestamp;
            funcParam.endTimestamp = endTimestamp;
        }
        const funcData = await getFuncData(funcParam);
        if (!isLatestRequest()) {
            return;
        }

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
            session.loadingFunc = false;
        });
    } catch (error: any) {
        if (!isLatestRequest()) {
            return;
        }
        runInAction(() => {
            session.loadingFunc = false;
        });
        message.error(error.message);
    }
};
export const getBarNewData = async (session: any, startTimestamp?: number, endTimestamp?: number): Promise<void> => {
    const requestSeq = (barDataRequestSeqMap.get(session) ?? 0) + 1;
    barDataRequestSeqMap.set(session, requestSeq);
    const isLatestRequest = (): boolean => barDataRequestSeqMap.get(session) === requestSeq;
    if (session.module === 'memsnapshot') {
        await ensureOpfsOrWaitForFallbackApproval();
        if (!isLatestRequest()) {
            return;
        }
    }
    const getBlocksRequest = session.module === 'leaks' ? getBlocksGraphData : getSnapshotBlocks;
    const getAllocationRequest = session.module === 'leaks' ? getLeaksAllocationsData : getSnapshotAllocations;
    const blockContextKey = createMemoryBlockContextKey(session);
    runInAction(() => {
        session.loadingBlocks = true;
        session.loadedMemoryBlockContextKey = '';
        session.loadingOverview = true;
        session.progressiveBlocksVisible = false;
        session.progressiveRenderedBatchCount = 0;
        session.progressiveRenderedInstanceCount = 0;
        session.progressiveRenderedEventCount = 0;
        session.progressiveTotalEventCount = 0;
        session.progressiveFirstRenderedBatchCount = 0;
        session.progressiveFirstRenderedInstanceCount = 0;
    });
    delete (globalThis as {
        __LEAKS_PROGRESSIVE_RENDER_METRICS__?: ProgressiveRenderMetrics;
    }).__LEAKS_PROGRESSIVE_RENDER_METRICS__;
    let requestActive = true;
    try {
        const param: BlockParam = { deviceId: session.deviceId, relativeTime: true, eventType: session.eventType, isTable: false };
        const cacheFileHash = createBlockPathCacheHash(session.fileHash, param.deviceId, param.eventType);
        const loadAllocation = async (): Promise<void> => {
            const allocationData = await getAllocationRequest(param);
            if (!requestActive || !isLatestRequest()) {
                return;
            }
            const { reservedLine, ...allocationResult } = allocationData;
            runInAction(() => {
                session.allocationData = allocationResult;
                if (allocationResult.allocations.length === 0) {
                    session.loadingOverview = false;
                }
            });
            if (isLatestRequest() && session.module === 'memsnapshot') {
                if (reservedLine !== undefined) {
                    workerSetReservedLine({ reservedLine });
                }
            }
        };
        const transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        const cacheHit = session.module === 'memsnapshot'
            ? await workerLoadMemoryBlockCache({ fileHash: cacheFileHash })
            : false;
        if (!isLatestRequest()) {
            return;
        }
        if (cacheHit) {
            await loadAllocation();
            if (isLatestRequest()) {
                runInAction(() => {
                    session.loadingBlocks = false;
                    session.loadedMemoryBlockContextKey = blockContextKey;
                });
            }
            return;
        }
        const blockData = await getBlocksRequest(param);
        if (!isLatestRequest()) {
            return;
        }
        const blockRenderTask = workerSetMemoryBlockData({ data: blockData, fileHash: cacheFileHash });
        await Promise.all([blockRenderTask, loadAllocation()]);
        if (!isLatestRequest()) {
            return;
        }
        runInAction(() => {
            session.loadingBlocks = false;
            session.loadedMemoryBlockContextKey = blockContextKey;
        });
    } catch (error: any) {
        requestActive = false;
        if (isLatestRequest()) {
            runInAction(() => {
                session.loadingBlocks = false;
                session.loadingOverview = false;
                session.progressiveBlocksVisible = false;
            });
            message.error(error.message);
        }
    }
};
export const getNewDetailData = async (session: any): Promise<void> => {
    try {
        const memoryDatas = await getMemoryDetailData(session.deviceId, session.memoryStamp, session.eventType);
        runInAction(() => {
            session.memoryData = memoryDatas;
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
        const currentPage = session.blocksCurrentPage;
        const pageSize = session.blocksPageSize;
        const blockParam: BlockParam = {
            deviceId: session.deviceId,
            relativeTime: true,
            eventType: session.eventType,
            isTable: true,
            startTimestamp: session.minTime,
            endTimestamp: session.maxTime,
            currentPage,
            pageSize,
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
        const maxPage = Math.max(Math.ceil(blockTableData.total / pageSize), 1);
        if (currentPage > maxPage) {
            runInAction(() => {
                session.blocksCurrentPage = maxPage;
            });
            return;
        }
        runInAction(() => {
            session.blocksTableData = blockTableData.blocks;
            session.blocksTableHeader = blockTableData.headers;
            session.blocksTotal = blockTableData.total;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};
export const getPotentialLeakStats = async (session: any, range?: [number, number]): Promise<void> => {
    const startTimestamp = range?.[0] ?? session.minTime;
    const endTimestamp = range?.[1] ?? session.maxTime;
    if (session.module !== 'memsnapshot' || session.deviceId === '' || endTimestamp === 0 || endTimestamp === undefined) return;
    const deviceId = session.deviceId;
    const requestId = session.leakStats.requestId + 1;
    const isLatestRequest = (): boolean => (
        session.module === 'memsnapshot' &&
        session.deviceId === deviceId &&
        session.minTime === startTimestamp &&
        session.maxTime === endTimestamp &&
        session.leakStats.requestId === requestId
    );
    runInAction(() => {
        session.leakStats.loading = true;
        session.leakStats.error = false;
        session.leakStats.requestId = requestId;
    });
    try {
        const leakStats = await getSnapshotLeakStats({
            deviceId,
            startTimestamp,
            endTimestamp,
        });
        runInAction(() => {
            if (!isLatestRequest()) return;
            session.leakStats = { ...leakStats, loading: false, error: false, requestId };
        });
    } catch (error: any) {
        runInAction(() => {
            if (!isLatestRequest()) return;
            session.leakStats.loading = false;
            session.leakStats.error = true;
        });
        message.error(error.message);
    }
};
export const getEventTableData = async (session: any): Promise<void> => {
    if (!isMemoryBlockLoadReady(session)) {
        return;
    }
    const request = session.module === 'leaks' ? getEventDetails : getSnapshotEvent;
    try {
        const currentPage = session.eventsCurrentPage;
        const pageSize = session.eventsPageSize;
        const eventParam: EventParam = {
            deviceId: session.deviceId,
            relativeTime: true,
            startTimestamp: session.minTime,
            endTimestamp: session.maxTime,
            currentPage,
            pageSize,
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
        const maxPage = Math.max(Math.ceil(eventTableData.total / pageSize), 1);
        if (currentPage > maxPage) {
            runInAction(() => {
                session.eventsCurrentPage = maxPage;
            });
            return;
        }
        runInAction(() => {
            session.eventsTableData = eventTableData.events;
            session.eventsTableHeader = eventTableData.headers;
            session.eventsTotal = eventTableData.total;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};

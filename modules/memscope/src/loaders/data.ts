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
import type { MemScopeContext } from '../state/MemScopeContext';
import { getBlockRenderer } from '../common/leaksWorker/blockWorker/worker';
import { getMemoryDetailData, getFuncData, getBlockDetails, getEventDetails, getBlocksGraphData, getLeaksAllocationsData } from '../api/requests';
import { FuncParam } from '../api/types';
import { type AllocationData, type BlockParam, EventParam, ThreShold } from '../common/shared/api/types';
import { message } from 'antd';
import { runInAction } from 'mobx';

import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from '../common/components/blockLoadState';

const funcDataRequestSeqMap = new WeakMap<object, number>();

const captureRequestContext = (session: MemScopeContext): (() => boolean) => {
    const contextKey = createMemoryBlockContextKey(session);
    const { deviceIds } = session;
    return () => createMemoryBlockContextKey(session) === contextKey && session.deviceIds === deviceIds;
};

const barDataRequestSeqMap = new WeakMap<object, number>();

const blockTableRequestSeqMap = new WeakMap<object, number>();

const eventTableRequestSeqMap = new WeakMap<object, number>();

export const getFuncNewData = async (session: MemScopeContext, startTimestamp?: number, endTimestamp?: number, shouldApply: () => boolean = () => true): Promise<void> => {
    if (session.module !== 'leaks') return;
    const requestSeq = (funcDataRequestSeqMap.get(session) ?? 0) + 1;
    funcDataRequestSeqMap.set(session, requestSeq);
    if (session.deviceId === '') { return; }
    const isCurrentContext = captureRequestContext(session);
    const { threadId } = session;
    const isLatestRequest = (): boolean => funcDataRequestSeqMap.get(session) === requestSeq &&
        isCurrentContext() && session.threadId === threadId && shouldApply();
    runInAction(() => {
        session.loadingFunc = true;
    });
    try {
        const funcParam: FuncParam = { deviceId: session.deviceId, relativeTime: true, threadId: Number(session.threadId), allowTrim: session.allowTrim };
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

export const getBarNewData = async (session: MemScopeContext): Promise<void> => {
    const { setAllocationLines: workerSetAllocationLines, transform: workerTransform, setMemoryBlockData: workerSetMemoryBlockData } = getBlockRenderer(session);

    if (session.module !== 'leaks') return;
    const requestSeq = (barDataRequestSeqMap.get(session) ?? 0) + 1;
    barDataRequestSeqMap.set(session, requestSeq);
    blockTableRequestSeqMap.set(session, (blockTableRequestSeqMap.get(session) ?? 0) + 1);
    eventTableRequestSeqMap.set(session, (eventTableRequestSeqMap.get(session) ?? 0) + 1);
    if (session.deviceId === '') { return; }
    const isCurrentContext = captureRequestContext(session);
    const isLatestRequest = (): boolean => barDataRequestSeqMap.get(session) === requestSeq && isCurrentContext();

    ;
    const getBlocksRequest = getBlocksGraphData;
    const blockContextKey = createMemoryBlockContextKey(session);
    runInAction(() => {
        session.loadingBlocks = true;
        session.loadedMemoryBlockContextKey = '';
        session.loadingOverview = true;
        session.allocationData = {
            ...session.allocationData,
            allocationLineAvailability: undefined,
        };
        session.progressiveBlocksVisible = false;
        session.progressiveRenderedBatchCount = 0;
        session.progressiveRenderedInstanceCount = 0;
        session.progressiveRenderedEventCount = 0;
        session.progressiveTotalEventCount = 0;
        session.progressiveFirstRenderedBatchCount = 0;
        session.progressiveFirstRenderedInstanceCount = 0;
        ;
    });
    delete (globalThis as {
        __LEAKS_PROGRESSIVE_RENDER_METRICS__?: ProgressiveRenderMetrics;
    }).__LEAKS_PROGRESSIVE_RENDER_METRICS__;
    let requestActive = true;
    try {
        const param: BlockParam = {
            deviceId: session.deviceId,
            relativeTime: true,
            eventType: session.eventType,
            isTable: false,
            sliceIndex: undefined,
        };
        const cacheFileHash = '';
        // memscope 未拆 allocationLines 接口，折线仍在 Memory/leaks/allocations 中。
        // worker 按 generation 接受折线；必须在 cache hit / setMemoryBlockData 抬升 generation 之后再下发。
        const publishAllocationLines = (lines: Parameters<typeof workerSetAllocationLines>[0] | undefined): void => {
            if (!requestActive || !isLatestRequest() || lines === undefined) {
                return;
            }
            workerSetAllocationLines(lines);
        };
        const loadAllocation = async (): Promise<Parameters<typeof workerSetAllocationLines>[0] | undefined> => {
            const allocationData: AllocationData = await getLeaksAllocationsData(param);

            if (!requestActive || !isLatestRequest()) {
                return undefined;
            }
            ;
            const { reservedLine, processUsedLine, deviceUsedLine, ...allocationResult } = allocationData;
            runInAction(() => {
                session.allocationData = {
                    ...allocationResult,
                    allocationLineAvailability: {
                        reservedLine: (reservedLine?.length ?? 0) > 0,
                        processUsedLine: (processUsedLine?.length ?? 0) > 0,
                        deviceUsedLine: (deviceUsedLine?.length ?? 0) > 0,
                    },
                };
                if (allocationResult.allocations.length === 0) {
                    session.loadingOverview = false;
                }
                ;
            });
            return { reservedLine, processUsedLine, deviceUsedLine };
        };

        const transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        const allocationTask = loadAllocation();
        void allocationTask.catch(() => undefined);
        const blockData = await getBlocksRequest(param); // leaks（Memory/leaks/blocks）保持单次请求
        if (!isLatestRequest()) {
            return;
        }
        const blockRenderTask = workerSetMemoryBlockData({ data: blockData, fileHash: cacheFileHash });
        publishAllocationLines(await allocationTask);
        await blockRenderTask;
        if (!isLatestRequest()) {
            return;
        }
        runInAction(() => {
            session.loadingBlocks = false;
            session.loadedMemoryBlockContextKey = blockContextKey;
        });
    } catch (error: any) {
        // 分片拉取被新请求取代：静默作废，loading 等状态由新请求管理

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

export const getNewDetailData = async (session: MemScopeContext): Promise<void> => {
    if (session.module !== 'leaks') return;
    if (session.deviceId === '') { return; }
    try {
        const memoryDatas = await getMemoryDetailData(session.deviceId, session.memoryStamp, session.eventType);
        runInAction(() => {
            session.memoryData = memoryDatas;
        });
    } catch (error: any) {
        message.error(error.message);
    }
};

const handleThreshold = (blockParam: any, session: MemScopeContext): void => {
    const { lazyUsedThreshold, delayedFreeThreshold, longIdleThreshold } = session;
    if (lazyUsedThreshold.valueT === null && lazyUsedThreshold.perT === null && delayedFreeThreshold.valueT === null &&
        delayedFreeThreshold.perT === null && longIdleThreshold.valueT === null && longIdleThreshold.perT === null) { return; }
    const threshold: {
        [key: string]: ThreShold;
    } = {};
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

export const getBlockTableData = async (session: MemScopeContext): Promise<void> => {
    if (session.module !== 'leaks') return;
    if (session.deviceId === '') { return; }

    const requestSeq = (blockTableRequestSeqMap.get(session) ?? 0) + 1;
    blockTableRequestSeqMap.set(session, requestSeq);
    const isCurrentContext = captureRequestContext(session);
    const isLatestRequest = (): boolean => blockTableRequestSeqMap.get(session) === requestSeq && isCurrentContext();
    const request = getBlockDetails;
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
            sliceIndex: undefined,
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

        handleThreshold(blockParam, session);
        const blockTableData = await request(blockParam);
        if (!isLatestRequest()) { return; }
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
        if (isLatestRequest()) { message.error(error.message); }
    }
};

export const getEventTableData = async (session: MemScopeContext): Promise<void> => {
    if (session.module !== 'leaks') return;
    if (session.deviceId === '') { return; }
    if (!isMemoryBlockLoadReady(session)) {
        runInAction(() => {
            session.eventsTableData = [];
            session.eventsTotal = 0;
        });
        return;
    }
    const requestSeq = (eventTableRequestSeqMap.get(session) ?? 0) + 1;
    eventTableRequestSeqMap.set(session, requestSeq);
    const isCurrentContext = captureRequestContext(session);
    const isLatestRequest = (): boolean => eventTableRequestSeqMap.get(session) === requestSeq && isCurrentContext();
    const request = getEventDetails;
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
            sliceIndex: undefined,
        };
        if (session.eventsOrder !== '') {
            eventParam.desc = session.eventsOrder === true;
            eventParam.orderBy = session.eventsOrderBy;
        }
        if (Object.keys(session.eventsFilters).length > 0) {
            eventParam.filters = session.eventsFilters;
        }
        if (Object.keys(session.eventsRangeFilters).length > 0) {
            eventParam.rangeFilters = session.eventsRangeFilters;
        }
        const eventTableData = await request(eventParam);
        if (!isLatestRequest()) { return; }
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
        if (isLatestRequest()) { message.error(error.message); }
    }
};

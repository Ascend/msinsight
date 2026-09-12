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
    workerSetAllocationLines,
    workerTransform,
    workerDestroy,
} from '@/leaksWorker/blockWorker/worker';
import {
    getMemoryDetailData, getFuncData, getBlockDetails, getEventDetails,
    FuncParam, type AllocationData, type AllocationLineData, type BlockParam, EventParam, type GraphParam,
    ThreShold,
    getBlocksGraphData,
    getLeaksAllocationsData,
    getSnapshotBlocks,
    getSnapshotBlockTable,
    getSnapshotEvent,
    getSnapshotAllocations,
    getSnapshotAllocationLines,
    getSnapshotLeakStats,
} from '../utils/RequestUtils';
import { message } from 'antd';
import { runInAction } from 'mobx';
import { ensureOpfsFallbackApproval, ensureOpfsOrWaitForFallbackApproval } from './opfsFallback';
import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from './blockLoadState';
import { isMemSnapshotSliceReadyForQuery } from '../utils/memSnapshotSlices';

const funcDataRequestSeqMap = new WeakMap<object, number>();

const getAllocationDataMaxSize = (data: AllocationData): number => {
    let maxSize = data.maxSize ?? 0;
    data.allocations.forEach(item => { maxSize = Math.max(maxSize, item.totalSize); });
    data.reservedLine?.forEach(item => { maxSize = Math.max(maxSize, item.reservedSize); });
    data.processUsedLine?.forEach(item => { maxSize = Math.max(maxSize, item.processUsed); });
    data.deviceUsedLine?.forEach(item => { maxSize = Math.max(maxSize, item.deviceUsed); });
    return maxSize;
};

const updateSnapshotGlobalMaxSize = (
    session: any,
    deviceId: string,
    eventType: string,
    maxSize: number,
): void => {
    const globalMaxSizes = session.snapshotGlobalMaxSizes ?? {};
    const currentMaxSize = globalMaxSizes[deviceId]?.[eventType] ?? 0;
    const nextMaxSize = Math.max(currentMaxSize, maxSize);
    if (nextMaxSize === currentMaxSize) {
        return;
    }
    runInAction(() => {
        session.snapshotGlobalMaxSizes = {
            ...globalMaxSizes,
            [deviceId]: {
                ...(globalMaxSizes[deviceId] ?? {}),
                [eventType]: nextMaxSize,
            },
        };
    });
};
const barDataRequestSeqMap = new WeakMap<object, number>();
const blockTableRequestSeqMap = new WeakMap<object, number>();
const eventTableRequestSeqMap = new WeakMap<object, number>();
const overviewPrefetchMap = new WeakMap<object, Map<string, Promise<AllocationData | undefined>>>();
const OVERVIEW_PRELOAD_CONCURRENCY = 2;
let overviewPreloadActiveCount = 0;
const overviewPreloadWaiters: Array<() => void> = [];

const acquireOverviewPreloadSlot = async (): Promise<void> => {
    if (overviewPreloadActiveCount < OVERVIEW_PRELOAD_CONCURRENCY) {
        overviewPreloadActiveCount += 1;
        return;
    }
    await new Promise<void>(resolve => {
        overviewPreloadWaiters.push(resolve);
    });
    overviewPreloadActiveCount += 1;
};

const releaseOverviewPreloadSlot = (): void => {
    overviewPreloadActiveCount = Math.max(0, overviewPreloadActiveCount - 1);
    const next = overviewPreloadWaiters.shift();
    if (next !== undefined) {
        next();
    }
};

const createBlockPathCacheHash = (fileHash: string, deviceId: string, eventType: string, sliceIndex: number): string =>
    fileHash ? `${fileHash}-${deviceId}-${eventType}-${sliceIndex}` : '';

const getSnapshotSliceRequestOrder = (readySlices: number[], selectedSliceIndex: number): number[] => {
    const descendingSlices = [...readySlices].sort((left, right) => right - left);
    const preferredSlice = readySlices.includes(selectedSliceIndex) ? selectedSliceIndex : descendingSlices[0];
    if (preferredSlice === undefined) {
        return [];
    }
    return [preferredSlice, ...descendingSlices.filter(sliceIndex => sliceIndex !== preferredSlice)];
};

const clearMemSnapshotWindowData = (session: any): void => {
    session.blocksTableData = [];
    session.blocksTotal = 0;
    session.eventsTableData = [];
    session.eventsTotal = 0;
    session.leakStats = {
        totalSize: 0,
        maxSize: 0,
        minSize: 0,
        loading: false,
        error: false,
        requestId: (session.leakStats?.requestId ?? 0) + 1,
    };
};

export const getFuncNewData = async (
    session: any,
    startTimestamp?: number,
    endTimestamp?: number,
    shouldApply: () => boolean = () => true,
): Promise<void> => {
    if (!isMemSnapshotSliceReadyForQuery(session)) {
        return;
    }
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
// 1000 起步、2 倍翻倍；blocks 单项比 events 更宽，上限 32000，为代理 10 MiB 限制保留余量
const BLOCKS_MIN_PAGE_SIZE = 1000;
const BLOCKS_MAX_PAGE_SIZE = 32000;

// blocks 分片拉取中途被新请求取代时抛出的哨兵：catch 中按对象身份识别（throw 与 catch 之间无包装），
// 静默 return（状态由新请求管理）
const SNAPSHOT_PAGINATION_SUPERSEDED = new Error('snapshot pagination superseded by a newer request');

type SnapshotBlocksPage = RenderData & { total?: number };

/**
 * 图视图 blocks 分片拉取（issue #499）：单条全量响应可达数十 MB，超过 JupyterLab 代理 10MB 上限导致断连。
 * 翻页策略：前两页 1000 条，此后页大小翻倍至 32000 上限，再按页码递增；
 * 以响应 total（COUNT 总数）为终止条件，各页 blocks 顺序拼接，等价于原全量数据。
 */
const fetchSnapshotViewBlocksPaginated = async (
    baseParam: BlockParam,
    isLatestRequest: () => boolean,
): Promise<RenderData> => {
    let currentDataCount = 0;
    let currentPage = 1;
    let pageSize = BLOCKS_MIN_PAGE_SIZE;
    let merged: SnapshotBlocksPage | null = null;
    let expectedTotal: number | undefined;
    for (;;) {
        const res: SnapshotBlocksPage = await getSnapshotBlocks({ ...baseParam, currentPage, pageSize });
        if (!isLatestRequest()) {
            throw SNAPSHOT_PAGINATION_SUPERSEDED;
        }
        if (merged === null) {
            // 首页：spread 后清空 blocks 作为合并容器（min/max 等元信息各页相同）
            merged = { ...res, blocks: [] };
            expectedTotal = res.total;
            if (expectedTotal !== undefined && (!Number.isSafeInteger(expectedTotal) || expectedTotal < 0)) {
                throw new Error('Invalid blocks pagination total');
            }
        } else if (res.total !== expectedTotal) {
            throw new Error('Blocks pagination total changed while loading');
        }
        merged.blocks = merged.blocks.concat(res.blocks);
        currentDataCount += res.blocks.length;
        // 旧后端若不返回 total，则将首页视为原全量响应并保持兼容。
        if (expectedTotal === undefined) {
            break;
        }
        if (currentDataCount > expectedTotal) {
            throw new Error('Blocks pagination returned more rows than total');
        }
        if (currentDataCount === expectedTotal) {
            break;
        }
        if (res.blocks.length === 0) {
            throw new Error('Blocks pagination returned an empty page before total was reached');
        }
        if (res.blocks.length < pageSize) {
            throw new Error('Blocks pagination returned a short page before total was reached');
        }
        if (pageSize < BLOCKS_MAX_PAGE_SIZE) {
            if (currentPage === 1) {
                currentPage = 2; // 第二页仍用 1000 小页
            } else {
                pageSize = Math.min(pageSize * 2, BLOCKS_MAX_PAGE_SIZE); // 停留在第二页持续翻倍页大小
            }
        } else {
            currentPage++;
        }
    }
    return merged as RenderData;
};

const ALLOCATIONS_PAGE_SIZE = 30000;

type SnapshotAllocationPage = AllocationData;

const validateAllocationTotal = (total: SnapshotAllocationPage['total']): void => {
    if (total === undefined || total === null || !Number.isSafeInteger(total.allocations) || total.allocations < 0) {
        throw new Error('Invalid allocations pagination total');
    }
};

const getExpectedPageLength = (total: number, currentPage: number): number => {
    const offset = (currentPage - 1) * ALLOCATIONS_PAGE_SIZE;
    return Math.min(ALLOCATIONS_PAGE_SIZE, Math.max(0, total - offset));
};

const fetchSnapshotAllocationsPaginated = async (
    baseParam: GraphParam,
    isLatestRequest: () => boolean,
): Promise<AllocationData> => {
    let currentPage = 1;
    let firstPage: SnapshotAllocationPage | undefined;
    let expectedTotal: NonNullable<SnapshotAllocationPage['total']> | undefined;
    const allocationChunks: Array<AllocationData['allocations']> = [];
    for (;;) {
        const page = await getSnapshotAllocations({ ...baseParam, currentPage, pageSize: ALLOCATIONS_PAGE_SIZE });
        if (!isLatestRequest()) {
            throw SNAPSHOT_PAGINATION_SUPERSEDED;
        }
        if (firstPage === undefined) {
            firstPage = page;
            if (page.total === undefined) {
                return page; // 旧后端忽略分页参数并返回历史全量响应。
            }
            validateAllocationTotal(page.total);
            expectedTotal = page.total;
        } else if (page.total?.allocations !== expectedTotal?.allocations ||
            page.minTimestamp !== firstPage.minTimestamp || page.maxTimestamp !== firstPage.maxTimestamp) {
            throw new Error('Allocations pagination metadata changed while loading');
        }
        if (!Array.isArray(page.allocations) || expectedTotal === undefined ||
            page.allocations.length !== getExpectedPageLength(expectedTotal.allocations, currentPage)) {
            throw new Error('Allocations pagination returned an unexpected page length');
        }
        allocationChunks.push(page.allocations);
        if (currentPage * ALLOCATIONS_PAGE_SIZE >= expectedTotal.allocations) {
            break;
        }
        currentPage++;
    }
    const merged: SnapshotAllocationPage = {
        ...firstPage,
        allocations: allocationChunks.flat(),
    };
    delete merged.total;
    return merged;
};

type SnapshotAllocationLinePage = AllocationLineData;

const fetchSnapshotAllocationLinesPaginated = async (
    baseParam: GraphParam,
    isLatestRequest: () => boolean,
): Promise<AllocationLineData> => {
    let currentPage = 1;
    let firstPage: SnapshotAllocationLinePage | undefined;
    let expectedTotal = 0;
    const reservedLineChunks: Array<NonNullable<AllocationLineData['reservedLine']>> = [];
    for (;;) {
        const page = await getSnapshotAllocationLines({ ...baseParam, currentPage, pageSize: ALLOCATIONS_PAGE_SIZE });
        if (!isLatestRequest()) {
            throw SNAPSHOT_PAGINATION_SUPERSEDED;
        }
        if (firstPage === undefined) {
            firstPage = page;
            if (page.total === undefined) {
                return page;
            }
            if (!Number.isSafeInteger(page.total.reservedLine) || page.total.reservedLine < 0) {
                throw new Error('Invalid allocation lines pagination total');
            }
            expectedTotal = page.total.reservedLine;
        } else if (page.total?.reservedLine !== expectedTotal || page.minTimestamp !== firstPage.minTimestamp ||
            page.maxTimestamp !== firstPage.maxTimestamp) {
            throw new Error('Allocation lines pagination metadata changed while loading');
        }
        const reservedLine = page.reservedLine ?? [];
        if (!Array.isArray(reservedLine) ||
            reservedLine.length !== getExpectedPageLength(expectedTotal, currentPage)) {
            throw new Error('Allocation lines pagination returned an unexpected page length');
        }
        reservedLineChunks.push(reservedLine);
        if (currentPage * ALLOCATIONS_PAGE_SIZE >= expectedTotal) {
            break;
        }
        currentPage++;
    }
    const merged: SnapshotAllocationLinePage = {
        ...firstPage,
        reservedLine: reservedLineChunks.flat(),
    };
    delete merged.total;
    return merged;
};

export const getBarNewData = async (session: any): Promise<void> => {
    const requestSeq = (barDataRequestSeqMap.get(session) ?? 0) + 1;
    barDataRequestSeqMap.set(session, requestSeq);
    blockTableRequestSeqMap.set(session, (blockTableRequestSeqMap.get(session) ?? 0) + 1);
    eventTableRequestSeqMap.set(session, (eventTableRequestSeqMap.get(session) ?? 0) + 1);
    const isLatestRequest = (): boolean => barDataRequestSeqMap.get(session) === requestSeq;
    if (!isMemSnapshotSliceReadyForQuery(session)) {
        workerDestroy();
        runInAction(() => {
            session.blockData = { blocks: [], minSize: 0, maxSize: 0, minTimestamp: 0, maxTimestamp: 0 };
            session.allocationData = { allocations: [], minTimestamp: 0, maxTimestamp: 0 };
            session.loadingBlocks = false;
            session.loadingOverview = false;
            session.loadedMemoryBlockContextKey = '';
            clearMemSnapshotWindowData(session);
        });
        return;
    }
    if (session.module === 'memsnapshot') {
        await ensureOpfsOrWaitForFallbackApproval();
        if (!isLatestRequest()) {
            return;
        }
    }
    const getBlocksRequest = session.module === 'leaks' ? getBlocksGraphData : getSnapshotBlocks;
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
        if (session.module === 'memsnapshot') {
            clearMemSnapshotWindowData(session);
        }
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
            sliceIndex: session.module === 'memsnapshot' ? session.selectedSliceIndex : undefined,
        };
        const cacheFileHash = createBlockPathCacheHash(
            session.snapshotParsingComplete !== false && !session.memSnapshotCacheRefreshPending ? session.fileHash : '',
            param.deviceId,
            param.eventType,
            session.selectedSliceIndex,
        );
        // memscope 未拆 allocationLines 接口，折线仍在 Memory/leaks/allocations 中。
        // worker 按 generation 接受折线；必须在 cache hit / setMemoryBlockData 抬升 generation 之后再下发。
        const publishAllocationLines = (
            lines: Parameters<typeof workerSetAllocationLines>[0] | undefined,
        ): void => {
            if (!requestActive || !isLatestRequest() || lines === undefined) {
                return;
            }
            workerSetAllocationLines(lines);
        };
        const loadAllocation = async (): Promise<Parameters<typeof workerSetAllocationLines>[0] | undefined> => {
            let allocationData: AllocationData;
            if (session.module === 'memsnapshot') {
                const cachedOverview = session.sliceOverviewData[param.deviceId]?.[param.sliceIndex ?? -1];
                const [overview, lines] = await Promise.all([
                    cachedOverview ?? fetchSnapshotAllocationsPaginated(param, isLatestRequest),
                    fetchSnapshotAllocationLinesPaginated(param, isLatestRequest),
                ]);
                allocationData = { ...overview, ...lines, allocations: overview.allocations };
            } else {
                allocationData = await getLeaksAllocationsData(param);
            }
            if (!requestActive || !isLatestRequest()) {
                return undefined;
            }
            if (session.module === 'memsnapshot') {
                updateSnapshotGlobalMaxSize(
                    session,
                    param.deviceId,
                    param.eventType,
                    getAllocationDataMaxSize(allocationData),
                );
            }
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
                if (session.module === 'memsnapshot') {
                    session.sliceOverviewData = {
                        ...session.sliceOverviewData,
                        [session.deviceId]: {
                            ...(session.sliceOverviewData[session.deviceId] ?? {}),
                            [session.selectedSliceIndex]: allocationResult,
                        },
                    };
                }
            });
            return { reservedLine, processUsedLine, deviceUsedLine };
        };
        const preloadRemainingOverviews = (): void => {
            if (session.module === 'memsnapshot' && session.snapshotParsingComplete !== false && isLatestRequest()) {
                void preloadSnapshotSliceOverviews(session);
            }
        };
        const transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        runInAction(() => {
            session.leaksWorkerInfo.renderOptions.transform = transform;
        });
        workerTransform({ transform });
        const cacheStatus: BlockPathCacheLoadStatus = session.module === 'memsnapshot'
            ? await workerLoadMemoryBlockCache({ fileHash: cacheFileHash })
            : 'miss';
        if (!isLatestRequest()) {
            return;
        }
        if (cacheStatus === 'unavailable') {
            await ensureOpfsFallbackApproval();
            if (!isLatestRequest()) {
                return;
            }
        }
        if (cacheStatus === 'hit') {
            runInAction(() => {
                session.loadingBlocks = false;
                session.loadedMemoryBlockContextKey = blockContextKey;
            });
            publishAllocationLines(await loadAllocation());
            preloadRemainingOverviews();
            return;
        }
        const allocationTask = loadAllocation();
        void allocationTask.catch(() => undefined);
        const blockData = session.module === 'memsnapshot'
            ? await fetchSnapshotViewBlocksPaginated(param, isLatestRequest)
            : await getBlocksRequest(param); // leaks（Memory/leaks/blocks）保持单次请求
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
        preloadRemainingOverviews();
    } catch (error: any) {
        // 分片拉取被新请求取代：静默作废，loading 等状态由新请求管理
        if (error === SNAPSHOT_PAGINATION_SUPERSEDED) {
            return;
        }
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

export const preloadSnapshotSliceOverviews = async (
    session: any,
    includeSelected: boolean = false,
): Promise<AllocationData | undefined> => {
    if (session.module !== 'memsnapshot' || session.deviceId === '') {
        return;
    }
    const deviceId = session.deviceId;
    const eventType = session.eventType;
    const fileHash = session.fileHash;
    const deviceSlices = session.snapshotSlices[deviceId];
    if (!deviceSlices) {
        return;
    }
    let pending = overviewPrefetchMap.get(session);
    if (pending === undefined) {
        pending = new Map<string, Promise<AllocationData | undefined>>();
        overviewPrefetchMap.set(session, pending);
    }
    const requestOrder = getSnapshotSliceRequestOrder(deviceSlices.readySlices, session.selectedSliceIndex);
    const currentSliceIndex = session.selectedSliceIndex >= 0 ? session.selectedSliceIndex : requestOrder[0];
    const existingTasks: Array<Promise<{ sliceIndex: number; allocationData?: AllocationData }>> = [];
    const pendingSlices: number[] = [];
    for (const sliceIndex of requestOrder) {
        if (!includeSelected && (sliceIndex === currentSliceIndex || sliceIndex === session.selectedSliceIndex)) {
            continue;
        }
        const key = `${fileHash}:${deviceId}:${eventType}:${sliceIndex}`;
        const hasCurrentGlobalMax = session.snapshotGlobalMaxSizes?.[deviceId]?.[eventType] !== undefined;
        if (hasCurrentGlobalMax && session.sliceOverviewData[deviceId]?.[sliceIndex] !== undefined) {
            continue;
        }
        const existingTask = pending?.get(key);
        if (existingTask !== undefined) {
            existingTasks.push(existingTask.then(allocationData => ({ sliceIndex, allocationData })));
            continue;
        }
        pendingSlices.push(sliceIndex);
    }
    const fetchedTasks = pendingSlices.map(sliceIndex => {
        const key = `${fileHash}:${deviceId}:${eventType}:${sliceIndex}`;
        const task = (async (): Promise<AllocationData | undefined> => {
            await acquireOverviewPreloadSlot();
            try {
                const allocationData = await fetchSnapshotAllocationsPaginated(
                    { deviceId, eventType, sliceIndex },
                    () => session.fileHash === fileHash && session.deviceIds?.[deviceId] !== undefined &&
                        session.eventType === eventType,
                );
                if (session.fileHash !== fileHash || session.deviceIds?.[deviceId] === undefined ||
                    session.eventType !== eventType) {
                    return undefined;
                }
                return allocationData;
            } catch {
                // 单个分片趋势预加载失败不影响当前窗口，后续选中该分片时仍会正常请求。
                return undefined;
            } finally {
                pending?.delete(key);
                releaseOverviewPreloadSlot();
            }
        })();
        pending?.set(key, task);
        return task.then(allocationData => ({ sliceIndex, allocationData }));
    });
    const results = [...await Promise.all(existingTasks), ...await Promise.all(fetchedTasks)];
    if (session.fileHash !== fileHash || session.deviceIds?.[deviceId] === undefined || session.eventType !== eventType) {
        return undefined;
    }
    const fetchedOverviews: Record<number, AllocationData> = {};
    for (const result of results) {
        if (result.allocationData !== undefined) {
            fetchedOverviews[result.sliceIndex] = result.allocationData;
        }
    }
    if (Object.keys(fetchedOverviews).length > 0) {
        runInAction(() => {
            session.sliceOverviewData = {
                ...session.sliceOverviewData,
                [deviceId]: {
                    ...(session.sliceOverviewData[deviceId] ?? {}),
                    ...fetchedOverviews,
                },
            };
        });
    }
    let selectedAllocationData: AllocationData | undefined;
    let prefetchedMaxSize = session.snapshotGlobalMaxSizes?.[deviceId]?.[eventType] ?? 0;
    for (const sliceIndex of requestOrder) {
        const cachedData = session.sliceOverviewData[deviceId]?.[sliceIndex];
        if (cachedData === undefined) {
            continue;
        }
        prefetchedMaxSize = Math.max(prefetchedMaxSize, getAllocationDataMaxSize(cachedData));
        if (sliceIndex === session.selectedSliceIndex) {
            selectedAllocationData = cachedData;
        }
    }
    updateSnapshotGlobalMaxSize(session, deviceId, eventType, prefetchedMaxSize);
    return selectedAllocationData;
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
    if (!isMemSnapshotSliceReadyForQuery(session)) {
        return;
    }
    const requestSeq = (blockTableRequestSeqMap.get(session) ?? 0) + 1;
    blockTableRequestSeqMap.set(session, requestSeq);
    const isLatestRequest = (): boolean => blockTableRequestSeqMap.get(session) === requestSeq;
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
            sliceIndex: session.module === 'memsnapshot' ? session.selectedSliceIndex : undefined,
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
        if (!isLatestRequest()) return;
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
        if (isLatestRequest()) message.error(error.message);
    }
};
export const getPotentialLeakStats = async (session: any, range?: [number, number]): Promise<void> => {
    const startTimestamp = range?.[0] ?? session.minTime;
    const endTimestamp = range?.[1] ?? session.maxTime;
    if (session.module !== 'memsnapshot' || session.deviceId === '' || endTimestamp === 0 ||
        endTimestamp === undefined || !isMemSnapshotSliceReadyForQuery(session)) {
        return;
    }
    const deviceId = session.deviceId;
    const sliceIndex = session.selectedSliceIndex;
    const requestId = session.leakStats.requestId + 1;
    const isLatestRequest = (): boolean => (
        session.module === 'memsnapshot' &&
        session.deviceId === deviceId &&
        session.selectedSliceIndex === sliceIndex &&
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
            sliceIndex,
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
        runInAction(() => {
            session.eventsTableData = [];
            session.eventsTotal = 0;
        });
        return;
    }
    const requestSeq = (eventTableRequestSeqMap.get(session) ?? 0) + 1;
    eventTableRequestSeqMap.set(session, requestSeq);
    const isLatestRequest = (): boolean => eventTableRequestSeqMap.get(session) === requestSeq;
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
            sliceIndex: session.module === 'memsnapshot' ? session.selectedSliceIndex : undefined,
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
        if (!isLatestRequest()) return;
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
        if (isLatestRequest()) message.error(error.message);
    }
};

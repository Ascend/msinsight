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

import { WebGLRenderer } from './webgl/WebGLRenderer';
import {
    BlockPathBuildCancelledError,
    BlockPathBuildProgress,
    buildBlockViewPath,
    buildBlockViewPathAndWriteToOPFS,
    getInitialBlockGraphMetadata,
    getZoom,
    processReservedLine,
    searchBlockDataByPoint,
    searchBlockDataByPointFromOPFS,
} from '../tools/dataProcess';
import { debounce } from 'lodash';
import { getCenteredBlockTransform } from '../tools/blockTransform';
import { BlockDataOPFS } from '../tools/BlockDataOPFS';
import {
    checkLeaksOpfsSyncAvailability,
    createLeaksOpfsRuntimeId,
    isLeaksOpfsEnabled,
} from '../tools/opfsConfig';
import { isPackedRenderData, unpackRenderData } from '../tools/packedBlockData';

let canvas: OffscreenCanvas;
let useOpfs = false;
let memoryBlockData: RenderData | undefined;
let memoryBlockMetadata: BlockGraphMetadata | undefined;
let blockDataOPFS: BlockDataOPFS;
let reservedLine: Array<[number, number]> = [];
let transform: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
let viewport: RenderOptions['viewport'];
let zoom: RenderOptions['zoom'];
let renderer: WebGLRenderer | null;
let hoverItem: Block | null = null;
let clickItem: Block | null = null;
let latestDataGeneration = 0;
let activeDataGeneration = 0;
let storageReadyGeneration = 0;
let dataLoadQueue: Promise<void> = Promise.resolve();
let hoverSearchVersion = 0;
const opfsRuntimeId = createLeaksOpfsRuntimeId();
const temporaryBlockDataStorageKey = `main-${opfsRuntimeId}`;
let resolveInitialization: () => void = () => undefined;
let initializationError: Error | undefined;
// 首次加载数据前必须等待渲染器和 OPFS 初始化完成，避免异步初始化期间访问未赋值的存储实例。
const initializationTask = new Promise<void>(resolve => {
    resolveInitialization = resolve;
});
const waitForInitialization = async (): Promise<void> => {
    await initializationTask;
    if (initializationError !== undefined) {
        throw initializationError;
    }
};
let reservedLineOverride: {
    generation: number;
    reservedLine: Array<[number, number]>;
    reservedSizeMax: number;
} | null = null;

const PROGRESSIVE_RENDER_FRAME_BUDGET_MS = 8;
const PROGRESSIVE_FRAME_TIMEOUT_MS = 40;

interface ProgressiveRenderState {
    enabled: boolean;
    framePublished: boolean;
    sliceStartedAt: number;
}

const getNow = (): number => typeof performance === 'undefined' ? Date.now() : performance.now();
const waitForProgressiveFrame = async (): Promise<void> => {
    if (typeof requestAnimationFrame !== 'function') {
        await new Promise<void>(resolve => setTimeout(resolve, 0));
        return;
    }
    await new Promise<void>(resolve => {
        let settled = false;
        const finish = (): void => {
            if (settled) {
                return;
            }
            settled = true;
            resolve();
        };
        const timeoutId = setTimeout(finish, PROGRESSIVE_FRAME_TIMEOUT_MS);
        requestAnimationFrame(() => {
            clearTimeout(timeoutId);
            finish();
        });
    });
};

const applyReservedLineOverride = (metadata: BlockGraphMetadata, generation: number): BlockGraphMetadata => {
    if (reservedLineOverride?.generation !== generation) {
        return metadata;
    }
    return {
        ...metadata,
        reservedLine: reservedLineOverride.reservedLine,
        reservedSizeMax: reservedLineOverride.reservedSizeMax,
    };
};

const isSamePathFragment = (first: Block | null, second: Block | null): boolean => {
    if (first === null || second === null) {
        return first === second;
    }
    const firstStart = first.path[0];
    const secondStart = second.path[0];
    const firstEnd = first.path[first.path.length - 1];
    const secondEnd = second.path[second.path.length - 1];
    return first.id === second.id && first.path.length === second.path.length &&
        firstStart?.[0] === secondStart?.[0] && firstStart?.[1] === secondStart?.[1] &&
        firstEnd?.[0] === secondEnd?.[0] && firstEnd?.[1] === secondEnd?.[1];
};

const getSizeInfo = (): { maxTimestamp: number; minTimestamp: number; maxSize: number; minSize: number } => {
    if (useOpfs && memoryBlockMetadata) {
        const maxSize = Math.max(memoryBlockMetadata.maxSize, memoryBlockMetadata.reservedSizeMax ?? memoryBlockMetadata.maxSize);
        return {
            maxTimestamp: memoryBlockMetadata.maxTimestamp,
            minTimestamp: memoryBlockMetadata.minTimestamp,
            maxSize,
            minSize: memoryBlockMetadata.minSize,
        };
    }
    if (memoryBlockData) {
        const maxSize = Math.max(memoryBlockData.maxSize, memoryBlockData.reservedSizeMax ?? memoryBlockData.maxSize);
        return {
            maxTimestamp: memoryBlockData.maxTimestamp,
            minTimestamp: memoryBlockData.minTimestamp,
            maxSize,
            minSize: memoryBlockData.minSize,
        };
    }
    return { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0 };
};

const resolveHitBlock = async (payload: Omit<HoverItemPayload, 'type'>): Promise<Block | null> => {
    if (useOpfs && memoryBlockMetadata?.batchCount && blockDataOPFS) {
        return searchBlockDataByPointFromOPFS(blockDataOPFS, payload, transform, zoom);
    }
    if (memoryBlockData?.blocks?.length) {
        return searchBlockDataByPoint(memoryBlockData, payload, transform, zoom);
    }
    return null;
};

const initCanvasHandler = async (payload: InitCanvasPayload): Promise<void> => {
    try {
        canvas = payload.canvas as OffscreenCanvas;
        renderer = new WebGLRenderer(canvas, payload.devicePixelRatio, opfsRuntimeId);
        viewport = { width: payload.width, height: payload.height };
        await renderer.initialize();
        blockDataOPFS = new BlockDataOPFS(temporaryBlockDataStorageKey);
        if (isLeaksOpfsEnabled()) {
            await blockDataOPFS.init();
        }
        resolveInitialization();
    } catch (error) {
        initializationError = error instanceof Error ? error : new Error(String(error));
        resolveInitialization();
        throw initializationError;
    }
};

const selectBlockDataStorage = async (
    fileHash: unknown,
): Promise<{ fileHash: string; available: boolean }> => {
    const result = await BlockDataOPFS.prepareStorage({
        fileHash,
        temporaryStorageKey: temporaryBlockDataStorageKey,
        blockDataOPFS,
        beforeStorageChange: async () => {
            await renderer?.setDataFromOPFS(null, 0, [], false);
        },
    });
    blockDataOPFS = result.blockDataOPFS;
    return { fileHash: result.fileHash, available: result.available };
};

const resetMemoryBlockDataState = (generation: number): void => {
    activeDataGeneration = generation;
    storageReadyGeneration = 0;
    hoverSearchVersion++;
    debouncedSearchBlockData.cancel();
    hoverItem = null;
    clickItem = null;
};

const updateMetadataView = (metadata: BlockGraphMetadata, generation: number): void => {
    memoryBlockMetadata = applyReservedLineOverride(metadata, generation);
    reservedLine = memoryBlockMetadata.reservedLine ?? [];
    zoom = getZoom(memoryBlockMetadata, canvas);
    self.postMessage({ type: 'dataInfo', sizeInfo: getSizeInfo(), zoom, generation });
    renderer?.setZoom(zoom, false);
};

const initializeProgressiveStorage = async (generation: number): Promise<void> => {
    storageReadyGeneration = generation;
    await renderer?.beginProgressiveDataFromOPFS(blockDataOPFS, reservedLine);
};

const renderProgressiveBatch = async (
    generation: number,
    startBatch: number,
    endBatch: number,
    progress: BlockPathBuildProgress,
    state: ProgressiveRenderState,
    shouldCancel: () => boolean,
): Promise<void> => {
    if (shouldCancel()) {
        return;
    }
    if (memoryBlockMetadata) {
        memoryBlockMetadata.batchCount = endBatch;
    }
    if (!state.enabled) {
        return;
    }
    const renderedInstanceCount = renderer?.appendDataFromOPFS(startBatch, endBatch) ?? 0;
    self.postMessage({
        type: 'progressiveRenderProgress',
        generation,
        batchCount: endBatch,
        renderedInstanceCount,
        ...progress,
    });
    if (!state.framePublished && renderedInstanceCount > 0) {
        await waitForProgressiveFrame();
        if (shouldCancel()) {
            return;
        }
        state.framePublished = true;
        self.postMessage({
            type: 'progressiveRenderStarted',
            generation,
            batchCount: endBatch,
            renderedInstanceCount,
            ...progress,
        });
        await waitForProgressiveFrame();
        state.sliceStartedAt = getNow();
        return;
    }
    if (getNow() - state.sliceStartedAt >= PROGRESSIVE_RENDER_FRAME_BUDGET_MS) {
        await waitForProgressiveFrame();
        state.sliceStartedAt = getNow();
    }
};

const setMemoryBlockDataFromOPFS = async (
    payload: SetMemoryBlocksDataPayload,
    shouldCancel: () => boolean,
): Promise<boolean> => {
    useOpfs = true;
    memoryBlockData = undefined;
    const storage = await selectBlockDataStorage(payload.fileHash);
    if (!storage.available) {
        useOpfs = false;
        return false;
    }
    const { fileHash } = storage;
    memoryBlockMetadata = getInitialBlockGraphMetadata(payload.data);
    reservedLine = memoryBlockMetadata.reservedLine ?? [];
    const cachedMetadata = await blockDataOPFS.loadCompleteCacheForBuild(fileHash);
    if (cachedMetadata) {
        if (shouldCancel()) {
            throw new BlockPathBuildCancelledError();
        }
        storageReadyGeneration = payload.generation;
        updateMetadataView(cachedMetadata, payload.generation);
        await renderer?.setDataFromOPFS(
            blockDataOPFS,
            cachedMetadata.batchCount,
            reservedLine,
            false,
        );
        return true;
    }
    const progressiveState: ProgressiveRenderState = {
        enabled: false,
        framePublished: false,
        sliceStartedAt: getNow(),
    };
    const builtMetadata = await buildBlockViewPathAndWriteToOPFS(payload.data, blockDataOPFS, {
        generation: payload.generation,
        shouldCancel,
        onGraphMetadataReady: metadata => {
            updateMetadataView(metadata, payload.generation);
            progressiveState.enabled = Number.isFinite(zoom.x) && Number.isFinite(zoom.y);
        },
        onStorageReady: async () => {
            await blockDataOPFS.tryMarkCacheBuilding(fileHash);
            await initializeProgressiveStorage(payload.generation);
        },
        onBatchesCommitted: (startBatch, endBatch, progress) =>
            renderProgressiveBatch(
                payload.generation,
                startBatch,
                endBatch,
                progress,
                progressiveState,
                shouldCancel,
            ),
    });
    memoryBlockMetadata = applyReservedLineOverride(builtMetadata, payload.generation);
    if (shouldCancel()) {
        throw new BlockPathBuildCancelledError();
    }
    await blockDataOPFS.trySaveCompleteCache(fileHash, builtMetadata);
    updateMetadataView(memoryBlockMetadata, payload.generation);
    await renderer?.setDataFromOPFS(blockDataOPFS, memoryBlockMetadata.batchCount, reservedLine, false);
    return true;
};

const setMemoryBlockDataInMemory = async (payload: SetMemoryBlocksDataPayload): Promise<void> => {
    useOpfs = false;
    memoryBlockMetadata = undefined;
    const renderData = isPackedRenderData(payload.data) ? unpackRenderData(payload.data) : payload.data;
    memoryBlockData = buildBlockViewPath(renderData);
    reservedLine = memoryBlockData.reservedLine ?? [];
    zoom = getZoom(memoryBlockData, canvas);
    self.postMessage({ type: 'dataInfo', sizeInfo: getSizeInfo(), zoom, generation: payload.generation });
    renderer?.setZoom(zoom, false);
    await renderer?.setData(memoryBlockData.blocks, reservedLine);
};

const completeMemoryBlockDataRender = async (
    payload: Pick<SetMemoryBlocksDataPayload, 'generation'>,
): Promise<void> => {
    await renderHighlightData(false);
    renderer?.updateCanvasSize(viewport);
    renderer?.renderFrame();
    if (memoryBlockMetadata?.metrics) {
        self.postMessage({ type: 'memoryMetrics', metrics: memoryBlockMetadata.metrics, generation: payload.generation });
    }
    self.postMessage({
        type: 'renderCompleted',
        generation: payload.generation,
        batchCount: memoryBlockMetadata?.batchCount ?? 0,
    });
};

const loadMemoryBlockCacheHandler = async (
    payload: LoadMemoryBlockCachePayload,
    shouldCancel: () => boolean,
): Promise<boolean> => {
    await waitForInitialization();
    if (!isLeaksOpfsEnabled()) {
        return false;
    }
    const storage = await selectBlockDataStorage(payload.fileHash);
    if (!storage.available || !storage.fileHash) {
        return false;
    }
    const { fileHash } = storage;
    const cachedMetadata = await blockDataOPFS.loadCompleteCache(fileHash);
    if (!cachedMetadata) {
        return false;
    }
    if (shouldCancel()) {
        throw new BlockPathBuildCancelledError();
    }
    resetMemoryBlockDataState(payload.generation);
    useOpfs = true;
    memoryBlockData = undefined;
    storageReadyGeneration = payload.generation;
    updateMetadataView(cachedMetadata, payload.generation);
    await renderer?.setDataFromOPFS(blockDataOPFS, cachedMetadata.batchCount, reservedLine, false);
    if (shouldCancel()) {
        throw new BlockPathBuildCancelledError();
    }
    await completeMemoryBlockDataRender(payload);
    return true;
};

const setMemoryBlockDataHandler = async (
    payload: SetMemoryBlocksDataPayload,
    shouldCancel: () => boolean,
): Promise<void> => {
    await waitForInitialization();
    resetMemoryBlockDataState(payload.generation);
    let renderedFromOpfs = false;
    if (isLeaksOpfsEnabled()) {
        try {
            renderedFromOpfs = await setMemoryBlockDataFromOPFS(payload, shouldCancel);
        } catch (error) {
            if (error instanceof BlockPathBuildCancelledError || isLeaksOpfsEnabled()) {
                throw error;
            }
        }
    }
    if (!renderedFromOpfs) {
        if (shouldCancel()) {
            throw new BlockPathBuildCancelledError();
        }
        await setMemoryBlockDataInMemory(payload);
    }
    await completeMemoryBlockDataRender(payload);
};

const setReservedLineHandler = (payload: SetReservedLinePayload): void => {
    if (payload.generation !== latestDataGeneration) {
        return;
    }
    const { reservedLine: processedReservedLine, reservedSizeMax } = processReservedLine(payload.reservedLine);
    reservedLineOverride = {
        generation: payload.generation,
        reservedLine: processedReservedLine,
        reservedSizeMax,
    };
    if (payload.generation !== activeDataGeneration) {
        return;
    }
    reservedLine = processedReservedLine;
    if (memoryBlockMetadata) {
        memoryBlockMetadata.reservedLine = processedReservedLine;
        memoryBlockMetadata.reservedSizeMax = reservedSizeMax;
        zoom = getZoom(memoryBlockMetadata, canvas);
    } else if (memoryBlockData) {
        memoryBlockData.reservedLine = processedReservedLine;
        memoryBlockData.reservedSizeMax = reservedSizeMax;
        zoom = getZoom(memoryBlockData, canvas);
    } else {
        return;
    }
    if (!Number.isFinite(zoom.x) || !Number.isFinite(zoom.y)) {
        return;
    }
    renderer?.setReservedLine(reservedLine).setZoom(
        zoom,
        !useOpfs || storageReadyGeneration === payload.generation,
    );
    self.postMessage({ type: 'dataInfo', sizeInfo: getSizeInfo(), zoom, generation: payload.generation });
};

const resizeCanvasHandler = (payload: ResizeCanvasPayload): void => {
    viewport = { width: payload.width, height: payload.height };
    renderer?.updateCanvasSize(viewport);
    if (!useOpfs && memoryBlockData === undefined) {
        return;
    }
    if (useOpfs && memoryBlockMetadata === undefined) {
        return;
    }
    const zoomSource = useOpfs ? memoryBlockMetadata : memoryBlockData;
    if (!zoomSource) {
        return;
    }
    zoom = getZoom(zoomSource, canvas);
    self.postMessage({ type: 'dataInfo', sizeInfo: getSizeInfo(), zoom });
    renderer?.setZoom(zoom);
};

const transformHandler = (payload: TransformPayload): void => {
    transform = payload.transform;
    renderer?.setTransform(transform);
};

const debouncedSearchBlockData = debounce(async (
    payload: HoverItemPayload,
    requestVersion: number,
): Promise<void> => {
    const nextHoverItem = await resolveHitBlock(payload);
    if (requestVersion !== hoverSearchVersion) {
        return;
    }
    if (isSamePathFragment(hoverItem, nextHoverItem)) {
        return;
    }
    hoverItem = nextHoverItem;
    await renderHighlightData();
    self.postMessage({ type: 'hoverItemResult', result: hoverItem });
}, 10);

const clickItemHandler = async (payload: ClickItemPayload): Promise<void> => {
    clickItem = await resolveHitBlock(payload);
    await renderHighlightData();
    self.postMessage({ type: 'clickItemResult', result: clickItem, selectionVersion: payload.selectionVersion });
};

const selectItemHandler = async (payload: SelectBlockItemPayload): Promise<void> => {
    clickItem = payload.item;
    await renderHighlightData();
    self.postMessage({ type: 'clickItemResult', result: clickItem, selectionVersion: payload.selectionVersion });
};

const selectBlockByIdHandler = async (payload: SelectBlockByIdPayload): Promise<void> => {
    if (useOpfs && blockDataOPFS) {
        clickItem = await blockDataOPFS.findBlockById(payload.blockId);
    } else {
        clickItem = memoryBlockData?.blocks?.find(block => block.id === payload.blockId) ?? null;
    }
    const centeredTransform = clickItem === null || viewport === undefined || zoom === undefined
        ? null
        : getCenteredBlockTransform(clickItem, viewport, zoom);
    if (centeredTransform !== null) {
        transform = centeredTransform;
        renderer?.setTransform(transform);
        self.postMessage({ type: 'transformResult', transform });
    }
    await renderHighlightData();
    self.postMessage({ type: 'clickItemResult', result: clickItem, selectionVersion: payload.selectionVersion });
};

const hoverItemHandler = (payload: HoverItemPayload): void => {
    const requestVersion = ++hoverSearchVersion;
    debouncedSearchBlockData(payload, requestVersion);
};

const renderHighlightData = async (render: boolean = true): Promise<void> => {
    const result: Block[] = [];
    if (clickItem !== null) {
        result.push(clickItem);
    }
    if (hoverItem !== null && hoverItem.id !== clickItem?.id) {
        result.push(hoverItem);
    }
    renderer?.setBaseDimmed(clickItem !== null, false);
    await renderer?.setHighlightData(result, render);
};

const destroyHandler = async (): Promise<void> => {
    hoverSearchVersion++;
    debouncedSearchBlockData.cancel();
    useOpfs = false;
    memoryBlockData = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, blocks: [] };
    memoryBlockMetadata = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, batchCount: 0 };
    reservedLine = [];
    transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
    zoom = { x: 1, y: 1, offset: 0 };
    hoverItem = null;
    clickItem = null;
    self.postMessage({
        type: 'dataInfo',
        sizeInfo: { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0 },
        zoom,
    });
    self.postMessage({ type: 'clickItemResult', result: null });
    await renderHighlightData(false);
    await renderer?.setDataFromOPFS(null, 0, [], false);
    if (isLeaksOpfsEnabled()) {
        await blockDataOPFS.release(temporaryBlockDataStorageKey);
    }
    renderer?.setTransform(transform).setZoom(zoom, false).updateCanvasSize(viewport);
};

const Handlers: PayloadHandlers = {
    initCanvas: initCanvasHandler,
    setReservedLine: setReservedLineHandler,
    resizeCanvas: resizeCanvasHandler,
    transform: transformHandler,
    hoverItem: hoverItemHandler,
    clickItem: clickItemHandler,
    selectBlockItem: selectItemHandler,
    selectBlockById: selectBlockByIdHandler,
    destroy: destroyHandler,
};

self.onmessage = (ev: MessageEvent<Payload>): void => {
    const payload = ev.data;
    if (payload.type === 'checkOpfsAvailability') {
        checkLeaksOpfsSyncAvailability().then(available => {
            self.postMessage({ type: 'opfsAvailabilityChecked', requestId: payload.requestId, available });
        });
        return;
    }
    if (payload.type === 'loadMemoryBlockCache') {
        latestDataGeneration = payload.generation;
        if (reservedLineOverride?.generation !== payload.generation) {
            reservedLineOverride = null;
        }
        const generation = payload.generation;
        dataLoadQueue = dataLoadQueue.catch(() => undefined).then(async () => {
            if (generation !== latestDataGeneration) {
                self.postMessage({ type: 'renderCancelled', generation });
                return;
            }
            try {
                const hit = await loadMemoryBlockCacheHandler(payload, () => generation !== latestDataGeneration);
                self.postMessage({
                    type: 'blockPathCacheLoadCompleted',
                    generation,
                    hit,
                });
            } catch (error) {
                if (error instanceof BlockPathBuildCancelledError || generation !== latestDataGeneration) {
                    self.postMessage({ type: 'renderCancelled', generation });
                    return;
                }
                // eslint-disable-next-line no-console
                console.error('Block worker cache load error:', error);
                self.postMessage({ type: 'renderFailed', generation, error: String(error) });
            }
        });
        return;
    }
    if (payload.type === 'setMemoryBlockData') {
        latestDataGeneration = payload.generation;
        if (reservedLineOverride?.generation !== payload.generation) {
            reservedLineOverride = null;
        }
        const generation = payload.generation;
        dataLoadQueue = dataLoadQueue.catch(() => undefined).then(async () => {
            if (generation !== latestDataGeneration) {
                self.postMessage({ type: 'renderCancelled', generation });
                return;
            }
            try {
                await setMemoryBlockDataHandler(payload, () => generation !== latestDataGeneration);
            } catch (error) {
                if (error instanceof BlockPathBuildCancelledError || generation !== latestDataGeneration) {
                    self.postMessage({ type: 'renderCancelled', generation });
                    return;
                }
                // eslint-disable-next-line no-console
                console.error('Block worker data load error:', error);
                self.postMessage({ type: 'renderFailed', generation, error: String(error) });
            }
        });
        return;
    }
    if (payload.type === 'destroy') {
        latestDataGeneration = payload.generation;
        dataLoadQueue = dataLoadQueue.catch(() => undefined).then(destroyHandler);
        return;
    }
    const handler = Handlers[payload.type];
    if (typeof handler === 'function') {
        Promise.resolve(handler(payload as never)).catch(error => {
            // eslint-disable-next-line no-console
            console.error('Block worker handler error:', error);
        });
    }
};

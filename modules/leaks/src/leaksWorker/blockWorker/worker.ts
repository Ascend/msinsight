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
import { isWebGL2Supported } from '../tools/detection';
import { checkLeaksOpfsAvailability } from '../tools/opfsConfig';
import { getPackedRenderDataTransferList, packRenderData } from '../tools/packedBlockData';
import { MainThreadRender } from './mainThreadindex';

declare const module: {
    hot?: {
        dispose: (callback: () => void) => void;
    };
};

export const BlockWorker = new Worker(new URL('./', import.meta.url));

let workerGeneration = 0;
const pendingWorkerLoads = new Map<number, { resolve: () => void; reject: (error: Error) => void }>();
const pendingCacheLoads = new Map<number, { resolve: (hit: boolean) => void; reject: (error: Error) => void }>();
const pendingOpfsProbes = new Map<number, (available: boolean) => void>();
let opfsProbeRequestId = 0;

export const isCurrentBlockWorkerGeneration = (generation: number | undefined): boolean =>
    generation === undefined || generation === workerGeneration;

module.hot?.dispose(() => {
    BlockWorker.terminate();
    for (const pending of pendingWorkerLoads.values()) {
        pending.reject(new Error('Block worker replaced by hot reload'));
    }
    pendingWorkerLoads.clear();
    for (const pending of pendingCacheLoads.values()) {
        pending.reject(new Error('Block worker replaced by hot reload'));
    }
    pendingCacheLoads.clear();
    for (const resolve of pendingOpfsProbes.values()) {
        resolve(false);
    }
    pendingOpfsProbes.clear();
});

BlockWorker.addEventListener('message', (event: MessageEvent<{
    type?: string;
    generation?: number;
    error?: string;
    hit?: boolean;
    requestId?: number;
    available?: boolean;
}>): void => {
    if (event.data.type === 'opfsAvailabilityChecked' && event.data.requestId !== undefined) {
        pendingOpfsProbes.get(event.data.requestId)?.(event.data.available === true);
        pendingOpfsProbes.delete(event.data.requestId);
        return;
    }
    const generation = event.data.generation;
    if (generation === undefined) {
        return;
    }
    const pendingCache = pendingCacheLoads.get(generation);
    if (event.data.type === 'blockPathCacheLoadCompleted' && pendingCache) {
        pendingCache.resolve(event.data.hit === true);
        pendingCacheLoads.delete(generation);
        return;
    }
    if (event.data.type === 'renderCancelled' && pendingCache) {
        pendingCache.resolve(false);
        pendingCacheLoads.delete(generation);
        return;
    }
    if (event.data.type === 'renderFailed' && pendingCache) {
        pendingCache.reject(new Error(event.data.error ?? 'Block worker cache load failed'));
        pendingCacheLoads.delete(generation);
        return;
    }
    const pending = pendingWorkerLoads.get(generation);
    if (!pending) {
        return;
    }
    if (event.data.type === 'renderCompleted' || event.data.type === 'renderCancelled') {
        pending.resolve();
        pendingWorkerLoads.delete(generation);
    } else if (event.data.type === 'renderFailed') {
        pending.reject(new Error(event.data.error ?? 'Block worker render failed'));
        pendingWorkerLoads.delete(generation);
    }
});

// Worker 实现
const WorkerBackend = {
    checkOpfsAvailability(): Promise<boolean> {
        const requestId = ++opfsProbeRequestId;
        const result = new Promise<boolean>(resolve => {
            pendingOpfsProbes.set(requestId, resolve);
        });
        BlockWorker.postMessage({ type: 'checkOpfsAvailability', requestId } as CheckOpfsAvailabilityPayload);
        return result;
    },
    initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
        const devicePixelRatio = window.devicePixelRatio || 1;
        const offscreenCanvas = (canvas as HTMLCanvasElement).transferControlToOffscreen();
        BlockWorker.postMessage(
            { type: 'initCanvas', canvas: offscreenCanvas, devicePixelRatio, width, height },
            [offscreenCanvas],
        );
    },
    loadMemoryBlockCache({ fileHash }: { fileHash?: string }): Promise<boolean> {
        const generation = ++workerGeneration;
        const completion = new Promise<boolean>((resolve, reject) => {
            pendingCacheLoads.set(generation, { resolve, reject });
        });
        BlockWorker.postMessage({
            type: 'loadMemoryBlockCache',
            generation,
            fileHash,
        } as LoadMemoryBlockCachePayload);
        return completion;
    },
    setMemoryBlockData({ data, fileHash }: { data: RenderData; fileHash?: string }): Promise<void> {
        const generation = ++workerGeneration;
        const packedData = packRenderData(data);
        data.blocks = [];
        const completion = new Promise<void>((resolve, reject) => {
            pendingWorkerLoads.set(generation, { resolve, reject });
        });
        BlockWorker.postMessage(
            { type: 'setMemoryBlockData', generation, data: packedData, fileHash } as SetMemoryBlocksDataPayload,
            getPackedRenderDataTransferList(packedData),
        );
        return completion;
    },
    setAllocationLines(lines: Omit<SetAllocationLinesPayload, 'type' | 'generation'>): void {
        BlockWorker.postMessage({
            type: 'setAllocationLines',
            generation: workerGeneration,
            ...lines,
        } as SetAllocationLinesPayload);
    },
    resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'resizeCanvas', width, height });
    },
    transform({ transform }: Omit<TransformPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'transform', transform });
    },
    setBlockGraphLayerVisibility({ visibility }: Omit<SetBlockGraphLayerVisibilityPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'setBlockGraphLayerVisibility', visibility });
    },
    setMarkerHoverHighlight({ active, blockId, blockIds }: Omit<SetMarkerHoverHighlightPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'setMarkerHoverHighlight', active, blockId, blockIds });
    },
    hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'hoverItem', clientX, clientY });
    },
    clickItem({ clientX, clientY, selectionVersion }: Omit<ClickItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'clickItem', clientX, clientY, selectionVersion });
    },
    selectItem({ item, selectionVersion }: Omit<SelectBlockItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'selectBlockItem', item, selectionVersion });
    },
    selectBlockById({ blockId, selectionVersion }: Omit<SelectBlockByIdPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'selectBlockById', blockId, selectionVersion });
    },
    destroy(): void {
        const generation = ++workerGeneration;
        BlockWorker.postMessage({ type: 'destroy', generation } as DestroyPayload);
    },
};

// 主线程实现（fallback）
const mainThreadRender = new MainThreadRender();
let mainThreadGeneration = 0;
let mainThreadLoadQueue: Promise<void> = Promise.resolve();
const MainThreadBackend = {
    checkOpfsAvailability: checkLeaksOpfsAvailability,
    async initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): Promise<void> {
        const devicePixelRatio = window.devicePixelRatio || 1;
        await mainThreadRender.initCanvasHandler({ canvas, devicePixelRatio, width, height });
    },
    loadMemoryBlockCache(): Promise<boolean> {
        return Promise.resolve(false);
    },
    setMemoryBlockData({ data, fileHash }: { data: RenderData; fileHash?: string }): Promise<void> {
        const generation = ++mainThreadGeneration;
        const packedData = packRenderData(data);
        data.blocks = [];
        const task = mainThreadLoadQueue.then(async () => {
            if (generation !== mainThreadGeneration) {
                return;
            }
            await mainThreadRender.setMemoryBlockDataHandler(
                { generation, data: packedData, fileHash },
                () => generation !== mainThreadGeneration,
            );
        });
        mainThreadLoadQueue = task.catch(() => undefined);
        return task;
    },
    setAllocationLines(lines: Omit<SetAllocationLinesPayload, 'type' | 'generation'>): void {
        mainThreadRender.setAllocationLinesHandler({
            generation: mainThreadGeneration,
            ...lines,
        });
    },
    resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
        mainThreadRender.resizeCanvasHandler({ width, height });
    },
    transform({ transform }: Omit<TransformPayload, 'type'>): void {
        mainThreadRender.transformHandler({ transform });
    },
    setBlockGraphLayerVisibility({ visibility }: Omit<SetBlockGraphLayerVisibilityPayload, 'type'>): void {
        mainThreadRender.setBlockGraphLayerVisibilityHandler({ visibility });
    },
    async setMarkerHoverHighlight(
        { active, blockId, blockIds }: Omit<SetMarkerHoverHighlightPayload, 'type'>,
    ): Promise<void> {
        await mainThreadRender.setMarkerHoverHighlightHandler({ active, blockId, blockIds });
    },
    hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
        mainThreadRender.hoverItemHandler({ clientX, clientY });
    },
    async clickItem({ clientX, clientY, selectionVersion }: Omit<ClickItemPayload, 'type'>): Promise<void> {
        await mainThreadRender.clickItemHandler({ clientX, clientY, selectionVersion });
    },
    async selectItem({ item, selectionVersion }: Omit<SelectBlockItemPayload, 'type'>): Promise<void> {
        await mainThreadRender.selectItemHandler({ item, selectionVersion });
    },
    async selectBlockById({ blockId, selectionVersion }: Omit<SelectBlockByIdPayload, 'type'>): Promise<void> {
        await mainThreadRender.selectBlockByIdHandler({ blockId, selectionVersion });
    },
    async destroy(): Promise<void> {
        mainThreadGeneration++;
        const task = mainThreadLoadQueue.then(async () => {
            await mainThreadRender.destroyHandler();
        });
        mainThreadLoadQueue = task.catch(() => undefined);
        await task;
    },
};

// 自动选择后端
const backend = isWebGL2Supported() ? WorkerBackend : MainThreadBackend;

// 导出统一接口
export const workerInitCanvas = backend.initCanvas;
export const workerCheckOpfsAvailability = backend.checkOpfsAvailability;
export const workerLoadMemoryBlockCache = backend.loadMemoryBlockCache;
export const workerSetMemoryBlockData = backend.setMemoryBlockData;
export const workerSetAllocationLines = backend.setAllocationLines;
export const workerResizeCanvas = backend.resizeCanvas;
export const workerTransform = backend.transform;
export const workerSetBlockGraphLayerVisibility = backend.setBlockGraphLayerVisibility;
export const workerSetMarkerHoverHighlight = backend.setMarkerHoverHighlight;
export const workerHoverItem = backend.hoverItem;
export const workerClickItem = backend.clickItem;
export const workerSelectItem = backend.selectItem;
export const workerSelectBlockById = backend.selectBlockById;
export const workerDestroy = backend.destroy;

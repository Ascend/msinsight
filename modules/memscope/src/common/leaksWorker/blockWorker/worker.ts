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
import type { MemoryContext } from '../../shared/state/MemoryContext';
import { BlockDataOPFS } from '../tools/BlockDataOPFS';

import { createRendererRegistry } from '../../shared/workers/RendererRegistry';
import { createNativeWorker } from './createWorker';

declare const module: {
    hot?: {
        dispose: (callback: () => void) => void;
    };
};

export interface BlockRenderer {
    dispose: () => void;
    subscribe: (listener: (event: MessageEvent<any>) => void) => () => void;
    isCurrentGeneration: (generation: number | undefined) => boolean;
    checkOpfsAvailability: () => Promise<OpfsAvailabilityStatus>;
    initCanvas: (payload: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>) => void | Promise<void>;
    loadMemoryBlockCache: (payload: { fileHash?: string }) => Promise<BlockPathCacheLoadStatus>;
    setMemoryBlockData: (payload: { data: RenderData; fileHash?: string }) => Promise<void>;
    removeMemoryBlockCaches: (payload: { fileHash: string }) => Promise<void>;
    setAllocationLines: (payload: Omit<SetAllocationLinesPayload, 'type' | 'generation'>) => void | Promise<void>;
    setBlockGraphGlobalMaxSize: (payload: Omit<SetBlockGraphGlobalMaxSizePayload, 'type'>) => void | Promise<void>;
    resizeCanvas: (payload: Omit<ResizeCanvasPayload, 'type'>) => void | Promise<void>;
    transform: (payload: Omit<TransformPayload, 'type'>) => void | Promise<void>;
    setBlockGraphLayerVisibility: (payload: Omit<SetBlockGraphLayerVisibilityPayload, 'type'>) => void | Promise<void>;
    setMarkerHoverHighlight: (payload: Omit<SetMarkerHoverHighlightPayload, 'type'>) => void | Promise<void>;
    hoverItem: (payload: Omit<HoverItemPayload, 'type'>) => void | Promise<void>;
    clickItem: (payload: Omit<ClickItemPayload, 'type'>) => void | Promise<void>;
    selectItem: (payload: Omit<SelectBlockItemPayload, 'type'>) => void | Promise<void>;
    selectBlockById: (payload: Omit<SelectBlockByIdPayload, 'type'>) => void | Promise<void>;
    destroy: () => void | Promise<void>;
}

export const createBlockRenderer = (session: MemoryContext, useWorker: boolean = isWebGL2Supported()): BlockRenderer => {
    const ownerModule = session.module;
    let disposed = false;
    const isOwnerCurrent = (): boolean => !disposed && session.module === ownerModule;
    let nativeWorker: Worker | undefined;
    let mainThreadRender: MainThreadRender | undefined;
    const listeners = new Set<(event: MessageEvent<any>) => void>();
    const transferredCanvases = new WeakSet<HTMLCanvasElement>();
    const getWorker = (): Worker => {
        if (!nativeWorker) {
            nativeWorker = createNativeWorker();
            nativeWorker.addEventListener('message', onMessage);
            nativeWorker.addEventListener('error', onError);
            nativeWorker.addEventListener('messageerror', onError);
        }
        return nativeWorker;
    };
    const getMainThreadRender = (): MainThreadRender => {
        if (!mainThreadRender) { mainThreadRender = new MainThreadRender(); mainThreadRender.session = session; mainThreadRender.isSessionCurrent = isOwnerCurrent; }
        return mainThreadRender;
    };

    let workerGeneration = 0;
    const pendingWorkerLoads = new Map<number, { resolve: () => void; reject: (error: Error) => void }>();
    const pendingCacheLoads = new Map<number, {
        resolve: (status: BlockPathCacheLoadStatus) => void;
        reject: (error: Error) => void;
    }>();
    const pendingOpfsProbes = new Map<number, (status: OpfsAvailabilityStatus) => void>();
    let opfsProbeRequestId = 0;
    const pendingCacheRemovals = new Map<number, { resolve: () => void; reject: (error: Error) => void }>();
    let cacheRemovalRequestId = 0;

    const onMessage = (event: MessageEvent<{
        type?: string;
        generation?: number;
        error?: string;
        status?: BlockPathCacheLoadStatus;
        requestId?: number;
        availabilityStatus?: OpfsAvailabilityStatus;
    }>): void => {
        if (disposed) return;
        if (event.data.type === 'blockPathCacheRemovalCompleted' && event.data.requestId !== undefined) {
            pendingCacheRemovals.get(event.data.requestId)?.resolve();
            pendingCacheRemovals.delete(event.data.requestId);
            return;
        }
        if (event.data.type === 'blockPathCacheRemovalFailed' && event.data.requestId !== undefined) {
            pendingCacheRemovals.get(event.data.requestId)?.reject(
                new Error(event.data.error ?? 'Block worker cache removal failed'),
            );
            pendingCacheRemovals.delete(event.data.requestId);
            return;
        }
        if (event.data.type === 'opfsAvailabilityChecked' && event.data.requestId !== undefined) {
            pendingOpfsProbes.get(event.data.requestId)?.(event.data.availabilityStatus ?? 'transient');
            pendingOpfsProbes.delete(event.data.requestId);
            return;
        }
        const generation = event.data.generation;
        if (generation === undefined) {
            return;
        }
        const pendingCache = pendingCacheLoads.get(generation);
        if (event.data.type === 'blockPathCacheLoadCompleted' && pendingCache) {
            pendingCache.resolve(event.data.status ?? 'miss');
            pendingCacheLoads.delete(generation);
            return;
        }
        if (event.data.type === 'renderCancelled' && pendingCache) {
            pendingCache.resolve('miss');
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
    };

    // Worker 实现
    const WorkerBackend = {
        checkOpfsAvailability(): Promise<OpfsAvailabilityStatus> {
            const requestId = ++opfsProbeRequestId;
            const result = new Promise<OpfsAvailabilityStatus>(resolve => {
                pendingOpfsProbes.set(requestId, resolve);
            });
            getWorker().postMessage({ type: 'checkOpfsAvailability', requestId } as CheckOpfsAvailabilityPayload);
            return result;
        },
        initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
            const devicePixelRatio = window.devicePixelRatio || 1;
            if (transferredCanvases.has(canvas as HTMLCanvasElement)) return;
            const offscreenCanvas = (canvas as HTMLCanvasElement).transferControlToOffscreen();
            transferredCanvases.add(canvas as HTMLCanvasElement);
            getWorker().postMessage(
                { type: 'initCanvas', canvas: offscreenCanvas, devicePixelRatio, width, height },
                [offscreenCanvas],
            );
        },
        loadMemoryBlockCache({ fileHash }: { fileHash?: string }): Promise<BlockPathCacheLoadStatus> {
            const generation = ++workerGeneration;
            const completion = new Promise<BlockPathCacheLoadStatus>((resolve, reject) => {
                pendingCacheLoads.set(generation, { resolve, reject });
            });
            getWorker().postMessage({
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
            getWorker().postMessage(
                { type: 'setMemoryBlockData', generation, data: packedData, fileHash } as SetMemoryBlocksDataPayload,
                getPackedRenderDataTransferList(packedData),
            );
            return completion;
        },
        removeMemoryBlockCaches({ fileHash }: { fileHash: string }): Promise<void> {
            const requestId = ++cacheRemovalRequestId;
            const completion = new Promise<void>((resolve, reject) => {
                pendingCacheRemovals.set(requestId, { resolve, reject });
            });
            getWorker().postMessage({ type: 'removeMemoryBlockCaches', requestId, fileHash } as RemoveMemoryBlockCachesPayload);
            return completion;
        },
        setAllocationLines(lines: Omit<SetAllocationLinesPayload, 'type' | 'generation'>): void {
            getWorker().postMessage({
                type: 'setAllocationLines',
                generation: workerGeneration,
                ...lines,
            } as SetAllocationLinesPayload);
        },
        setBlockGraphGlobalMaxSize({ maxSize }: Omit<SetBlockGraphGlobalMaxSizePayload, 'type'>): void {
            getWorker().postMessage({ type: 'setBlockGraphGlobalMaxSize', maxSize } as SetBlockGraphGlobalMaxSizePayload);
        },
        resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
            getWorker().postMessage({ type: 'resizeCanvas', width, height });
        },
        transform({ transform }: Omit<TransformPayload, 'type'>): void {
            getWorker().postMessage({ type: 'transform', transform });
        },
        setBlockGraphLayerVisibility({ visibility }: Omit<SetBlockGraphLayerVisibilityPayload, 'type'>): void {
            getWorker().postMessage({ type: 'setBlockGraphLayerVisibility', visibility });
        },
        setMarkerHoverHighlight({ active, blockId, blockIds }: Omit<SetMarkerHoverHighlightPayload, 'type'>): void {
            getWorker().postMessage({ type: 'setMarkerHoverHighlight', active, blockId, blockIds });
        },
        hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
            getWorker().postMessage({ type: 'hoverItem', clientX, clientY });
        },
        clickItem({ clientX, clientY, selectionVersion }: Omit<ClickItemPayload, 'type'>): void {
            getWorker().postMessage({ type: 'clickItem', clientX, clientY, selectionVersion });
        },
        selectItem({ item, selectionVersion }: Omit<SelectBlockItemPayload, 'type'>): void {
            getWorker().postMessage({ type: 'selectBlockItem', item, selectionVersion });
        },
        selectBlockById({ blockId, selectionVersion }: Omit<SelectBlockByIdPayload, 'type'>): void {
            getWorker().postMessage({ type: 'selectBlockById', blockId, selectionVersion });
        },
        destroy(): void {
            const generation = ++workerGeneration;
            getWorker().postMessage({ type: 'destroy', generation } as DestroyPayload);
        },
    };

    // 主线程实现（fallback）
    let mainThreadGeneration = 0;
    let mainThreadLoadQueue: Promise<void> = Promise.resolve();
    const MainThreadBackend = {
        checkOpfsAvailability: checkLeaksOpfsAvailability,
        async initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): Promise<void> {
            const devicePixelRatio = window.devicePixelRatio || 1;
            await getMainThreadRender().initCanvasHandler({ canvas, devicePixelRatio, width, height });
        },
        loadMemoryBlockCache({ fileHash }: { fileHash?: string }): Promise<BlockPathCacheLoadStatus> {
            const generation = ++mainThreadGeneration;
            const task = mainThreadLoadQueue.then(async () => {
                if (!isOwnerCurrent() || generation !== mainThreadGeneration) {
                    return 'miss' as const;
                }
                return getMainThreadRender().loadMemoryBlockCacheHandler(
                    { generation, fileHash },
                    () => !isOwnerCurrent() || generation !== mainThreadGeneration,
                );
            });
            mainThreadLoadQueue = task.then(() => undefined, () => undefined);
            return task;
        },
        setMemoryBlockData({ data, fileHash }: { data: RenderData; fileHash?: string }): Promise<void> {
            const generation = ++mainThreadGeneration;
            const packedData = packRenderData(data);
            data.blocks = [];
            const task = mainThreadLoadQueue.then(async () => {
                if (!isOwnerCurrent() || generation !== mainThreadGeneration) {
                    return;
                }
                await getMainThreadRender().setMemoryBlockDataHandler(
                    { generation, data: packedData, fileHash },
                    () => !isOwnerCurrent() || generation !== mainThreadGeneration,
                );
            });
            mainThreadLoadQueue = task.catch(() => undefined);
            return task;
        },
        async removeMemoryBlockCaches({ fileHash }: { fileHash: string }): Promise<void> {
            const task = mainThreadLoadQueue.then(async () => {
                await BlockDataOPFS.removePersistentCachesBySourceHash(fileHash);
            });
            mainThreadLoadQueue = task.catch(() => undefined);
            await task;
        },
        setAllocationLines(lines: Omit<SetAllocationLinesPayload, 'type' | 'generation'>): void {
            getMainThreadRender().setAllocationLinesHandler({
                generation: mainThreadGeneration,
                ...lines,
            });
        },
        setBlockGraphGlobalMaxSize(payload: Omit<SetBlockGraphGlobalMaxSizePayload, 'type'>): void {
            getMainThreadRender().setBlockGraphGlobalMaxSizeHandler(payload);
        },
        resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
            getMainThreadRender().resizeCanvasHandler({ width, height });
        },
        transform({ transform }: Omit<TransformPayload, 'type'>): void {
            getMainThreadRender().transformHandler({ transform });
        },
        setBlockGraphLayerVisibility({ visibility }: Omit<SetBlockGraphLayerVisibilityPayload, 'type'>): void {
            getMainThreadRender().setBlockGraphLayerVisibilityHandler({ visibility });
        },
        async setMarkerHoverHighlight(
            { active, blockId, blockIds }: Omit<SetMarkerHoverHighlightPayload, 'type'>,
        ): Promise<void> {
            await getMainThreadRender().setMarkerHoverHighlightHandler({ active, blockId, blockIds });
        },
        hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
            getMainThreadRender().hoverItemHandler({ clientX, clientY });
        },
        async clickItem({ clientX, clientY, selectionVersion }: Omit<ClickItemPayload, 'type'>): Promise<void> {
            await getMainThreadRender().clickItemHandler({ clientX, clientY, selectionVersion });
        },
        async selectItem({ item, selectionVersion }: Omit<SelectBlockItemPayload, 'type'>): Promise<void> {
            await getMainThreadRender().selectItemHandler({ item, selectionVersion });
        },
        async selectBlockById({ blockId, selectionVersion }: Omit<SelectBlockByIdPayload, 'type'>): Promise<void> {
            await getMainThreadRender().selectBlockByIdHandler({ blockId, selectionVersion });
        },
        async destroy(): Promise<void> {
            mainThreadGeneration++;
            const task = mainThreadLoadQueue.then(async () => {
                await getMainThreadRender().destroyHandler();
            });
            mainThreadLoadQueue = task.catch(() => undefined);
            await task;
        },
    };

    const backend = useWorker ? WorkerBackend : MainThreadBackend;

    const settlePending = (): void => {
        for (const pending of pendingWorkerLoads.values()) pending.resolve();
        pendingWorkerLoads.clear();
        for (const pending of pendingCacheLoads.values()) pending.resolve('miss');
        pendingCacheLoads.clear();
        for (const resolve of pendingOpfsProbes.values()) resolve('transient');
        pendingOpfsProbes.clear();
        for (const pending of pendingCacheRemovals.values()) pending.reject(new Error('Renderer disposed'));
        pendingCacheRemovals.clear();
    };
    const onError = (): void => {
        const error = new Error('Block renderer failed');
        for (const pending of pendingWorkerLoads.values()) pending.reject(error);
        for (const pending of pendingCacheLoads.values()) pending.reject(error);
        dispose();
    };

    const dispose = (): void => {
        if (disposed) return;
        disposed = true;
        workerGeneration++;
        mainThreadGeneration++;
        settlePending();
        for (const listener of listeners) nativeWorker?.removeEventListener('message', listener);
        listeners.clear();
        if (nativeWorker) {
            nativeWorker.removeEventListener('message', onMessage);
            nativeWorker.removeEventListener('error', onError);
            nativeWorker.removeEventListener('messageerror', onError);

            const retiring = nativeWorker;
            const timeout = setTimeout(() => { retiring.terminate(); }, 1000);
            retiring.addEventListener('message', event => {
                if (event.data.type === 'disposed') { clearTimeout(timeout); retiring.terminate(); }
            });
            retiring.postMessage({ type: 'dispose', generation: workerGeneration });

            nativeWorker = undefined;
        }
        const retiringMainThread = mainThreadRender;
        if (retiringMainThread) {
            retiringMainThread.session = undefined;
            void mainThreadLoadQueue.catch(() => undefined).then(() => retiringMainThread.destroyHandler()).finally(() => { mainThreadRender = undefined; }).catch(() => undefined);
        }
    };

    return {
        dispose,
        isCurrentGeneration: (generation: number | undefined): boolean => isOwnerCurrent() && (generation === undefined || generation === workerGeneration),
        subscribe: (listener: (event: MessageEvent<any>) => void): (() => void) => {
            if (!isOwnerCurrent() || !useWorker) return () => undefined;
            const currentWorker = getWorker();
            const ownedListener = (event: MessageEvent<any>): void => {
                if (isOwnerCurrent()) listener(event);
            };
            listeners.add(ownedListener);
            currentWorker.addEventListener('message', ownedListener);
            return () => { listeners.delete(ownedListener); currentWorker.removeEventListener('message', ownedListener); };
        },
        initCanvas: (payload: Parameters<typeof backend.initCanvas>[0]) => !isOwnerCurrent() ? undefined : backend.initCanvas(payload),
        checkOpfsAvailability: () => !isOwnerCurrent() ? Promise.resolve('transient' as const) : backend.checkOpfsAvailability(),
        loadMemoryBlockCache: (payload: Parameters<typeof backend.loadMemoryBlockCache>[0]) => !isOwnerCurrent() ? Promise.resolve('miss' as const) : backend.loadMemoryBlockCache(payload),
        setMemoryBlockData: (payload: Parameters<typeof backend.setMemoryBlockData>[0]) => !isOwnerCurrent() ? Promise.resolve() : backend.setMemoryBlockData(payload),
        removeMemoryBlockCaches: (payload: Parameters<typeof backend.removeMemoryBlockCaches>[0]) => !isOwnerCurrent() ? Promise.resolve() : backend.removeMemoryBlockCaches(payload),
        setAllocationLines: (payload: Parameters<typeof backend.setAllocationLines>[0]) => !isOwnerCurrent() ? undefined : backend.setAllocationLines(payload),
        setBlockGraphGlobalMaxSize: (payload: Parameters<typeof backend.setBlockGraphGlobalMaxSize>[0]) => !isOwnerCurrent() ? undefined : backend.setBlockGraphGlobalMaxSize(payload),
        resizeCanvas: (payload: Parameters<typeof backend.resizeCanvas>[0]) => !isOwnerCurrent() ? undefined : backend.resizeCanvas(payload),
        transform: (payload: Parameters<typeof backend.transform>[0]) => !isOwnerCurrent() ? undefined : backend.transform(payload),
        setBlockGraphLayerVisibility: (payload: Parameters<typeof backend.setBlockGraphLayerVisibility>[0]) => !isOwnerCurrent() ? undefined : backend.setBlockGraphLayerVisibility(payload),
        setMarkerHoverHighlight: (payload: Parameters<typeof backend.setMarkerHoverHighlight>[0]) => !isOwnerCurrent() ? undefined : backend.setMarkerHoverHighlight(payload),
        hoverItem: (payload: Parameters<typeof backend.hoverItem>[0]) => !isOwnerCurrent() ? undefined : backend.hoverItem(payload),
        clickItem: (payload: Parameters<typeof backend.clickItem>[0]) => !isOwnerCurrent() ? undefined : backend.clickItem(payload),
        selectItem: (payload: Parameters<typeof backend.selectItem>[0]) => !isOwnerCurrent() ? undefined : backend.selectItem(payload),
        selectBlockById: (payload: Parameters<typeof backend.selectBlockById>[0]) => !isOwnerCurrent() ? undefined : backend.selectBlockById(payload),
        destroy: () => !isOwnerCurrent() || (!nativeWorker && !mainThreadRender) ? undefined : backend.destroy(),
    };
};

const renderers = createRendererRegistry(createBlockRenderer);
export const getBlockRenderer = renderers.get;
module.hot?.dispose(renderers.disposeAll);
export const bindBlockWorkerSession = (session: MemoryContext | undefined): (() => void) => session ? renderers.retain(session) : () => undefined;
export const subscribeBlockWorker = (session: MemoryContext, listener: (event: MessageEvent<any>) => void): (() => void) => renderers.get(session).subscribe(listener);
export const isCurrentBlockWorkerGeneration = (generation: number | undefined, session: MemoryContext): boolean => renderers.peek(session)?.isCurrentGeneration(generation) ?? false;
export const isWorkerBlockRenderer = isWebGL2Supported();
export const workerInitCanvas = (payload: Parameters<BlockRenderer['initCanvas']>[0], session: MemoryContext): ReturnType<BlockRenderer['initCanvas']> => renderers.get(session).initCanvas(payload);
export const workerCheckOpfsAvailability = (session: MemoryContext): ReturnType<BlockRenderer['checkOpfsAvailability']> => renderers.get(session).checkOpfsAvailability();
export const workerLoadMemoryBlockCache = (payload: Parameters<BlockRenderer['loadMemoryBlockCache']>[0], session: MemoryContext): ReturnType<BlockRenderer['loadMemoryBlockCache']> => renderers.get(session).loadMemoryBlockCache(payload);
export const workerSetMemoryBlockData = (payload: Parameters<BlockRenderer['setMemoryBlockData']>[0], session: MemoryContext): ReturnType<BlockRenderer['setMemoryBlockData']> => renderers.get(session).setMemoryBlockData(payload);
export const workerRemoveMemoryBlockCaches = (payload: Parameters<BlockRenderer['removeMemoryBlockCaches']>[0], session: MemoryContext): ReturnType<BlockRenderer['removeMemoryBlockCaches']> => renderers.get(session).removeMemoryBlockCaches(payload);
export const workerSetAllocationLines = (payload: Parameters<BlockRenderer['setAllocationLines']>[0], session: MemoryContext): ReturnType<BlockRenderer['setAllocationLines']> => renderers.get(session).setAllocationLines(payload);
export const workerSetBlockGraphGlobalMaxSize = (payload: Parameters<BlockRenderer['setBlockGraphGlobalMaxSize']>[0], session: MemoryContext): ReturnType<BlockRenderer['setBlockGraphGlobalMaxSize']> => renderers.get(session).setBlockGraphGlobalMaxSize(payload);
export const workerResizeCanvas = (payload: Parameters<BlockRenderer['resizeCanvas']>[0], session: MemoryContext): ReturnType<BlockRenderer['resizeCanvas']> => renderers.get(session).resizeCanvas(payload);
export const workerTransform = (payload: Parameters<BlockRenderer['transform']>[0], session: MemoryContext): ReturnType<BlockRenderer['transform']> => renderers.get(session).transform(payload);
export const workerSetBlockGraphLayerVisibility = (payload: Parameters<BlockRenderer['setBlockGraphLayerVisibility']>[0], session: MemoryContext): ReturnType<BlockRenderer['setBlockGraphLayerVisibility']> => renderers.get(session).setBlockGraphLayerVisibility(payload);
export const workerSetMarkerHoverHighlight = (payload: Parameters<BlockRenderer['setMarkerHoverHighlight']>[0], session: MemoryContext): ReturnType<BlockRenderer['setMarkerHoverHighlight']> => renderers.get(session).setMarkerHoverHighlight(payload);
export const workerHoverItem = (payload: Parameters<BlockRenderer['hoverItem']>[0], session: MemoryContext): ReturnType<BlockRenderer['hoverItem']> => renderers.get(session).hoverItem(payload);
export const workerClickItem = (payload: Parameters<BlockRenderer['clickItem']>[0], session: MemoryContext): ReturnType<BlockRenderer['clickItem']> => renderers.get(session).clickItem(payload);
export const workerSelectItem = (payload: Parameters<BlockRenderer['selectItem']>[0], session: MemoryContext): ReturnType<BlockRenderer['selectItem']> => renderers.get(session).selectItem(payload);
export const workerSelectBlockById = (payload: Parameters<BlockRenderer['selectBlockById']>[0], session: MemoryContext): ReturnType<BlockRenderer['selectBlockById']> => renderers.get(session).selectBlockById(payload);
export const workerDestroy = (session: MemoryContext | undefined): ReturnType<BlockRenderer['destroy']> => session ? renderers.peek(session)?.destroy() : undefined;

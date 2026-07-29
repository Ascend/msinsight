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

import {
    BlockPathBuildProgress,
    BlockPathBuildCancelledError,
    buildBlockViewPath,
    buildBlockViewPathAndWriteToOPFS,
    getInitialBlockGraphMetadata,
    getZoom,
    processReservedLine,
    searchBlockDataByPoint,
    searchBlockDataByPointFromOPFS,
} from '../tools/dataProcess';
import { debounce } from 'lodash';
import { NativeRenderer } from './nativeCanvas/NativeRenderer';
import { store } from '@/store';
import { Session } from '@/entity/session';
import { runInAction } from 'mobx';
import { getCenteredBlockTransform } from '../tools/blockTransform';
import { BlockDataOPFS } from '../tools/BlockDataOPFS';
import { createLeaksOpfsRuntimeId, isLeaksOpfsEnabled } from '../tools/opfsConfig';
import { isPackedRenderData, unpackRenderData } from '../tools/packedBlockData';

const MAIN_THREAD_PROGRESSIVE_BATCH_INTERVAL = 16;

interface ProgressiveRenderState {
    enabled: boolean;
    framePublished: boolean;
    lastBatch: number;
}

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

export class MainThreadRender {
    canvas: HTMLCanvasElement = document.createElement('canvas');
    useOpfs = false;
    memoryBlockData: RenderData | undefined;
    memoryBlockMetadata: BlockGraphMetadata | undefined;
    blockDataOPFS: BlockDataOPFS;
    reservedLine: Array<[number, number]> = [];
    transform: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
    viewport: RenderOptions['viewport'] = { width: 0, height: 0 };
    zoom: RenderOptions['zoom'] = { x: 1, y: 1, offset: 0 };
    renderer: NativeRenderer | null = null;
    hoverItem: Block | null = null;
    clickItem: Block | null = null;
    hoverSearchVersion: number = 0;
    activeDataGeneration: number = 0;
    storageReadyGeneration: number = 0;
    reservedLineOverride: {
        generation: number;
        reservedLine: Array<[number, number]>;
        reservedSizeMax: number;
    } | null = null;

    session: Session;

    constructor() {
        const { sessionStore } = store;
        this.session = sessionStore.activeSession as Session;
        this.blockDataOPFS = new BlockDataOPFS(`main-thread-${createLeaksOpfsRuntimeId()}`);
    }

    private getSizeInfo(): { maxTimestamp: number; minTimestamp: number; maxSize: number; minSize: number } {
        if (this.useOpfs && this.memoryBlockMetadata) {
            const maxSize = Math.max(
                this.memoryBlockMetadata.maxSize,
                this.memoryBlockMetadata.reservedSizeMax ?? this.memoryBlockMetadata.maxSize,
            );
            return {
                maxTimestamp: this.memoryBlockMetadata.maxTimestamp,
                minTimestamp: this.memoryBlockMetadata.minTimestamp,
                maxSize,
                minSize: this.memoryBlockMetadata.minSize,
            };
        }
        if (this.memoryBlockData) {
            const maxSize = Math.max(this.memoryBlockData.maxSize, this.memoryBlockData.reservedSizeMax ?? this.memoryBlockData.maxSize);
            return {
                maxTimestamp: this.memoryBlockData.maxTimestamp,
                minTimestamp: this.memoryBlockData.minTimestamp,
                maxSize,
                minSize: this.memoryBlockData.minSize,
            };
        }
        return { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0 };
    }

    private applyReservedLineOverride(metadata: BlockGraphMetadata, generation: number): BlockGraphMetadata {
        if (this.reservedLineOverride?.generation !== generation) {
            return metadata;
        }
        return {
            ...metadata,
            reservedLine: this.reservedLineOverride.reservedLine,
            reservedSizeMax: this.reservedLineOverride.reservedSizeMax,
        };
    }

    private async resolveHitBlock(payload: Omit<HoverItemPayload, 'type'>): Promise<Block | null> {
        if (this.useOpfs && this.memoryBlockMetadata?.batchCount && this.blockDataOPFS) {
            return searchBlockDataByPointFromOPFS(this.blockDataOPFS, payload, this.transform, this.zoom);
        }
        if (this.memoryBlockData?.blocks?.length) {
            return searchBlockDataByPoint(this.memoryBlockData, payload, this.transform, this.zoom);
        }
        return null;
    }

    async initCanvasHandler(payload: Omit<InitCanvasPayload, 'type'>): Promise<void> {
        this.canvas = payload.canvas as HTMLCanvasElement;
        this.renderer = new NativeRenderer(this.canvas, devicePixelRatio);
        this.viewport = { width: payload.width, height: payload.height };
        await this.renderer.initialize();
        if (isLeaksOpfsEnabled()) {
            await this.blockDataOPFS.init();
        }
    }

    private resetMemoryBlockDataState(generation: number): void {
        this.activeDataGeneration = generation;
        this.storageReadyGeneration = 0;
        if (this.reservedLineOverride?.generation !== generation) {
            this.reservedLineOverride = null;
        }
        this.hoverSearchVersion++;
        this.debouncedSearchBlockData.cancel();
        this.hoverItem = null;
        this.clickItem = null;
    }

    private updateMetadataView(metadata: BlockGraphMetadata, generation: number): void {
        this.memoryBlockMetadata = this.applyReservedLineOverride(metadata, generation);
        this.reservedLine = this.memoryBlockMetadata.reservedLine ?? [];
        this.zoom = getZoom(this.memoryBlockMetadata, this.canvas);
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = this.getSizeInfo();
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
        });
        this.renderer?.setZoom(this.zoom, false);
    }

    private async initializeProgressiveStorage(generation: number): Promise<void> {
        this.storageReadyGeneration = generation;
        await this.renderer?.setDataFromOPFS(this.blockDataOPFS, 0, this.reservedLine, false);
        this.renderer?.updateCanvasSize(this.viewport);
    }

    private async renderProgressiveBatch(
        generation: number,
        endBatch: number,
        progress: BlockPathBuildProgress,
        state: ProgressiveRenderState,
        shouldCancel: () => boolean,
    ): Promise<void> {
        if (shouldCancel()) {
            return;
        }
        if (this.memoryBlockMetadata) {
            this.memoryBlockMetadata.batchCount = endBatch;
        }
        const isFirstProgressiveFrame = !state.framePublished;
        if (!state.enabled ||
            (!isFirstProgressiveFrame &&
                endBatch - state.lastBatch < MAIN_THREAD_PROGRESSIVE_BATCH_INTERVAL)) {
            return;
        }
        state.lastBatch = endBatch;
        await this.renderer?.setDataFromOPFS(this.blockDataOPFS, endBatch, this.reservedLine);
        if (shouldCancel()) {
            return;
        }
        runInAction(() => {
            this.session.progressiveBlocksVisible = true;
            this.session.progressiveRenderedBatchCount = endBatch;
            this.session.progressiveRenderedEventCount = Math.max(
                this.session.progressiveRenderedEventCount,
                progress.processedEventCount,
            );
            this.session.progressiveTotalEventCount = progress.totalEventCount;
            if (isFirstProgressiveFrame) {
                this.session.progressiveFirstRenderedBatchCount = endBatch;
                this.session.progressiveFirstRenderedInstanceCount = 0;
            }
        });
        if (isFirstProgressiveFrame) {
            state.framePublished = true;
            const target = globalThis as {
                __LEAKS_PROGRESSIVE_RENDER_METRICS__?: ProgressiveRenderMetrics;
            };
            target.__LEAKS_PROGRESSIVE_RENDER_METRICS__ = {
                generation,
                firstBatchCount: endBatch,
                firstRenderedInstanceCount: 0,
                firstFrameAt: Date.now(),
            };
        }
    }

    private async setMemoryBlockDataFromOPFS(
        payload: Omit<SetMemoryBlocksDataPayload, 'type'>,
        shouldCancel: () => boolean,
    ): Promise<void> {
        this.useOpfs = true;
        this.memoryBlockData = undefined;
        this.memoryBlockMetadata = getInitialBlockGraphMetadata(payload.data);
        this.reservedLine = this.memoryBlockMetadata.reservedLine ?? [];
        const progressiveState: ProgressiveRenderState = { enabled: false, framePublished: false, lastBatch: 0 };
        const builtMetadata = await buildBlockViewPathAndWriteToOPFS(payload.data, this.blockDataOPFS, {
            generation: payload.generation,
            shouldCancel,
            onGraphMetadataReady: metadata => {
                this.updateMetadataView(metadata, payload.generation);
                progressiveState.enabled = Number.isFinite(this.zoom.x) && Number.isFinite(this.zoom.y);
            },
            onStorageReady: () => this.initializeProgressiveStorage(payload.generation),
            onBatchesCommitted: (_startBatch, endBatch, progress) =>
                this.renderProgressiveBatch(payload.generation, endBatch, progress, progressiveState, shouldCancel),
        });
        this.memoryBlockMetadata = this.applyReservedLineOverride(builtMetadata, payload.generation);
        if (shouldCancel()) {
            throw new BlockPathBuildCancelledError();
        }
        this.updateMetadataView(this.memoryBlockMetadata, payload.generation);
        await this.renderer?.setDataFromOPFS(
            this.blockDataOPFS,
            this.memoryBlockMetadata.batchCount,
            this.reservedLine,
            false,
        );
    }

    private setMemoryBlockDataInMemory(payload: Omit<SetMemoryBlocksDataPayload, 'type'>): void {
        this.useOpfs = false;
        this.memoryBlockMetadata = undefined;
        const renderData = isPackedRenderData(payload.data) ? unpackRenderData(payload.data) : payload.data;
        this.memoryBlockData = buildBlockViewPath(renderData);
        this.reservedLine = this.memoryBlockData.reservedLine ?? [];
        this.zoom = getZoom(this.memoryBlockData, this.canvas);
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = this.getSizeInfo();
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
        });
        this.renderer?.setZoom(this.zoom).setData(this.memoryBlockData.blocks, this.reservedLine);
    }

    private async completeMemoryBlockDataRender(payload: Omit<SetMemoryBlocksDataPayload, 'type'>): Promise<void> {
        await this.renderHighlightData(false);
        this.renderer?.updateCanvasSize(this.viewport);
        await this.renderer?.renderFrame();
        if (this.memoryBlockMetadata?.metrics) {
            (globalThis as { __LEAKS_MEMORY_METRICS__?: BlockGraphBuildMetrics }).__LEAKS_MEMORY_METRICS__ =
                this.memoryBlockMetadata.metrics;
        }
        const progressiveMetricsTarget = globalThis as {
            __LEAKS_PROGRESSIVE_RENDER_METRICS__?: ProgressiveRenderMetrics;
        };
        progressiveMetricsTarget.__LEAKS_PROGRESSIVE_RENDER_METRICS__ = {
            ...progressiveMetricsTarget.__LEAKS_PROGRESSIVE_RENDER_METRICS__,
            generation: payload.generation,
            firstBatchCount: progressiveMetricsTarget.__LEAKS_PROGRESSIVE_RENDER_METRICS__?.firstBatchCount ?? 0,
            firstRenderedInstanceCount:
                progressiveMetricsTarget.__LEAKS_PROGRESSIVE_RENDER_METRICS__?.firstRenderedInstanceCount ?? 0,
            firstFrameAt: progressiveMetricsTarget.__LEAKS_PROGRESSIVE_RENDER_METRICS__?.firstFrameAt ?? 0,
            completedAt: Date.now(),
            totalBatchCount: this.memoryBlockMetadata?.batchCount ?? 0,
        };
        runInAction(() => {
            this.session.loadingBlocks = false;
            this.session.progressiveBlocksVisible = false;
            this.session.progressiveRenderedBatchCount = this.memoryBlockMetadata?.batchCount ?? 0;
            this.session.progressiveRenderedEventCount = this.session.progressiveTotalEventCount;
        });
    }

    async setMemoryBlockDataHandler(
        payload: Omit<SetMemoryBlocksDataPayload, 'type'>,
        shouldCancel: () => boolean = () => false,
    ): Promise<void> {
        this.resetMemoryBlockDataState(payload.generation);
        if (isLeaksOpfsEnabled()) {
            await this.setMemoryBlockDataFromOPFS(payload, shouldCancel);
        } else {
            this.setMemoryBlockDataInMemory(payload);
        }
        await this.completeMemoryBlockDataRender(payload);
    }

    setReservedLineHandler(payload: Omit<SetReservedLinePayload, 'type'>): void {
        const { reservedLine: processedReservedLine, reservedSizeMax } = processReservedLine(payload.reservedLine);
        this.reservedLineOverride = {
            generation: payload.generation,
            reservedLine: processedReservedLine,
            reservedSizeMax,
        };
        if (payload.generation !== this.activeDataGeneration) {
            return;
        }
        this.reservedLine = processedReservedLine;
        if (this.memoryBlockMetadata) {
            this.memoryBlockMetadata.reservedLine = processedReservedLine;
            this.memoryBlockMetadata.reservedSizeMax = reservedSizeMax;
            this.zoom = getZoom(this.memoryBlockMetadata, this.canvas);
        } else if (this.memoryBlockData) {
            this.memoryBlockData.reservedLine = processedReservedLine;
            this.memoryBlockData.reservedSizeMax = reservedSizeMax;
            this.zoom = getZoom(this.memoryBlockData, this.canvas);
        } else {
            return;
        }
        if (!Number.isFinite(this.zoom.x) || !Number.isFinite(this.zoom.y)) {
            return;
        }
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = this.getSizeInfo();
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
        });
        this.renderer?.setReservedLine(this.reservedLine).setZoom(
            this.zoom,
            !this.useOpfs || this.storageReadyGeneration === payload.generation,
        );
    }

    resizeCanvasHandler(payload: Omit<ResizeCanvasPayload, 'type'>): void {
        this.viewport = { width: payload.width, height: payload.height };
        this.renderer?.updateCanvasSize(this.viewport);
        if (!this.useOpfs && this.memoryBlockData === undefined) {
            return;
        }
        if (this.useOpfs && this.memoryBlockMetadata === undefined) {
            return;
        }
        const zoomSource = this.useOpfs ? this.memoryBlockMetadata : this.memoryBlockData;
        if (!zoomSource) {
            return;
        }
        this.zoom = getZoom(zoomSource, this.canvas);
        this.renderer?.setZoom(this.zoom);
    }

    transformHandler(payload: Omit<TransformPayload, 'type'>): void {
        this.transform = payload.transform;
        this.renderer?.setTransform(this.transform);
    }

    debouncedSearchBlockData = debounce(async (
        payload: Omit<HoverItemPayload, 'type'>,
        requestVersion: number,
    ): Promise<void> => {
        const nextHoverItem = await this.resolveHitBlock(payload);
        if (requestVersion !== this.hoverSearchVersion) {
            return;
        }
        if (isSamePathFragment(this.hoverItem, nextHoverItem)) {
            return;
        }
        this.hoverItem = nextHoverItem;
        await this.renderHighlightData();
        runInAction(() => {
            this.session.leaksWorkerInfo.hoverItem = this.hoverItem;
        });
    }, 10);

    async clickItemHandler(payload: Omit<ClickItemPayload, 'type'>): Promise<void> {
        this.clickItem = await this.resolveHitBlock(payload);
        runInAction(() => {
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
        await this.renderHighlightData();
    }

    async selectItemHandler(payload: Omit<SelectBlockItemPayload, 'type'>): Promise<void> {
        this.clickItem = payload.item;
        await this.renderHighlightData();
        runInAction(() => {
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
    }

    async selectBlockByIdHandler(payload: Omit<SelectBlockByIdPayload, 'type'>): Promise<void> {
        if (this.useOpfs && this.blockDataOPFS) {
            this.clickItem = await this.blockDataOPFS.findBlockById(payload.blockId);
        } else {
            this.clickItem = this.memoryBlockData?.blocks?.find(block => block.id === payload.blockId) ?? null;
        }
        const centeredTransform = this.clickItem === null
            ? null
            : getCenteredBlockTransform(this.clickItem, this.viewport, this.zoom);
        if (centeredTransform !== null) {
            this.transform = centeredTransform;
            this.renderer?.setTransform(this.transform);
        }
        await this.renderHighlightData();
        runInAction(() => {
            this.session.leaksWorkerInfo.renderOptions.transform = this.transform;
            this.session.leaksWorkerInfo.clickItem = this.clickItem;
        });
    }

    hoverItemHandler(payload: Omit<HoverItemPayload, 'type'>): void {
        const requestVersion = ++this.hoverSearchVersion;
        this.debouncedSearchBlockData(payload, requestVersion);
    }

    async renderHighlightData(render: boolean = true): Promise<void> {
        const result: Block[] = [];
        if (this.clickItem !== null) {
            result.push(this.clickItem);
        }
        if (this.hoverItem !== null && this.hoverItem.id !== this.clickItem?.id) {
            result.push(this.hoverItem);
        }
        this.renderer?.setBaseDimmed(this.clickItem !== null, false);
        this.renderer?.setHighlightData(result, render);
    }

    async destroyHandler(): Promise<void> {
        this.hoverSearchVersion++;
        this.debouncedSearchBlockData.cancel();
        this.useOpfs = false;
        this.memoryBlockData = {
            maxTimestamp: 0,
            minTimestamp: 0,
            maxSize: 0,
            minSize: 0,
            blocks: [],
            reservedLine: [],
            reservedSizeMax: 0,
        };
        this.memoryBlockMetadata = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, batchCount: 0 };
        this.reservedLine = [];
        this.transform = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
        this.zoom = { x: 1, y: 1, offset: 0 };
        this.hoverItem = null;
        this.clickItem = null;
        runInAction(() => {
            this.session.leaksWorkerInfo.sizeInfo = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0 };
            this.session.leaksWorkerInfo.renderOptions.zoom = this.zoom;
            this.session.leaksWorkerInfo.clickItem = null;
            this.session.leaksWorkerInfo.hoverItem = null;
        });
        await this.renderHighlightData(false);
        await this.blockDataOPFS?.clear();
        await this.renderer?.setDataFromOPFS(null, 0, [], false);
        this.renderer?.setTransform(this.transform).setZoom(this.zoom, false).updateCanvasSize(this.viewport);
    }
}

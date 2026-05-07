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
import { MainThreadRender } from './mainThreadindex';
export const BlockWorker = new Worker(new URL('./', import.meta.url));

// Worker 实现
const WorkerBackend = {
    initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
        const devicePixelRatio = window.devicePixelRatio || 1;
        const offscreenCanvas = (canvas as HTMLCanvasElement).transferControlToOffscreen();
        BlockWorker.postMessage(
            { type: 'initCanvas', canvas: offscreenCanvas, devicePixelRatio, width, height },
            [offscreenCanvas],
        );
    },
    setMemoryBlockData({ data }: Omit<SetMemoryBlocksDataPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'setMemoryBlockData', data });
    },
    resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'resizeCanvas', width, height });
    },
    transform({ transform }: Omit<TransformPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'transform', transform });
    },
    hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'hoverItem', clientX, clientY });
    },
    clickItem({ clientX, clientY, selectionVersion }: Omit<HoverItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'clickItem', clientX, clientY, selectionVersion });
    },
    selectItem({ item, selectionVersion }: Omit<SelectBlockItemPayload, 'type'>): void {
        BlockWorker.postMessage({ type: 'selectBlockItem', item, selectionVersion });
    },
    destroy(): void {
        BlockWorker.postMessage({ type: 'destroy' });
    },
};

// 主线程实现（fallback）
const mainThreadRender = new MainThreadRender();
const MainThreadBackend = {
    initCanvas({ canvas, width, height }: Omit<InitCanvasPayload, 'type' | 'devicePixelRatio'>): void {
        const devicePixelRatio = window.devicePixelRatio || 1;
        mainThreadRender.initCanvasHandler({ canvas, devicePixelRatio, width, height });
    },
    setMemoryBlockData({ data }: Omit<SetMemoryBlocksDataPayload, 'type'>): void {
        mainThreadRender.setMemoryBlockDataHandler({ data });
    },
    resizeCanvas({ width, height }: Omit<ResizeCanvasPayload, 'type'>): void {
        mainThreadRender.resizeCanvasHandler({ width, height });
    },
    transform({ transform }: Omit<TransformPayload, 'type'>): void {
        mainThreadRender.transformHandler({ transform });
    },
    hoverItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
        mainThreadRender.hoverItemHandler({ clientX, clientY });
    },
    clickItem({ clientX, clientY }: Omit<HoverItemPayload, 'type'>): void {
        mainThreadRender.clickItemHandler({ clientX, clientY });
    },
    selectItem({ item }: Omit<SelectBlockItemPayload, 'type'>): void {
        mainThreadRender.selectItemHandler({ item });
    },
    destroy(): void {
        mainThreadRender.destroyHandler();
    },
};

// 自动选择后端
const backend = isWebGL2Supported() ? WorkerBackend : MainThreadBackend;

// 导出统一接口
export const workerInitCanvas = backend.initCanvas;
export const workerSetMemoryBlockData = backend.setMemoryBlockData;
export const workerResizeCanvas = backend.resizeCanvas;
export const workerTransform = backend.transform;
export const workerHoverItem = backend.hoverItem;
export const workerClickItem = backend.clickItem;
export const workerSelectItem = backend.selectItem;
export const workerDestroy = backend.destroy;

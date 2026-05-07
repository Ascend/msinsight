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
import { buildBlockViewPath, getZoom, searchBlockDataByPoint } from '../tools/dataProcess';
import { debounce } from 'lodash';

let canvas: OffscreenCanvas;
let memoryBlockData: RenderData;
let transform: RenderOptions['transform'] = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
let viewport: RenderOptions['viewport'];
let zoom: RenderOptions['zoom'];
let renderer: WebGLRenderer | null;
let hoverItem: Block | null = null;
let clickItem: Block | null = null;

const initCanvasHandler = async (payload: InitCanvasPayload): Promise<void> => {
    canvas = payload.canvas as OffscreenCanvas;
    renderer = new WebGLRenderer(canvas, payload.devicePixelRatio);
    viewport = { width: payload.width, height: payload.height };
    await renderer.initialize();
};

const setMemoryBlockDataHandler = (payload: SetMemoryBlocksDataPayload): void => {
    hoverItem = null;
    clickItem = null;
    memoryBlockData = buildBlockViewPath(payload.data);
    const { maxTimestamp, minTimestamp, maxSize, minSize } = memoryBlockData;
    zoom = getZoom(memoryBlockData, canvas);
    self.postMessage({
        type: 'dataInfo',
        sizeInfo: {
            maxTimestamp,
            minTimestamp,
            maxSize,
            minSize,
        },
        zoom,
    });
    renderer?.setZoom(zoom).setData(memoryBlockData.blocks);
    renderHighlightData();
    renderer?.updateCanvasSize(viewport);
    self.postMessage({ type: 'renderCompleted' });
};

const resizeCanvasHandler = (payload: ResizeCanvasPayload): void => {
    viewport = { width: payload.width, height: payload.height };
    renderer?.updateCanvasSize(viewport);
    if (memoryBlockData === undefined) {
        return;
    }
    zoom = getZoom(memoryBlockData, canvas);
    const { maxTimestamp, minTimestamp, maxSize, minSize } = memoryBlockData;
    self.postMessage({
        type: 'dataInfo',
        sizeInfo: {
            maxTimestamp,
            minTimestamp,
            maxSize,
            minSize,
        },
        zoom,
    });
    renderer?.setZoom(zoom);
};

const transformHandler = (payload: TransformPayload): void => {
    transform = payload.transform;
    renderer?.setTransform(transform);
};

const debouncedSearchBlockData = debounce((payload: HoverItemPayload): void => {
    if (memoryBlockData?.blocks?.length > 0) {
        hoverItem = searchBlockDataByPoint(memoryBlockData, payload, transform, zoom);
        renderHighlightData();
        self.postMessage({ type: 'hoverItemResult', result: hoverItem });
    }
}, 10);

const clickItemHandler = (payload: HoverItemPayload): void => {
    clickItem = searchBlockDataByPoint(memoryBlockData, payload, transform, zoom);
    self.postMessage({ type: 'clickItemResult', result: clickItem, selectionVersion: payload.selectionVersion });
    renderHighlightData();
};

const selectItemHandler = (payload: SelectBlockItemPayload): void => {
    clickItem = payload.item;
    renderHighlightData();
    self.postMessage({ type: 'clickItemResult', result: clickItem, selectionVersion: payload.selectionVersion });
};

const hoverItemHandler = (payload: HoverItemPayload): void => {
    debouncedSearchBlockData(payload);
};

const renderHighlightData = (): void => {
    const result: Block[] = [];
    if (clickItem !== null) {
        result.push(clickItem);
    }
    if (hoverItem !== null && hoverItem.id !== clickItem?.id) {
        result.push(hoverItem);
    }
    renderer?.setBaseDimmed(clickItem !== null);
    renderer?.setHighlightData(result);
};

const destroyHandler = (): void => {
    memoryBlockData = { maxTimestamp: 0, minTimestamp: 0, maxSize: 0, minSize: 0, blocks: [] };
    transform = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
    zoom = { x: 1, y: 1, offset: 0 };
    hoverItem = null;
    clickItem = null;
    self.postMessage({
        type: 'dataInfo',
        sizeInfo: {
            maxTimestamp: 0,
            minTimestamp: 0,
            maxSize: 0,
            minSize: 0,
        },
        zoom,
    });
    self.postMessage({ type: 'clickItemResult', result: null });
    renderHighlightData();
    renderer?.setData([]).setTransform(transform).setZoom(zoom);
};

const Handlers: PayloadHandlers = {
    initCanvas: initCanvasHandler,
    setMemoryBlockData: setMemoryBlockDataHandler,
    resizeCanvas: resizeCanvasHandler,
    transform: transformHandler,
    hoverItem: hoverItemHandler,
    clickItem: clickItemHandler,
    selectBlockItem: selectItemHandler,
    destroy: destroyHandler,
};

self.onmessage = async (ev: MessageEvent<Payload>): Promise<void> => {
    const payload = ev.data;
    const handler = Handlers[payload.type];
    if (typeof handler === 'function') {
        try {
            await handler(payload);
        } catch (error) {
            // eslint-disable-next-line no-console
            console.error('Block worker handler error:', error);
        }
    }
};

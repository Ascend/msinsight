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

import { getMemoryStateRenderData, getMemoryStateZoom, searchStateDataByPoint } from '../tools/dataProcess';
import { WebGLRenderer } from './webgl/WebGLRenderer';
import { debounce } from 'lodash';

let canvas: OffscreenCanvas;
let memoryStateData: Segment[];
let transform: RenderOptions['transform'] = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
let viewport: RenderOptions['viewport'];
let zoom: RenderOptions['zoom'];
let renderer: WebGLRenderer | null;

let hoverItem: StateDataHoverResult | null = null;
let clickItem: StateDataHoverResult | null = null;

const initCanvasHandler = async (payload: InitCanvasPayload): Promise<void> => {
    canvas = payload.canvas as OffscreenCanvas;
    renderer = new WebGLRenderer(canvas, payload.devicePixelRatio);
    canvas.width = payload.width;
    canvas.height = payload.height;
    viewport = { width: payload.width, height: payload.height };
    await renderer.initialize();
};

const setMemoryStateDataHandler = (payload: SetMemoryStateDataPayload): void => {
    clickItem = null;
    hoverItem = null;
    renderHighlintData();
    memoryStateData = getMemoryStateRenderData(payload.data);
    zoom = getMemoryStateZoom(memoryStateData, canvas);
    renderer?.setZoom(zoom).setData(memoryStateData);
    renderer?.updateCanvasSize(viewport);
};

const resizeCanvasHandler = (payload: ResizeCanvasPayload): void => {
    viewport = { width: payload.width, height: payload.height };
    renderer?.updateCanvasSize(viewport);
    if (memoryStateData === undefined) {
        return;
    }
    zoom = getMemoryStateZoom(memoryStateData, canvas);
    renderer?.setZoom(zoom);
};

const transformHandler = (payload: TransformPayload): void => {
    transform = payload.transform;
    renderer?.setTransform(transform);
};

const debouncedSearchBlockData = debounce((payload: HoverItemPayload): void => {
    if (memoryStateData?.length > 0) {
        hoverItem = searchStateDataByPoint(memoryStateData, payload, transform, zoom);
        renderHighlintData();
        self.postMessage({ type: 'hoverItemResult', result: hoverItem });
    }
}, 10);

const clickItemHandler = (payload: HoverItemPayload): void => {
    clickItem = searchStateDataByPoint(memoryStateData, payload, transform, zoom);
    self.postMessage({ type: 'clickItemResult', result: clickItem });
    renderHighlintData();
};

const selectItemHandler = (payload: SelectStateItemPayload): void => {
    clickItem = payload.item;
    renderHighlintData();
    self.postMessage({ type: 'clickItemResult', result: clickItem });
};

const hoverItemHandler = (payload: HoverItemPayload): void => {
    debouncedSearchBlockData(payload);
};

// 通过这个方法，优先高亮hover数据
const renderHighlintData = (): void => {
    const result = getHighlightData();
    renderer?.setHighlightData(result);
};

const isSameHighlightItem = (leftItem: StateDataHoverResult | null, rightItem: StateDataHoverResult | null): boolean => {
    if (leftItem === null || rightItem === null || leftItem.type !== rightItem.type) {
        return false;
    }
    if (leftItem.type === 'segment') {
        return leftItem.data.allocOrMapEventId === rightItem.data.allocOrMapEventId;
    }
    return leftItem.data.blocks[0]?.id === rightItem.data.blocks[0]?.id;
};

const getHighlightData = (): StateDataHoverResult[] => {
    const result: StateDataHoverResult[] = [];
    if (clickItem !== null) {
        result.push(clickItem);
    }
    if (hoverItem !== null && !isSameHighlightItem(hoverItem, clickItem)) {
        result.push(hoverItem);
    }
    return result;
};

const destroyHandler = (): void => {
    memoryStateData = [];
    transform = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
    zoom = { x: 1, y: 1, offset: 0 };
    clickItem = null;
    hoverItem = null;
    renderHighlintData();
    renderer?.setData([]).setTransform(transform).setZoom(zoom);
};

type StateWorkerPayloadType = 'initCanvas' | 'setMemoryStateData' | 'resizeCanvas' | 'transform' | 'hoverItem' | 'clickItem' | 'selectStateItem' | 'destroy';
const Handlers: PayloadHandlers<StateWorkerPayloadType> = {
    initCanvas: initCanvasHandler,
    setMemoryStateData: setMemoryStateDataHandler,
    resizeCanvas: resizeCanvasHandler,
    transform: transformHandler,
    hoverItem: hoverItemHandler,
    clickItem: clickItemHandler,
    selectStateItem: selectItemHandler,
    destroy: destroyHandler,
};

self.onmessage = async (ev: MessageEvent<Payload>): Promise<void> => {
    const payload = ev.data;
    const handler = Handlers[payload.type as StateWorkerPayloadType] as ((payload: Payload) => void) | undefined;
    await handler?.(payload);
};

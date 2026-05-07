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

interface InitCanvasPayload {
    type: 'initCanvas';
    canvas: OffscreenCanvas | HTMLCanvasElement;
    width: number;
    height: number;
    devicePixelRatio: number;
};

interface SetMemoryBlocksDataPayload {
    type: 'setMemoryBlockData';
    data: RenderData;
};

interface ResizeCanvasPayload {
    type: 'resizeCanvas';
    width: number;
    height: number;
};

interface TransformPayload {
    type: 'transform';
    transform: RenderOptions['transform'];
};

interface HoverItemPayload {
    type: 'hoverItem';
    clientX: number;
    clientY: number;
    selectionVersion?: number;
};

interface SelectBlockItemPayload {
    type: 'selectBlockItem';
    item: Block | null;
    selectionVersion?: number;
};

interface SelectStateItemPayload {
    type: 'selectStateItem';
    item: StateDataHoverResult | null;
    selectionVersion?: number;
};

interface SetMemoryStateDataPayload {
    type: 'setMemoryState';
    data: Segment[];
};

type Payload =
    | InitCanvasPayload
    | SetDataPayload
    | ResizeCanvasPayload
    | TransformPayload
    | HoverItemPayload
    | SelectBlockItemPayload
    | SelectStateItemPayload
    | SetMemoryStateDataPayload;

type PayloadHandlers = Partial<{
    [K in Payload['type']]: (payload: Extract<Payload, { type: K }>) => void;
}>;

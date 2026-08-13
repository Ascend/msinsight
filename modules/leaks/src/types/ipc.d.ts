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
    generation: number;
    data: RenderData | PackedRenderData;
    fileHash?: string;
};

interface LoadMemoryBlockCachePayload {
    type: 'loadMemoryBlockCache';
    generation: number;
    fileHash?: string;
};

interface CheckOpfsAvailabilityPayload {
    type: 'checkOpfsAvailability';
    requestId: number;
};

interface SetReservedLinePayload {
    type: 'setReservedLine';
    generation: number;
    reservedLine: ReservedLinePoint[];
};

interface ReservedLinePoint {
    timestamp: number;
    reservedSize: number;
};

interface DestroyPayload {
    type: 'destroy';
    generation: number;
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

interface ClickItemPayload {
    type: 'clickItem';
    clientX: number;
    clientY: number;
    selectionVersion?: number;
};

interface SelectBlockItemPayload {
    type: 'selectBlockItem';
    item: Block | null;
    selectionVersion?: number;
};

interface SelectBlockByIdPayload {
    type: 'selectBlockById';
    blockId: number;
    selectionVersion?: number;
};

interface SelectStateItemPayload {
    type: 'selectStateItem';
    item: StateDataHoverResult | null;
    selectionVersion?: number;
};

interface SetMemoryStateDataPayload {
    type: 'setMemoryStateData';
    data: Segment[];
};

type Payload =
    | InitCanvasPayload
    | CheckOpfsAvailabilityPayload
    | LoadMemoryBlockCachePayload
    | SetMemoryBlocksDataPayload
    | SetReservedLinePayload
    | ResizeCanvasPayload
    | TransformPayload
    | HoverItemPayload
    | ClickItemPayload
    | SelectBlockItemPayload
    | SelectBlockByIdPayload
    | SelectStateItemPayload
    | SetMemoryStateDataPayload
    | DestroyPayload;

type PayloadHandlers = Partial<{
    [K in Payload['type']]: (payload: Extract<Payload, { type: K }>) => void | Promise<void>;
}>;

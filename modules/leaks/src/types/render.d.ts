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

interface RenderOptions {
    transform: {
        x: number;
        y: number;
        scaleX: number;
        scaleY: number;
    };
    viewport: {
        width: number;
        height: number;
    };
    zoom: {
        x: number;
        y: number;
        offset: number;
    };
}

interface Block {
    id: number;
    addr: string;
    _startTimestamp: number;
    _endTimestamp: number;
    size: number;
    path: Array<[number, number]>;
}

interface PackedRenderData {
    maxTimestamp: number;
    minTimestamp: number;
    maxSize: number;
    minSize: number;
    ids: Float64Array;
    startTimestamps: Float64Array;
    endTimestamps: Float64Array;
    sizes: Float64Array;
    addressIndices: Uint32Array;
    addresses: string[];
    reservedLine?: Array<[number, number]>;
    reservedSizeMax?: number;
    transferBytes: number;
}

interface BlockGraphBuildMetrics {
    generation: number;
    blockCount: number;
    eventCount: number;
    maxActiveBlocks: number;
    peakActivePathPoints: number;
    pathPoints: number;
    droppedPathPoints: number;
    batchCount: number;
    opfsPathBytes: number;
    transferBytes: number;
    buildDurationMs: number;
    lodApplied: boolean;
}

interface ProgressiveRenderMetrics {
    generation: number;
    firstBatchCount: number;
    firstRenderedInstanceCount: number;
    firstFrameAt: number;
    completedAt?: number;
    totalBatchCount?: number;
}

interface BlockMeta {
    id: number;
    addr: string;
    _startTimestamp: number;
    _endTimestamp: number;
    pathStartTimestamp: number;
    pathEndTimestamp: number;
    size: number;
    pathOffset: number;
    pathLength: number;
}

interface BatchData {
    metas: BlockMeta[];
    pathData: Float32Array;
}

interface BatchTimeRange {
    minStartTimestamp: number;
    maxEndTimestamp: number;
}

type SyncHandle = FileSystemSyncAccessHandle;

interface BlockGraphMetadata {
    maxTimestamp: number;
    minTimestamp: number;
    maxSize: number;
    minSize: number;
    batchCount: number;
    reservedLine?: Array<[number, number]>;
    reservedSizeMax?: number;
    metrics?: BlockGraphBuildMetrics;
}

interface RenderData {
    maxTimestamp: number;
    minTimestamp: number;
    maxSize: number;
    minSize: number;
    blocks: Block[];
    reservedLine?: Array<[number, number]>;
    reservedSizeMax?: number;
}

interface SpatialGrid {
    [key: string]: number[]; // 存储区块索引的数组
}

interface Shader {
    vertexShader: string;
    fragmentShader: string;
}

interface Segment {
    address: string;
    stream: number;
    size: number;
    blocks: StateBlock[];
    offsetX: number;
    offsetY: number;
    allocOrMapEventId: number;
    eventId?: number;
}

interface StateBlock {
    offset: number;
    id: number;
    size: number;
    colorIndex?: number;
}

type Theme = 'light' | 'dark';

interface StateDataHoverResult {
    type: 'segment' | 'block';
    data: Segment;
}

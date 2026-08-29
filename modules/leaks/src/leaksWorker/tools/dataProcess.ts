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

import { BlockDataOPFS, blockFromMeta, getPointFromPathData, type PackedBlockPath } from './BlockDataOPFS';
import { isPackedRenderData } from './packedBlockData';

const processAllocationLine = <T extends { timestamp: number }>(points: T[] | undefined, valueKey: keyof T): {
    points: Array<[number, number]>;
    maxSize: number;
} => {
    let maxSize = 0;
    const processedPoints = (points ?? []).map(point => {
        const timestamp = Number(point.timestamp);
        const value = Number(point[valueKey]);
        maxSize = Math.max(maxSize, value);
        return [timestamp, value] as [number, number];
    });
    return { points: processedPoints, maxSize };
};

export const processAllocationLines = (payload: Omit<SetAllocationLinesPayload, 'type' | 'generation'>): {
    lines: AllocationLineData;
    allocationLineSizeMax: number;
} => {
    const reserved = processAllocationLine(payload.reservedLine, 'reservedSize');
    const processUsed = processAllocationLine(payload.processUsedLine, 'processUsed');
    const deviceUsed = processAllocationLine(payload.deviceUsedLine, 'deviceUsed');
    return {
        lines: {
            reservedLine: reserved.points,
            processUsedLine: processUsed.points,
            deviceUsedLine: deviceUsed.points,
        },
        allocationLineSizeMax: Math.max(reserved.maxSize, processUsed.maxSize, deviceUsed.maxSize),
    };
};

export const getZoom = (
    data: RenderData | BlockGraphMetadata,
    canvas: OffscreenCanvas | HTMLCanvasElement,
): RenderOptions['zoom'] => {
    const maxSize = Math.max(data.maxSize, data.reservedSizeMax ?? data.maxSize);
    return {
        x: canvas.width / (data.maxTimestamp - data.minTimestamp),
        y: canvas.height / (maxSize - data.minSize),
        offset: data.minTimestamp,
    };
};

const getTransformScaleX = (transform: RenderOptions['transform']): number => transform.scaleX;
const getTransformScaleY = (transform: RenderOptions['transform']): number => transform.scaleY;

const BLOCK_SNAP_TARGET_WIDTH_PX = 6;
const blockViewPathCache = new WeakMap<SetMemoryBlocksDataPayload['data'], RenderData>();
const stateRenderDataCache = new WeakMap<Segment[], Segment[]>();

const getNow = (): number => typeof performance === 'undefined' ? Date.now() : performance.now();

const tracePerf = (label: string, startedAt: number, extra: Record<string, number>): void => {
    const perfGlobal = globalThis as {
        __LEAKS_PERF_TRACE__?: boolean;
        __LEAKS_PERF_LAST__?: Record<string, Record<string, number>>;
    };
    const traceEnabled = perfGlobal.__LEAKS_PERF_TRACE__ === true;
    if (!traceEnabled) {
        return;
    }
    perfGlobal.__LEAKS_PERF_LAST__ = {
        ...perfGlobal.__LEAKS_PERF_LAST__,
        [label]: { ...extra, duration: Number((getNow() - startedAt).toFixed(2)) },
    };
};

const upperBound = <T>(data: T[], value: number, getter: (item: T) => number): number => {
    let left = 0;
    let right = data.length;
    while (left < right) {
        const mid = Math.floor((left + right) / 2);
        if (getter(data[mid]) <= value) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
};

const isPointInBlockEntry = (
    block: Block,
    pathIndex: number,
    absoluteX: number,
    y: number,
    minHitWidth: number,
): boolean => {
    const startPt = block.path[pathIndex];
    const endPt = block.path[pathIndex + 1];
    const rx = Math.max(endPt[0], startPt[0] + minHitWidth);
    return isPointInExtrudedSegment(absoluteX, y, startPt[0], startPt[1], rx, endPt[1], block.size);
};

const getBlockSnapCandidate = (
    block: Block,
    pathIndex: number,
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): { block: Block; distance: number; order: number } | null => {
    const startPt = block.path[pathIndex];
    const endPt = block.path[pathIndex + 1];
    const visibleStart = startPt[0];
    const visibleEnd = Math.max(endPt[0], startPt[0] + minHitWidth);
    const visibleWidth = visibleEnd - visibleStart;
    if (snapHitWidth <= minHitWidth || visibleWidth >= snapHitWidth) {
        return null;
    }
    if (absoluteX >= visibleStart && absoluteX <= visibleEnd) {
        return null;
    }
    const extraWidth = (snapHitWidth - visibleWidth) / 2;
    if (absoluteX < visibleStart - extraWidth || absoluteX > visibleEnd + extraWidth) {
        return null;
    }
    const snapX = absoluteX < visibleStart ? visibleStart : visibleEnd;
    if (!isPointInExtrudedSegment(snapX, y, startPt[0], startPt[1], visibleEnd, endPt[1], block.size)) {
        return null;
    }
    return {
        block,
        distance: Math.abs(absoluteX - snapX),
        order: pathIndex,
    };
};

const pickUniqueSnapBlock = (candidates: Array<{ block: Block; distance: number; order: number }>): Block | null => {
    if (candidates.length < 1) {
        return null;
    }
    const candidatesByBlockId = new Map<number, { block: Block; distance: number; order: number }>();
    for (const candidate of candidates) {
        const existing = candidatesByBlockId.get(candidate.block.id);
        if (existing === undefined || candidate.distance < existing.distance ||
            (candidate.distance === existing.distance && candidate.order < existing.order)) {
            candidatesByBlockId.set(candidate.block.id, candidate);
        }
    }
    if (candidatesByBlockId.size !== 1) {
        return null;
    }
    const candidate = candidatesByBlockId.values().next().value;
    return candidate === undefined ? null : candidate.block;
};

const searchBlockDataDynamically = (
    blocks: RenderData['blocks'],
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): Block | null => {
    const exactStart = absoluteX - minHitWidth;
    const exactEnd = absoluteX;
    const exactBlockLimit = upperBound(blocks, exactEnd, block => block._startTimestamp);
    for (let blockIndex = 0; blockIndex < exactBlockLimit; blockIndex++) {
        const block = blocks[blockIndex];
        if (block._endTimestamp < exactStart || block.path.length <= 1) {
            continue;
        }
        const firstPathIndex = Math.max(0, upperBound(block.path, exactStart, point => point[0]) - 2);
        const lastPathIndex = Math.min(block.path.length - 2, upperBound(block.path, exactEnd, point => point[0]));
        for (let pathIndex = firstPathIndex; pathIndex <= lastPathIndex; pathIndex++) {
            const startPoint = block.path[pathIndex];
            const endPoint = block.path[pathIndex + 1];
            const start = Math.min(startPoint[0], endPoint[0]);
            const end = Math.max(endPoint[0], startPoint[0] + minHitWidth);
            const minY = Math.min(startPoint[1], endPoint[1]);
            const maxY = Math.max(startPoint[1], endPoint[1]) + block.size;
            if (absoluteX < start || absoluteX > end || y < minY || y > maxY) {
                continue;
            }
            if (isPointInBlockEntry(block, pathIndex, absoluteX, y, minHitWidth)) {
                return block;
            }
        }
    }

    if (snapHitWidth <= minHitWidth) {
        return null;
    }

    const snapStart = absoluteX - snapHitWidth;
    const snapEnd = absoluteX + snapHitWidth;
    const snapBlockLimit = upperBound(blocks, snapEnd, block => block._startTimestamp);
    const candidates: Array<{ block: Block; distance: number; order: number }> = [];
    for (let blockIndex = 0; blockIndex < snapBlockLimit; blockIndex++) {
        const block = blocks[blockIndex];
        if (block._endTimestamp < snapStart || block.path.length <= 1) {
            continue;
        }
        const firstPathIndex = Math.max(0, upperBound(block.path, snapStart, point => point[0]) - 2);
        const lastPathIndex = Math.min(block.path.length - 2, upperBound(block.path, snapEnd, point => point[0]));
        for (let pathIndex = firstPathIndex; pathIndex <= lastPathIndex; pathIndex++) {
            const candidate = getBlockSnapCandidate(block, pathIndex, absoluteX, y, minHitWidth, snapHitWidth);
            if (candidate !== null) {
                candidates.push({
                    ...candidate,
                    order: blockIndex * 1000000 + pathIndex,
                });
            }
        }
    }
    return pickUniqueSnapBlock(candidates);
};

export const searchBlockDataByPoint = (
    data: RenderData | RenderData['blocks'],
    { clientX, clientY }: Omit<HoverItemPayload, 'type'>,
    transform: RenderOptions['transform'],
    zoom: RenderOptions['zoom'],
): Block | null => {
    const blocks = Array.isArray(data) ? data : data.blocks;
    const x = (clientX - transform.x) / zoom.x / getTransformScaleX(transform);
    const y = (clientY - transform.y) / zoom.y / getTransformScaleY(transform);
    const absoluteX = x + zoom.offset;
    const scaleX = getTransformScaleX(transform);
    const minHitWidth = zoom.x * scaleX > 0 ? 1 / zoom.x / scaleX : 0;
    const snapHitWidth = zoom.x * scaleX > 0 ? BLOCK_SNAP_TARGET_WIDTH_PX / zoom.x / scaleX : 0;
    return searchBlockDataDynamically(blocks, absoluteX, y, minHitWidth, snapHitWidth);
};

const isPointInMetaEntry = (
    meta: BlockMeta,
    pathData: Float32Array,
    pathIndex: number,
    absoluteX: number,
    y: number,
    minHitWidth: number,
): boolean => {
    const startPt = getPointFromPathData(pathData, meta.pathOffset, pathIndex);
    const endPt = getPointFromPathData(pathData, meta.pathOffset, pathIndex + 1);
    const rx = Math.max(endPt[0], startPt[0] + minHitWidth);
    return isPointInExtrudedSegment(absoluteX, y, startPt[0], startPt[1], rx, endPt[1], meta.size);
};

const getMetaSnapCandidate = (
    meta: BlockMeta,
    pathData: Float32Array,
    pathIndex: number,
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): { meta: BlockMeta; distance: number; order: number } | null => {
    const startPt = getPointFromPathData(pathData, meta.pathOffset, pathIndex);
    const endPt = getPointFromPathData(pathData, meta.pathOffset, pathIndex + 1);
    const visibleStart = startPt[0];
    const visibleEnd = Math.max(endPt[0], startPt[0] + minHitWidth);
    const visibleWidth = visibleEnd - visibleStart;
    if (snapHitWidth <= minHitWidth || visibleWidth >= snapHitWidth) {
        return null;
    }
    if (absoluteX >= visibleStart && absoluteX <= visibleEnd) {
        return null;
    }
    const extraWidth = (snapHitWidth - visibleWidth) / 2;
    if (absoluteX < visibleStart - extraWidth || absoluteX > visibleEnd + extraWidth) {
        return null;
    }
    const snapX = absoluteX < visibleStart ? visibleStart : visibleEnd;
    if (!isPointInExtrudedSegment(snapX, y, startPt[0], startPt[1], visibleEnd, endPt[1], meta.size)) {
        return null;
    }
    return {
        meta,
        distance: Math.abs(absoluteX - snapX),
        order: pathIndex,
    };
};

const pickUniqueSnapMeta = (
    candidates: Array<{ meta: BlockMeta; pathData: Float32Array; distance: number; order: number }>,
): Block | null => {
    if (candidates.length < 1) {
        return null;
    }
    const candidatesByBlockId = new Map<number, { meta: BlockMeta; pathData: Float32Array; distance: number; order: number }>();
    for (const candidate of candidates) {
        const existing = candidatesByBlockId.get(candidate.meta.id);
        if (existing === undefined || candidate.distance < existing.distance ||
            (candidate.distance === existing.distance && candidate.order < existing.order)) {
            candidatesByBlockId.set(candidate.meta.id, candidate);
        }
    }
    if (candidatesByBlockId.size !== 1) {
        return null;
    }
    const candidate = candidatesByBlockId.values().next().value;
    return candidate === undefined ? null : blockFromMeta(candidate.meta, candidate.pathData);
};

const searchBlockDataInBatch = (
    batchData: BatchData,
    x: number,
    y: number,
    zoom: RenderOptions['zoom'],
    minHitWidth: number,
): Block | null => {
    const { metas, pathData } = batchData;
    const offset = zoom.offset;
    const absoluteX = x + offset;
    const dataLen = metas.length;

    let left = 0;
    let right = dataLen;
    while (left < right) {
        const mid = Math.floor((left + right) / 2);
        if (metas[mid].pathStartTimestamp - offset > x) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    const endIdx = left;
    for (let i = 0; i < endIdx; i++) {
        const meta = metas[i];
        if (x > meta.pathEndTimestamp - offset) {
            continue;
        }
        if (meta.pathLength <= 1) {
            continue;
        }
        const pathLen = meta.pathLength;
        let pLeft = 0;
        let pRight = pathLen - 1;
        while (pLeft < pRight) {
            const mid = Math.ceil((pLeft + pRight) / 2);
            const midX = getPointFromPathData(pathData, meta.pathOffset, mid)[0] - offset;
            if (x < midX) {
                pRight = mid - 1;
            } else {
                pLeft = mid;
            }
        }
        const j = pLeft;
        if (j < pathLen - 1 && isPointInMetaEntry(meta, pathData, j, absoluteX, y, minHitWidth)) {
            return blockFromMeta(meta, pathData);
        }
    }
    return null;
};

const searchBlockSnapInBatch = (
    batchData: BatchData,
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
    batchIndex: number,
): Array<{ meta: BlockMeta; pathData: Float32Array; distance: number; order: number }> => {
    const { metas, pathData } = batchData;
    const snapStart = absoluteX - snapHitWidth;
    const snapEnd = absoluteX + snapHitWidth;
    const candidates: Array<{ meta: BlockMeta; pathData: Float32Array; distance: number; order: number }> = [];
    for (let metaIndex = 0; metaIndex < metas.length; metaIndex++) {
        const meta = metas[metaIndex];
        if (meta.pathEndTimestamp < snapStart || meta.pathStartTimestamp > snapEnd || meta.pathLength <= 1) {
            continue;
        }
        let left = 0;
        let right = meta.pathLength - 1;
        while (left < right) {
            const middle = Math.floor((left + right) / 2);
            const timestamp = pathData[(meta.pathOffset + middle) * 2];
            if (timestamp < snapStart) {
                left = middle + 1;
            } else {
                right = middle;
            }
        }
        const firstSegment = Math.max(0, left - 1);
        for (let pathIndex = firstSegment; pathIndex < meta.pathLength - 1; pathIndex++) {
            const segmentStart = pathData[(meta.pathOffset + pathIndex) * 2];
            if (segmentStart > snapEnd) {
                break;
            }
            const candidate = getMetaSnapCandidate(meta, pathData, pathIndex, absoluteX, y, minHitWidth, snapHitWidth);
            if (candidate !== null) {
                candidates.push({
                    meta: candidate.meta,
                    pathData,
                    distance: candidate.distance,
                    order: batchIndex * 1000000 + metaIndex * 1000 + candidate.order,
                });
            }
        }
    }
    return candidates;
};

export const searchBlockDataByPointFromOPFS = async (
    blockDataOPFS: BlockDataOPFS,
    { clientX, clientY }: Omit<HoverItemPayload, 'type'>,
    transform: RenderOptions['transform'],
    zoom: RenderOptions['zoom'],
): Promise<Block | null> => {
    const x = (clientX - transform.x) / zoom.x / getTransformScaleX(transform);
    const y = (clientY - transform.y) / zoom.y / getTransformScaleY(transform);
    const absoluteX = x + zoom.offset;
    const scaleX = getTransformScaleX(transform);
    const minHitWidth = zoom.x * scaleX > 0 ? 1 / zoom.x / scaleX : 0;
    const snapHitWidth = zoom.x * scaleX > 0 ? BLOCK_SNAP_TARGET_WIDTH_PX / zoom.x / scaleX : 0;

    const batchIndices = blockDataOPFS.findBatchesByTimestamp(absoluteX);
    for (const batchIndex of batchIndices) {
        const batchData = await blockDataOPFS.readBatchForHitTest(batchIndex);
        if (!batchData) {
            continue;
        }
        const result = searchBlockDataInBatch(batchData, x, y, zoom, minHitWidth);
        if (result) {
            return result;
        }
    }

    if (snapHitWidth <= minHitWidth) {
        return null;
    }

    const snapStart = absoluteX - snapHitWidth;
    const snapEnd = absoluteX + snapHitWidth;
    const snapBatchIndices = blockDataOPFS.findBatchesOverlappingRange(snapStart, snapEnd);
    const snapCandidates: Array<{ meta: BlockMeta; pathData: Float32Array; distance: number; order: number }> = [];
    for (const batchIndex of snapBatchIndices) {
        const batchData = await blockDataOPFS.readBatchForHitTest(batchIndex);
        if (!batchData) {
            continue;
        }
        snapCandidates.push(...searchBlockSnapInBatch(batchData, absoluteX, y, minHitWidth, snapHitWidth, batchIndex));
    }
    return pickUniqueSnapMeta(snapCandidates);
};
// 射线法计算点是否在四边形（矩形/平行四边形）范围内
const isPointInExtrudedSegment = (px: number, py: number, sx1: number, sy1: number, sx2: number, sy2: number, h: number): boolean => {
    const p0 = [sx1, sy1];
    const p1 = [sx1, sy1 + h];
    const p2 = [sx2, sy2 + h];
    const p3 = [sx2, sy2];
    const points = [p0, p1, p2, p3];
    let inside = false;
    for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
        const xi = points[i][0];
        const yi = points[i][1];
        const xj = points[j][0];
        const yj = points[j][1];
        const intersect = ((yi > py) !== (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
};

enum MemoryEventAction {
    Malloc = 0,
    Free = 1,
};
interface MemoryEvent {
    eventAction: MemoryEventAction;
    time: number;
    blockPtr: Block;
}

const addPathPoint = (block: Block, time: number, size: number): void => {
    if (block.path === undefined || block.path.length === 0) {
        block.path = [[time, size]];
        return;
    }
    const lastPoint = block.path[block.path.length - 1];
    // 如果新加入点点时间戳在最后一个点之前，视为无效点
    if (time < lastPoint[0]) {
        return;
    }
    // 如果目前只有一个点，直接添加
    if (block.path.length === 1) {
        block.path.push([time, size]);
        return;
    }
    // 检查是否可以压缩水平线段（三个连续点size相同)
    const secondLastPoint = block.path[block.path.length - 2];
    if (size === lastPoint[1] && size === secondLastPoint[1]) {
        // 合并：将最后一个点的timestamp更新为新的timestamp
        lastPoint[0] = time;
        return;
    }
    // 无法合并则正常添加新点
    block.path.push([time, size]);
};

const DEFAULT_MAX_PATH_POINTS = 4000000;
const DEFAULT_MAX_PATH_POINTS_PER_BLOCK = 8192;
const BUILD_YIELD_INTERVAL = 4096;
const BUILD_YIELD_BUDGET_MS = 8;
const PROGRESSIVE_BOOTSTRAP_FRAGMENT_LIMIT = 64;
const PROGRESSIVE_BOOTSTRAP_PATH_POINT_LIMIT = 8192;

export class BlockPathBuildCancelledError extends Error {
    constructor() {
        super('Block path build cancelled');
        this.name = 'BlockPathBuildCancelledError';
    }
}

export interface BlockPathBuildProgress {
    processedEventCount: number;
    totalEventCount: number;
}

export interface BlockPathBuildOptions {
    generation?: number;
    maxPathPoints?: number;
    maxPathPointsPerBlock?: number;
    shouldCancel?: () => boolean;
    onGraphMetadataReady?: (metadata: BlockGraphMetadata) => void | Promise<void>;
    onStorageReady?: () => void | Promise<void>;
    onBatchesCommitted?: (
        startBatch: number,
        endBatch: number,
        progress: BlockPathBuildProgress,
    ) => void | Promise<void>;
}

class CompactBlockPath implements PackedBlockPath {
    private pathChunks: Float32Array[] = [];
    private pointCount: number = 0;
    private currentChunk: Float32Array | null = null;
    private currentChunkOffset: number = 0;
    private lastPointChunk: Float32Array | null = null;
    private lastPointOffset: number = 0;
    private lastX: number = 0;
    private firstX: number = 0;
    private lastY: number = 0;
    private secondLastY: number = 0;

    get chunks(): readonly Float32Array[] {
        return this.pathChunks;
    }

    get pathLength(): number {
        return this.pointCount;
    }

    get pathStartTimestamp(): number {
        return this.firstX;
    }

    get pathEndTimestamp(): number {
        return this.lastX;
    }

    addPoint(time: number, size: number): void {
        if (this.pointCount > 0 && time < this.lastX) {
            return;
        }
        if (this.pointCount === 0) {
            this.pushPoint(time, size);
            return;
        }
        if (this.pointCount > 1 && size === this.lastY && size === this.secondLastY) {
            if (this.lastPointChunk) {
                this.lastPointChunk[this.lastPointOffset] = time;
            }
            this.lastX = time;
            return;
        }
        this.pushPoint(time, size);
    }

    drainFragment(): PackedBlockPath | null {
        if (this.pointCount < 2) {
            return null;
        }
        const fragment = this.detach();
        this.pushPoint(this.lastX, this.lastY);
        return fragment;
    }

    detach(): PackedBlockPath {
        const fragment: PackedBlockPath = {
            chunks: this.pathChunks,
            pathLength: this.pointCount,
            pathStartTimestamp: this.firstX,
            pathEndTimestamp: this.lastX,
        };
        this.pathChunks = [];
        this.pointCount = 0;
        this.currentChunk = null;
        this.currentChunkOffset = 0;
        this.lastPointChunk = null;
        this.lastPointOffset = 0;
        this.secondLastY = this.lastY;
        return fragment;
    }

    private pushPoint(time: number, size: number): void {
        if (this.currentChunk === null || this.currentChunkOffset + 2 > this.currentChunk.length) {
            const previousSize = this.currentChunk?.length ?? 2;
            this.currentChunk = new Float32Array(Math.min(previousSize * 2, 8192));
            this.currentChunkOffset = 0;
            this.pathChunks.push(this.currentChunk);
        }
        this.currentChunk[this.currentChunkOffset] = time;
        this.currentChunk[this.currentChunkOffset + 1] = size;
        this.lastPointChunk = this.currentChunk;
        this.lastPointOffset = this.currentChunkOffset;
        this.currentChunkOffset += 2;
        this.secondLastY = this.pointCount === 0 ? size : this.lastY;
        if (this.pointCount === 0) {
            this.firstX = time;
        }
        this.lastX = time;
        this.lastY = size;
        this.pointCount++;
    }

    getLastSize(): number {
        return this.pointCount === 0 ? 0 : this.lastY;
    }
}

interface BlockSource {
    blockCount: number;
    transferBytes: number;
    reservedLine?: Array<[number, number]>;
    reservedSizeMax?: number;
    getId: (index: number) => number;
    getAddress: (index: number) => string;
    getStart: (index: number) => number;
    getEnd: (index: number) => number;
    getSize: (index: number) => number;
    release: (index: number) => void;
}

const createBlockSource = (data: RenderData | PackedRenderData): BlockSource => {
    if (isPackedRenderData(data)) {
        return {
            blockCount: data.ids.length,
            transferBytes: data.transferBytes,
            reservedLine: data.reservedLine,
            reservedSizeMax: data.reservedSizeMax,
            getId: index => data.ids[index],
            getAddress: index => data.addresses[data.addressIndices[index]] ?? '',
            getStart: index => data.startTimestamps[index],
            getEnd: index => data.endTimestamps[index],
            getSize: index => data.sizes[index],
            release: () => undefined,
        };
    }
    const blocks = data.blocks;
    data.blocks = [];
    return {
        blockCount: blocks.length,
        transferBytes: 0,
        reservedLine: data.reservedLine,
        reservedSizeMax: data.reservedSizeMax,
        getId: index => blocks[index].id,
        getAddress: index => blocks[index].addr,
        getStart: index => blocks[index]._startTimestamp,
        getEnd: index => blocks[index]._endTimestamp,
        getSize: index => blocks[index].size,
        release: index => {
            blocks[index] = null as unknown as Block;
        },
    };
};

export const getInitialBlockGraphMetadata = (data: RenderData | PackedRenderData): BlockGraphMetadata => ({
    maxTimestamp: data.maxTimestamp,
    minTimestamp: data.minTimestamp,
    maxSize: data.maxSize,
    minSize: data.minSize,
    batchCount: 0,
    reservedLine: data.reservedLine,
    reservedSizeMax: data.reservedSizeMax,
});

let yieldChannel: MessageChannel | undefined;
const channelYieldResolvers: Array<() => void> = [];

const yieldThroughMessageChannel = async (): Promise<void> => {
    if (yieldChannel === undefined) {
        yieldChannel = new MessageChannel();
        yieldChannel.port1.onmessage = (): void => {
            channelYieldResolvers.shift()?.();
        };
        (yieldChannel.port1 as MessagePort & { unref?: () => void }).unref?.();
        (yieldChannel.port2 as MessagePort & { unref?: () => void }).unref?.();
    }
    await new Promise<void>(resolve => {
        channelYieldResolvers.push(resolve);
        yieldChannel?.port2.postMessage(undefined);
    });
};

const yieldToWorkerEventLoop = async (): Promise<void> => {
    const scheduler = (globalThis as typeof globalThis & {
        scheduler?: { yield?: () => Promise<void> };
    }).scheduler;
    if (scheduler?.yield) {
        await scheduler.yield();
        return;
    }
    if (typeof MessageChannel !== 'undefined') {
        await yieldThroughMessageChannel();
        return;
    }
    await new Promise<void>(resolve => setTimeout(resolve, 0));
};

export const buildBlockViewPathAndWriteToOPFS = async (
    blockView: RenderData | PackedRenderData,
    blockDataOPFS: BlockDataOPFS,
    options: BlockPathBuildOptions = {},
): Promise<BlockGraphMetadata> => {
    const startedAt = getNow();
    const source = createBlockSource(blockView);
    const { blockCount, reservedLine, reservedSizeMax } = source;
    const generation = options.generation ?? 0;
    const assertNotCancelled = (): void => {
        if (options.shouldCancel?.()) {
            throw new BlockPathBuildCancelledError();
        }
    };
    if (blockCount === 0) {
        const emptyMetadata: BlockGraphMetadata = {
            maxTimestamp: 0,
            minTimestamp: 0,
            maxSize: 0,
            minSize: 0,
            batchCount: 0,
            reservedLine,
            reservedSizeMax,
        };
        await options.onGraphMetadataReady?.(emptyMetadata);
        assertNotCancelled();
        await blockDataOPFS.clear();
        assertNotCancelled();
        await options.onStorageReady?.();
        assertNotCancelled();
        const metrics: BlockGraphBuildMetrics = {
            generation,
            blockCount: 0,
            eventCount: 0,
            maxActiveBlocks: 0,
            peakActivePathPoints: 0,
            pathPoints: 0,
            droppedPathPoints: 0,
            batchCount: 0,
            opfsPathBytes: 0,
            transferBytes: source.transferBytes,
            buildDurationMs: Number((getNow() - startedAt).toFixed(2)),
            lodApplied: false,
        };
        return { ...emptyMetadata, metrics };
    }

    const eventCount = blockCount * 2;
    const sortedEvents = new Uint32Array(eventCount);
    for (let blockIndex = 0; blockIndex < blockCount; blockIndex++) {
        sortedEvents[blockIndex * 2] = blockIndex * 2 + MemoryEventAction.Malloc;
        sortedEvents[blockIndex * 2 + 1] = blockIndex * 2 + MemoryEventAction.Free;
    }
    const getEventTime = (event: number): number => event % 2 === MemoryEventAction.Malloc
        ? source.getStart(Math.floor(event / 2))
        : source.getEnd(Math.floor(event / 2));
    sortedEvents.sort((a, b) => getEventTime(a) - getEventTime(b) || a - b);

    const maxTimestamp = getEventTime(sortedEvents[eventCount - 1]);
    const minTimestamp = getEventTime(sortedEvents[0]);
    const minSize = 0;
    let currentPreviewSize = 0;
    let maxSize = 0;
    let previewSliceStartedAt = getNow();
    for (let eventIndex = 0; eventIndex < eventCount; eventIndex++) {
        if (eventIndex > 0 && eventIndex % BUILD_YIELD_INTERVAL === 0 &&
            getNow() - previewSliceStartedAt >= BUILD_YIELD_BUDGET_MS) {
            await yieldToWorkerEventLoop();
            assertNotCancelled();
            previewSliceStartedAt = getNow();
        }
        const event = sortedEvents[eventIndex];
        const blockIndex = Math.floor(event / 2);
        if (event % 2 === MemoryEventAction.Malloc) {
            currentPreviewSize += source.getSize(blockIndex);
            maxSize = Math.max(maxSize, currentPreviewSize);
        } else {
            currentPreviewSize -= source.getSize(blockIndex);
        }
    }
    await options.onGraphMetadataReady?.({
        maxTimestamp,
        minTimestamp,
        maxSize,
        minSize,
        batchCount: 0,
        reservedLine,
        reservedSizeMax,
    });
    assertNotCancelled();

    const previousBlock = new Int32Array(blockCount);
    const nextBlock = new Int32Array(blockCount);
    previousBlock.fill(-1);
    nextBlock.fill(-1);
    const paths = new Array<CompactBlockPath | null>(blockCount).fill(null);
    const maxActivePathPoints = Math.max(blockCount, options.maxPathPoints ?? DEFAULT_MAX_PATH_POINTS);
    const maxPointsPerBlock = Math.max(2, Math.min(
        options.maxPathPointsPerBlock ?? DEFAULT_MAX_PATH_POINTS_PER_BLOCK,
        Math.floor(blockDataOPFS.getMaxBatchPathFloats() / 2),
    ));
    let topBlock = -1;
    let activeBlockCount = 0;
    let maxActiveBlocks = 0;
    let activePathPoints = 0;
    let peakActivePathPoints = 0;
    let persistedPathPoints = 0;
    let currentTotalSize = 0;
    let operationCount = 0;
    let pathSliceStartedAt = getNow();
    let bootstrapFragmentCount = 0;
    let bootstrapPathPoints = 0;
    let processedEventCount = 0;

    await blockDataOPFS.clear();
    assertNotCancelled();
    await options.onStorageReady?.();
    assertNotCancelled();

    const notifyCommittedBatches = async (previousBatchCount: number): Promise<void> => {
        const nextBatchCount = blockDataOPFS.getBatchCount();
        if (nextBatchCount <= previousBatchCount) {
            return;
        }
        await options.onBatchesCommitted?.(previousBatchCount, nextBatchCount, {
            processedEventCount,
            totalEventCount: eventCount,
        });
        assertNotCancelled();
    };
    const createBlock = (blockIndex: number): Block => ({
        id: source.getId(blockIndex),
        addr: source.getAddress(blockIndex),
        _startTimestamp: source.getStart(blockIndex),
        _endTimestamp: source.getEnd(blockIndex),
        size: source.getSize(blockIndex),
        path: [],
    });
    const writePathFragment = async (blockIndex: number, fragment: PackedBlockPath): Promise<void> => {
        const previousBatchCount = blockDataOPFS.getBatchCount();
        await blockDataOPFS.addPackedBlock(createBlock(blockIndex), fragment);
        persistedPathPoints += fragment.pathLength;
        if (previousBatchCount === 0 && blockDataOPFS.getBatchCount() === 0) {
            bootstrapFragmentCount++;
            bootstrapPathPoints += fragment.pathLength;
            if (bootstrapFragmentCount >= PROGRESSIVE_BOOTSTRAP_FRAGMENT_LIMIT ||
                bootstrapPathPoints >= PROGRESSIVE_BOOTSTRAP_PATH_POINT_LIMIT) {
                await blockDataOPFS.flush();
            }
        }
        await notifyCommittedBatches(previousBatchCount);
    };
    const flushActivePath = async (blockIndex: number, path: CompactBlockPath): Promise<void> => {
        const previousLength = path.pathLength;
        const fragment = path.drainFragment();
        if (!fragment) {
            return;
        }
        activePathPoints -= previousLength - path.pathLength;
        await writePathFragment(blockIndex, fragment);
    };
    const addPathPoint = (
        blockIndex: number,
        path: CompactBlockPath,
        time: number,
        size: number,
        flushAllowed: boolean = true,
    ): Promise<void> | null => {
        const previousLength = path.pathLength;
        path.addPoint(time, size);
        activePathPoints += path.pathLength - previousLength;
        peakActivePathPoints = Math.max(peakActivePathPoints, activePathPoints);
        if (flushAllowed && path.pathLength >= 2 &&
            (path.pathLength >= maxPointsPerBlock || activePathPoints >= maxActivePathPoints)) {
            return flushActivePath(blockIndex, path);
        }
        return null;
    };
    const writeBlock = async (blockIndex: number, path: CompactBlockPath): Promise<void> => {
        const pathLength = path.pathLength;
        const fragment = path.detach();
        if (fragment.pathLength > 0) {
            await writePathFragment(blockIndex, fragment);
        }
        activePathPoints -= pathLength;
        paths[blockIndex] = null;
        source.release(blockIndex);
    };

    for (let eventIndex = 0; eventIndex < eventCount; eventIndex++) {
        if (++operationCount % BUILD_YIELD_INTERVAL === 0 &&
            getNow() - pathSliceStartedAt >= BUILD_YIELD_BUDGET_MS) {
            await yieldToWorkerEventLoop();
            assertNotCancelled();
            pathSliceStartedAt = getNow();
        }
        const event = sortedEvents[eventIndex];
        const eventAction = event % 2;
        const blockIndex = Math.floor(event / 2);
        const time = getEventTime(event);
        if (eventAction === MemoryEventAction.Malloc) {
            const path = new CompactBlockPath();
            paths[blockIndex] = path;
            previousBlock[blockIndex] = topBlock;
            if (topBlock >= 0) {
                nextBlock[topBlock] = blockIndex;
            }
            topBlock = blockIndex;
            activeBlockCount++;
            maxActiveBlocks = Math.max(maxActiveBlocks, activeBlockCount);
            const flushPromise = addPathPoint(blockIndex, path, time, currentTotalSize);
            if (flushPromise) {
                await flushPromise;
            }
            currentTotalSize += source.getSize(blockIndex);
            maxSize = currentTotalSize > maxSize ? currentTotalSize : maxSize;
            processedEventCount = eventIndex + 1;
            continue;
        }
        const freeSize = source.getSize(blockIndex);
        let activeIndex = topBlock;
        while (activeIndex >= 0) {
            if (++operationCount % BUILD_YIELD_INTERVAL === 0 &&
                getNow() - pathSliceStartedAt >= BUILD_YIELD_BUDGET_MS) {
                await yieldToWorkerEventLoop();
                assertNotCancelled();
                pathSliceStartedAt = getNow();
            }
            const path = paths[activeIndex];
            if (!path) {
                break;
            }
            const lastSize = path.getLastSize();
            const isFreedBlock = activeIndex === blockIndex;
            const firstFlush = addPathPoint(activeIndex, path, time, lastSize, !isFreedBlock);
            if (firstFlush) {
                await firstFlush;
            }
            if (isFreedBlock) {
                break;
            }
            const secondFlush = addPathPoint(activeIndex, path, time + 1, lastSize - freeSize);
            if (secondFlush) {
                await secondFlush;
            }
            activeIndex = previousBlock[activeIndex];
        }
        if (activeIndex < 0) {
            processedEventCount = eventIndex + 1;
            continue;
        }
        currentTotalSize -= freeSize;
        const lowerBlock = previousBlock[blockIndex];
        const upperBlock = nextBlock[blockIndex];
        if (lowerBlock >= 0) {
            nextBlock[lowerBlock] = upperBlock;
        }
        if (upperBlock >= 0) {
            previousBlock[upperBlock] = lowerBlock;
        } else {
            topBlock = lowerBlock;
        }
        activeBlockCount--;
        const freedPath = paths[blockIndex];
        if (freedPath) {
            await writeBlock(blockIndex, freedPath);
        }
        processedEventCount = eventIndex + 1;
    }

    processedEventCount = eventCount;

    let activeIndex = topBlock;
    while (activeIndex >= 0) {
        const lowerBlock = previousBlock[activeIndex];
        const path = paths[activeIndex];
        currentTotalSize -= source.getSize(activeIndex);
        if (path) {
            addPathPoint(activeIndex, path, maxTimestamp, currentTotalSize, false);
            await writeBlock(activeIndex, path);
        }
        activeIndex = lowerBlock;
    }

    assertNotCancelled();
    const previousBatchCount = blockDataOPFS.getBatchCount();
    await blockDataOPFS.flush();
    await notifyCommittedBatches(previousBatchCount);
    const batchCount = blockDataOPFS.getBatchCount();
    const metrics: BlockGraphBuildMetrics = {
        generation,
        blockCount,
        eventCount,
        maxActiveBlocks,
        peakActivePathPoints,
        pathPoints: persistedPathPoints,
        droppedPathPoints: 0,
        batchCount,
        opfsPathBytes: blockDataOPFS.getStoredPathBytes(),
        transferBytes: source.transferBytes,
        buildDurationMs: Number((getNow() - startedAt).toFixed(2)),
        lodApplied: false,
    };
    tracePerf('buildBlockViewPathAndWriteToOPFS', startedAt, {
        batchCount,
        pathPoints: metrics.pathPoints,
        droppedPathPoints: metrics.droppedPathPoints,
    });
    return {
        maxTimestamp,
        minTimestamp,
        maxSize,
        minSize,
        batchCount,
        reservedLine,
        reservedSizeMax,
        metrics,
    };
};

export const buildBlockViewPath = (blockView: RenderData): RenderData => {
    const cachedData = blockViewPathCache.get(blockView);
    if (cachedData !== undefined) {
        return cachedData;
    }
    const startedAt = getNow();
    if (blockView.blocks === undefined || blockView.blocks.length === 0) {
        blockViewPathCache.set(blockView, blockView);
        return blockView;
    }
    // 将已基于开始时间排序的block数组，还原成基于开始、结束时间构造的事件
    const sortedEvents: MemoryEvent[] = [];
    for (const block of blockView.blocks) {
        sortedEvents.push(
            { eventAction: MemoryEventAction.Malloc, time: block._startTimestamp, blockPtr: block },
            { eventAction: MemoryEventAction.Free, time: block._endTimestamp, blockPtr: block },
        );
    }
    sortedEvents.sort((a, b) => a.time - b.time);

    const currentBlocks: Block[] = [];
    let currentTotalSize = 0;
    blockView.maxTimestamp = sortedEvents[sortedEvents.length - 1].time;
    blockView.minTimestamp = sortedEvents[0].time;
    blockView.minSize = 0;

    sortedEvents.forEach(({ eventAction, time, blockPtr }, index) => {
        // 如果是分配事件
        if (eventAction === MemoryEventAction.Malloc) {
            currentBlocks.push(blockPtr);
            addPathPoint(blockPtr, time, currentTotalSize);
            currentTotalSize += blockPtr.size;
            blockView.maxSize = currentTotalSize > blockView.maxSize ? currentTotalSize : blockView.maxSize;
            return;
        }
        // 否则为释放事件, 需要在currentBlocks中找到被释放的块，根据内存分配时的特征，使用倒序查找更合适
        let freeBlockIdx = -1;
        const freeSize = blockPtr.size;
        for (let i = currentBlocks.length - 1; i >= 0; i--) {
            const block = currentBlocks[i];
            // 取block.path路径点中最后一次的高度，保持不变的插入一个点
            const lastPoint = block.path?.[block.path.length - 1] ?? [0, 0];
            addPathPoint(block, time, lastPoint[1]);
            if (block.id === blockPtr.id) {
                freeBlockIdx = i;
                break;
            }
            // 非此次释放块，在1个时间步之后插入新的下落点
            addPathPoint(block, time + 1, lastPoint[1] - freeSize);
        }
        if (freeBlockIdx < 0) {
            // 查找失败，报错返回
            return;
        }
        currentTotalSize -= freeSize;
        currentBlocks.splice(freeBlockIdx, 1);
    });
    // 处理剩余块
    for (let i = currentBlocks.length - 1; i >= 0; i--) {
        currentTotalSize -= currentBlocks[i].size;
        addPathPoint(currentBlocks[i], blockView.maxTimestamp, currentTotalSize);
    }
    blockViewPathCache.set(blockView, blockView);
    tracePerf('buildBlockViewPath', startedAt, {
        blocks: blockView.blocks.length,
    });
    return blockView;
};

let X_GAP = 20;
const Y_GAP = 20;
const LINE_HEIGHT = 40;
export const getMemoryStateRenderData = (data: Segment[]): Segment[] => {
    const cachedData = stateRenderDataCache.get(data);
    if (cachedData !== undefined) {
        return cachedData;
    }
    const startedAt = getNow();
    if (data.length < 1) {
        return [];
    }
    const lastSegment = data[data.length - 1];
    X_GAP = Math.max(Math.round(lastSegment.size / 100), 20); // segment间的间隔取最长行的1/100
    const maxSizeX = lastSegment.size + X_GAP * 2; // 额外增加宽度，避免定格绘制
    const stateRenderData: Segment[] = [];
    let currentRow = 0;
    let currentRowSum = X_GAP; // 当前行总长
    for (let i = 0; i < data.length; i++) {
        const segment = data[i];
        if (segment.size + currentRowSum + X_GAP <= maxSizeX) {
            segment.offsetX = currentRowSum;
            segment.offsetY = currentRow * (LINE_HEIGHT + Y_GAP) + Y_GAP;
            stateRenderData.push(segment);
            currentRowSum += segment.size + X_GAP;
        } else {
            currentRow++;
            segment.offsetX = X_GAP;
            segment.offsetY = currentRow * (LINE_HEIGHT + Y_GAP) + Y_GAP;
            stateRenderData.push(segment);
            currentRowSum = segment.size + X_GAP * 2;
        }
    }
    stateRenderDataCache.set(data, stateRenderData);
    tracePerf('getMemoryStateRenderData', startedAt, {
        segments: stateRenderData.length,
    });
    return stateRenderData;
};

const isSameStateRow = (segment: Segment, y: number): boolean => y >= segment.offsetY && y <= segment.offsetY + LINE_HEIGHT;

const getStateBlockSnapCandidate = (
    block: StateBlock,
    blockIndex: number,
    localX: number,
    minHitWidth: number,
    snapHitWidth: number,
): { block: StateBlock; distance: number; order: number } | null => {
    const visibleStart = block.offset;
    const visibleEnd = Math.max(block.offset + block.size, block.offset + minHitWidth);
    const visibleWidth = visibleEnd - visibleStart;
    if (snapHitWidth <= minHitWidth || visibleWidth >= snapHitWidth || (localX >= visibleStart && localX <= visibleEnd)) {
        return null;
    }
    const extraWidth = (snapHitWidth - visibleWidth) / 2;
    if (localX < visibleStart - extraWidth || localX > visibleEnd + extraWidth) {
        return null;
    }
    return {
        block: { ...block, colorIndex: blockIndex },
        distance: Math.min(Math.abs(localX - visibleStart), Math.abs(localX - visibleEnd)),
        order: blockIndex,
    };
};

const searchStateDataDynamically = (
    data: Segment[],
    x: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): StateDataHoverResult | null => {
    const segmentSnapCandidates: Array<{ segment: Segment; distance: number; order: number }> = [];
    for (let segmentIndex = 0; segmentIndex < data.length; segmentIndex++) {
        const segment = data[segmentIndex];
        if (!isSameStateRow(segment, y)) {
            continue;
        }
        const segmentEnd = Math.max(segment.offsetX + segment.size, segment.offsetX + minHitWidth);
        if (x >= segment.offsetX && x <= segmentEnd) {
            const localX = x - segment.offsetX;
            for (let blockIndex = 0; blockIndex < segment.blocks.length; blockIndex++) {
                const block = segment.blocks[blockIndex];
                const blockEnd = Math.max(block.offset + block.size, block.offset + minHitWidth);
                if (localX >= block.offset && localX <= blockEnd) {
                    const { blocks, ...newSegment } = segment;
                    return { type: 'block', data: { ...newSegment, blocks: [{ ...block, colorIndex: blockIndex }] } };
                }
            }
            const { blocks, ...newSegment } = segment;
            return { type: 'segment', data: { ...newSegment, blocks: [] } };
        }
        if (snapHitWidth <= minHitWidth) {
            continue;
        }
        const visibleWidth = segmentEnd - segment.offsetX;
        if (visibleWidth >= snapHitWidth) {
            continue;
        }
        const extraWidth = (snapHitWidth - visibleWidth) / 2;
        if (x >= segment.offsetX - extraWidth && x <= segmentEnd + extraWidth) {
            segmentSnapCandidates.push({
                segment,
                distance: Math.min(Math.abs(x - segment.offsetX), Math.abs(x - segmentEnd)),
                order: segmentIndex,
            });
        }
    }
    if (segmentSnapCandidates.length !== 1) {
        return null;
    }
    const segment = segmentSnapCandidates[0].segment;
    const localX = x - segment.offsetX;
    const blockSnapCandidates: Array<{ block: StateBlock; distance: number; order: number }> = [];
    for (let blockIndex = 0; blockIndex < segment.blocks.length; blockIndex++) {
        const candidate = getStateBlockSnapCandidate(segment.blocks[blockIndex], blockIndex, localX, minHitWidth, snapHitWidth);
        if (candidate !== null) {
            blockSnapCandidates.push(candidate);
        }
    }
    const { blocks, ...newSegment } = segment;
    if (blockSnapCandidates.length === 1) {
        return { type: 'block', data: { ...newSegment, blocks: [blockSnapCandidates[0].block] } };
    }
    return { type: 'segment', data: { ...newSegment, blocks: [] } };
};

export const getMemoryStateZoom = (data: Segment[], canvas: OffscreenCanvas | HTMLCanvasElement): RenderOptions['zoom'] => {
    if (data.length < 1) {
        return { x: 1, y: 1, offset: 0 };
    }
    const lastSegment = data[data.length - 1];
    const maxSizeX = lastSegment.size + X_GAP * 2; // 最长的行，额外增加宽度，避免定格绘制

    const maxSizeY = lastSegment.offsetY + LINE_HEIGHT + Y_GAP;
    return {
        x: canvas.width / maxSizeX,
        y: canvas.height / maxSizeY,
        offset: 0, // 在状态图中没有意义
    };
};

export const searchStateDataByPoint = (
    data: Segment[],
    { clientX, clientY }: Omit<HoverItemPayload, 'type'>,
    transform: RenderOptions['transform'],
    zoom: RenderOptions['zoom'],
): StateDataHoverResult | null => {
    const x = (clientX - transform.x) / zoom.x / getTransformScaleX(transform);
    const y = (clientY - transform.y) / zoom.y / getTransformScaleY(transform);
    const scaleX = getTransformScaleX(transform);
    const minHitWidth = zoom.x * scaleX > 0 ? 1 / zoom.x / scaleX : 0;
    const snapHitWidth = zoom.x * scaleX > 0 ? BLOCK_SNAP_TARGET_WIDTH_PX / zoom.x / scaleX : 0;
    return searchStateDataDynamically(data, x, y, minHitWidth, snapHitWidth);
};

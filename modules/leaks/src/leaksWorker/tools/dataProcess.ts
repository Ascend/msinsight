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

export const getZoom = (data: RenderData, canvas: OffscreenCanvas | HTMLCanvasElement): RenderOptions['zoom'] => {
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

export const buildBlockViewPath = (blockView: SetMemoryBlocksDataPayload['data']): RenderData => {
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

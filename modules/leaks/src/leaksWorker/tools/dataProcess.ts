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
    return {
        x: canvas.width / (data.maxTimestamp - data.minTimestamp),
        y: canvas.height / (data.maxSize - data.minSize),
        offset: data.minTimestamp,
    };
};

const getTransformScaleX = (transform: RenderOptions['transform']): number => transform.scaleX ?? transform.scale;
const getTransformScaleY = (transform: RenderOptions['transform']): number => transform.scaleY ?? transform.scale;

interface BlockHitEntry {
    blockIndex: number;
    pathIndex: number;
    start: number;
    end: number;
    minY: number;
    maxY: number;
}

interface BlockHitIndex {
    minTime: number;
    maxTime: number;
    bucketSize: number;
    buckets: number[][];
    wideEntries: number[];
    entries: BlockHitEntry[];
}

interface IndexedRenderData extends RenderData {
    blockHitIndex?: BlockHitIndex;
}

interface StateHitRow {
    yStart: number;
    yEnd: number;
    segments: number[];
}

interface StateHitIndex {
    rows: StateHitRow[];
    blockOrders: number[][];
}

const BLOCK_HIT_TARGET_BUCKET_SIZE = 64;
const BLOCK_HIT_MAX_BUCKET_COUNT = 2048;
const BLOCK_HIT_MAX_BUCKET_SPAN = 128;
const BLOCK_SNAP_TARGET_WIDTH_PX = 6;
const blockViewPathCache = new WeakMap<SetMemoryBlocksDataPayload['data'], RenderData>();
const stateRenderDataCache = new WeakMap<Segment[], Segment[]>();
const stateHitIndexCache = new WeakMap<Segment[], StateHitIndex>();

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

const getBucketIndex = (time: number, index: BlockHitIndex): number => {
    if (index.buckets.length <= 1 || index.bucketSize <= 0) {
        return 0;
    }
    const bucketIndex = Math.floor((time - index.minTime) / index.bucketSize);
    return Math.max(0, Math.min(index.buckets.length - 1, bucketIndex));
};

const sortBlockHitEntries = (hitIndex: BlockHitIndex, entryIndexes: Iterable<number>): number[] => {
    return Array.from(entryIndexes).sort((left, right) => {
        const leftEntry = hitIndex.entries[left];
        const rightEntry = hitIndex.entries[right];
        return leftEntry.blockIndex - rightEntry.blockIndex || leftEntry.pathIndex - rightEntry.pathIndex;
    });
};

const collectBlockHitEntryIndexes = (hitIndex: BlockHitIndex, startTime: number, endTime: number): number[] => {
    const candidateEntryIndexes = new Set<number>();
    const startBucket = getBucketIndex(startTime, hitIndex);
    const endBucket = getBucketIndex(endTime, hitIndex);
    for (let bucketIndex = startBucket; bucketIndex <= endBucket; bucketIndex++) {
        for (const entryIndex of hitIndex.buckets[bucketIndex]) {
            candidateEntryIndexes.add(entryIndex);
        }
    }
    for (const entryIndex of hitIndex.wideEntries) {
        candidateEntryIndexes.add(entryIndex);
    }
    return sortBlockHitEntries(hitIndex, candidateEntryIndexes);
};

const buildBlockHitIndex = (renderData: RenderData): BlockHitIndex | undefined => {
    const { blocks, minTimestamp, maxTimestamp } = renderData;
    if (blocks.length < 1 || maxTimestamp <= minTimestamp) {
        return undefined;
    }
    const entries: BlockHitEntry[] = [];
    for (let blockIndex = 0; blockIndex < blocks.length; blockIndex++) {
        const block = blocks[blockIndex];
        for (let pathIndex = 0; pathIndex < block.path.length - 1; pathIndex++) {
            const startPoint = block.path[pathIndex];
            const endPoint = block.path[pathIndex + 1];
            entries.push({
                blockIndex,
                pathIndex,
                start: Math.min(startPoint[0], endPoint[0]),
                end: Math.max(startPoint[0], endPoint[0]),
                minY: Math.min(startPoint[1], endPoint[1]),
                maxY: Math.max(startPoint[1], endPoint[1]) + block.size,
            });
        }
    }
    if (entries.length < 1) {
        return undefined;
    }
    const bucketCount = Math.max(1, Math.min(BLOCK_HIT_MAX_BUCKET_COUNT, Math.ceil(entries.length / BLOCK_HIT_TARGET_BUCKET_SIZE)));
    const index: BlockHitIndex = {
        minTime: minTimestamp,
        maxTime: maxTimestamp,
        bucketSize: (maxTimestamp - minTimestamp) / bucketCount,
        buckets: Array.from({ length: bucketCount }, () => []),
        wideEntries: [],
        entries,
    };
    entries.forEach((entry, entryIndex) => {
        const startBucket = getBucketIndex(entry.start, index);
        const endBucket = getBucketIndex(entry.end, index);
        if (endBucket - startBucket > BLOCK_HIT_MAX_BUCKET_SPAN) {
            index.wideEntries.push(entryIndex);
            return;
        }
        for (let bucketIndex = startBucket; bucketIndex <= endBucket; bucketIndex++) {
            index.buckets[bucketIndex].push(entryIndex);
        }
    });
    return index;
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

const searchBlockDataWithIndex = (
    blocks: RenderData['blocks'],
    hitIndex: BlockHitIndex,
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): Block | null => {
    const exactCandidates = collectBlockHitEntryIndexes(hitIndex, absoluteX - minHitWidth, absoluteX);
    for (const entryIndex of exactCandidates) {
        const entry = hitIndex.entries[entryIndex];
        if (absoluteX < entry.start || absoluteX > Math.max(entry.end, entry.start + minHitWidth) || y < entry.minY || y > entry.maxY) {
            continue;
        }
        const block = blocks[entry.blockIndex];
        if (isPointInBlockEntry(block, entry.pathIndex, absoluteX, y, minHitWidth)) {
            return block;
        }
    }

    if (snapHitWidth <= minHitWidth) {
        return null;
    }

    const snapCandidates: Array<{ block: Block; distance: number; order: number }> = [];
    const snapEntryIndexes = collectBlockHitEntryIndexes(hitIndex, absoluteX - snapHitWidth, absoluteX + snapHitWidth);
    for (const entryIndex of snapEntryIndexes) {
        const entry = hitIndex.entries[entryIndex];
        if (y < entry.minY || y > entry.maxY) {
            continue;
        }
        const candidate = getBlockSnapCandidate(blocks[entry.blockIndex], entry.pathIndex, absoluteX, y, minHitWidth, snapHitWidth);
        if (candidate !== null) {
            snapCandidates.push({
                ...candidate,
                order: entry.blockIndex * 1000000 + entry.pathIndex,
            });
        }
    }
    return pickUniqueSnapBlock(snapCandidates);
};

const searchBlockDataWithLinearSnap = (
    blocks: RenderData['blocks'],
    absoluteX: number,
    y: number,
    minHitWidth: number,
    snapHitWidth: number,
): Block | null => {
    if (snapHitWidth <= minHitWidth) {
        return null;
    }
    const candidates: Array<{ block: Block; distance: number; order: number }> = [];
    for (let blockIndex = 0; blockIndex < blocks.length; blockIndex++) {
        const block = blocks[blockIndex];
        if (block.path.length <= 1) {
            continue;
        }
        for (let pathIndex = 0; pathIndex < block.path.length - 1; pathIndex++) {
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
    const hitIndex = Array.isArray(data) ? undefined : (data as IndexedRenderData).blockHitIndex;
    // 将鼠标点击位置转换为真实坐标
    const x = (clientX - transform.x) / zoom.x / getTransformScaleX(transform);
    const y = (clientY - transform.y) / zoom.y / getTransformScaleY(transform);
    const absoluteX = x + zoom.offset;
    const scaleX = getTransformScaleX(transform);
    const minHitWidth = zoom.x * scaleX > 0 ? 1 / zoom.x / scaleX : 0;
    const snapHitWidth = zoom.x * scaleX > 0 ? BLOCK_SNAP_TARGET_WIDTH_PX / zoom.x / scaleX : 0;

    if (hitIndex !== undefined) {
        return searchBlockDataWithIndex(blocks, hitIndex, absoluteX, y, minHitWidth, snapHitWidth);
    }

    for (let i = 0; i < blocks.length; i++) {
        const block = blocks[i];
        const blockLx = block._startTimestamp - zoom.offset;
        const blockRx = Math.max(block._endTimestamp - zoom.offset, blockLx + minHitWidth);
        if (x < blockLx) {
            const snapBlock = searchBlockDataWithLinearSnap(blocks, absoluteX, y, minHitWidth, snapHitWidth);
            if (snapBlock !== null) {
                return snapBlock;
            }
            return null; // 之后的所有数据都不会被选中
        }
        if (x > blockRx) {
            continue;
        }
        if (block.path.length > 1) {
            for (let j = 0; j < block.path.length - 1; j++) {
                const startPt = block.path[j];
                const endPt = block.path[j + 1];
                // 坐标调零
                const lx = (startPt[0] - zoom.offset);
                const ly = startPt[1];
                const rx = Math.max((endPt[0] - zoom.offset), lx + minHitWidth);
                const ry = endPt[1];
                const h = block.size;
                if (x < lx) {
                    const snapBlock = searchBlockDataWithLinearSnap(blocks, absoluteX, y, minHitWidth, snapHitWidth);
                    if (snapBlock !== null) {
                        return snapBlock;
                    }
                    return null; // 之后的所有数据都不会被选中
                }
                if (y > ly + h) {
                    break; // 当前block中的数据都不会被选中
                }
                if (isPointInExtrudedSegment(x, y, lx, ly, rx, ry, h)) {
                    return block;
                }
            }
        }
    }
    const snapBlock = searchBlockDataWithLinearSnap(blocks, absoluteX, y, minHitWidth, snapHitWidth);
    if (snapBlock !== null) {
        return snapBlock;
    }
    return null; // 没有找到匹配的数据块
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
    (blockView as IndexedRenderData).blockHitIndex = buildBlockHitIndex(blockView);
    blockViewPathCache.set(blockView, blockView);
    tracePerf('buildBlockViewPath', startedAt, {
        blocks: blockView.blocks.length,
        hitEntries: (blockView as IndexedRenderData).blockHitIndex?.entries.length ?? 0,
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
    stateHitIndexCache.set(stateRenderData, buildStateHitIndex(stateRenderData));
    stateRenderDataCache.set(data, stateRenderData);
    tracePerf('getMemoryStateRenderData', startedAt, {
        segments: stateRenderData.length,
        rows: stateHitIndexCache.get(stateRenderData)?.rows.length ?? 0,
    });
    return stateRenderData;
};

const buildStateHitIndex = (data: Segment[]): StateHitIndex => {
    const rows: StateHitRow[] = [];
    const rowIndexes = new Map<number, StateHitRow>();
    const blockOrders: number[][] = [];
    data.forEach((segment, segmentIndex) => {
        let row = rowIndexes.get(segment.offsetY);
        if (row === undefined) {
            row = {
                yStart: segment.offsetY,
                yEnd: segment.offsetY + LINE_HEIGHT,
                segments: [],
            };
            rowIndexes.set(segment.offsetY, row);
            rows.push(row);
        }
        row.segments.push(segmentIndex);
        blockOrders[segmentIndex] = segment.blocks
            .map((block, blockIndex) => ({ block, blockIndex }))
            .sort((left, right) => left.block.offset - right.block.offset)
            .map(item => item.blockIndex);
    });
    rows.sort((left, right) => left.yStart - right.yStart);
    rows.forEach(row => row.segments.sort((left, right) => data[left].offsetX - data[right].offsetX));
    return { rows, blockOrders };
};

const findStateHitRow = (rows: StateHitRow[], y: number): StateHitRow | null => {
    let left = 0;
    let right = rows.length - 1;
    while (left <= right) {
        const mid = Math.floor((left + right) / 2);
        const row = rows[mid];
        if (y < row.yStart) {
            right = mid - 1;
        } else if (y > row.yEnd) {
            left = mid + 1;
        } else {
            return row;
        }
    }
    return null;
};

const findStateSegmentIndex = (data: Segment[], row: StateHitRow, x: number, minHitWidth: number): number => {
    const upperIndex = upperBound(row.segments, x, segmentIndex => data[segmentIndex].offsetX) - 1;
    for (let i = upperIndex; i >= 0; i--) {
        const segment = data[row.segments[i]];
        const segmentRight = Math.max(segment.offsetX + segment.size, segment.offsetX + minHitWidth);
        if (x > segmentRight) {
            break;
        }
        if (x >= segment.offsetX) {
            return row.segments[i];
        }
    }
    return -1;
};

const findStateSegmentIndexSnap = (data: Segment[], row: StateHitRow, x: number, minHitWidth: number, snapHitWidth: number): number => {
    if (snapHitWidth <= minHitWidth) {
        return -1;
    }
    const candidates: Array<{ segmentIndex: number; distance: number }> = [];
    for (const segmentIndex of row.segments) {
        const segment = data[segmentIndex];
        const visibleStart = segment.offsetX;
        const visibleEnd = Math.max(segment.offsetX + segment.size, segment.offsetX + minHitWidth);
        const visibleWidth = visibleEnd - visibleStart;
        if (visibleWidth >= snapHitWidth || (x >= visibleStart && x <= visibleEnd)) {
            continue;
        }
        const extraWidth = (snapHitWidth - visibleWidth) / 2;
        if (x < visibleStart - extraWidth || x > visibleEnd + extraWidth) {
            continue;
        }
        candidates.push({
            segmentIndex,
            distance: Math.min(Math.abs(x - visibleStart), Math.abs(x - visibleEnd)),
        });
    }
    if (candidates.length !== 1) {
        return -1;
    }
    return candidates[0].segmentIndex;
};

const findStateBlock = (segment: Segment, blockOrder: number[] | undefined, x: number, minHitWidth: number): StateBlock | null => {
    const localX = x - segment.offsetX;
    const orderedBlockIndexes = blockOrder ?? segment.blocks.map((_block, index) => index);
    const upperIndex = upperBound(orderedBlockIndexes, localX, blockIndex => segment.blocks[blockIndex].offset) - 1;
    for (let i = upperIndex; i >= 0; i--) {
        const block = segment.blocks[orderedBlockIndexes[i]];
        const blockRight = Math.max(block.offset + block.size, block.offset + minHitWidth);
        if (localX > blockRight) {
            break;
        }
        if (localX >= block.offset) {
            return { ...block, colorIndex: orderedBlockIndexes[i] };
        }
    }
    return null;
};

const findStateBlockSnap = (
    segment: Segment,
    blockOrder: number[] | undefined,
    x: number,
    minHitWidth: number,
    snapHitWidth: number,
): StateBlock | null => {
    if (snapHitWidth <= minHitWidth) {
        return null;
    }
    const localX = x - segment.offsetX;
    const orderedBlockIndexes = blockOrder ?? segment.blocks.map((_block, index) => index);
    const candidatesByBlockId = new Map<number, { block: StateBlock; distance: number; order: number }>();
    for (const blockIndex of orderedBlockIndexes) {
        const block = segment.blocks[blockIndex];
        const visibleStart = block.offset;
        const visibleEnd = Math.max(block.offset + block.size, block.offset + minHitWidth);
        const visibleWidth = visibleEnd - visibleStart;
        if (visibleWidth >= snapHitWidth || (localX >= visibleStart && localX <= visibleEnd)) {
            continue;
        }
        const extraWidth = (snapHitWidth - visibleWidth) / 2;
        if (localX < visibleStart - extraWidth || localX > visibleEnd + extraWidth) {
            continue;
        }
        const distance = Math.min(Math.abs(localX - visibleStart), Math.abs(localX - visibleEnd));
        const existing = candidatesByBlockId.get(block.id);
        if (existing === undefined || distance < existing.distance ||
            (distance === existing.distance && blockIndex < existing.order)) {
            candidatesByBlockId.set(block.id, {
                block: { ...block, colorIndex: blockIndex },
                distance,
                order: blockIndex,
            });
        }
    }
    if (candidatesByBlockId.size !== 1) {
        return null;
    }
    const candidate = candidatesByBlockId.values().next().value;
    return candidate === undefined ? null : candidate.block;
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
    // 将鼠标点击位置转换为真实坐标
    const x = (clientX - transform.x) / zoom.x / getTransformScaleX(transform);
    const y = (clientY - transform.y) / zoom.y / getTransformScaleY(transform);
    const scaleX = getTransformScaleX(transform);
    const minHitWidth = zoom.x * scaleX > 0 ? 1 / zoom.x / scaleX : 0;
    const snapHitWidth = zoom.x * scaleX > 0 ? BLOCK_SNAP_TARGET_WIDTH_PX / zoom.x / scaleX : 0;
    const stateHitIndex = stateHitIndexCache.get(data);

    if (stateHitIndex !== undefined) {
        const row = findStateHitRow(stateHitIndex.rows, y);
        if (row === null) {
            return null;
        }
        let segmentIndex = findStateSegmentIndex(data, row, x, minHitWidth);
        if (segmentIndex < 0) {
            segmentIndex = findStateSegmentIndexSnap(data, row, x, minHitWidth, snapHitWidth);
        }
        if (segmentIndex < 0) {
            return null;
        }
        const segment = data[segmentIndex];
        const block = findStateBlock(segment, stateHitIndex.blockOrders[segmentIndex], x, minHitWidth) ??
            findStateBlockSnap(segment, stateHitIndex.blockOrders[segmentIndex], x, minHitWidth, snapHitWidth);
        const { blocks, ...newSegment } = segment;
        if (block !== null) {
            return { type: 'block', data: { ...newSegment, blocks: [block] } };
        }
        return { type: 'segment', data: { ...newSegment, blocks: [] } };
    }

    for (let i = 0; i < data.length; i++) {
        const segment = data[i];
        if (x < segment.offsetX || x > Math.max(segment.offsetX + segment.size, segment.offsetX + minHitWidth) || y < segment.offsetY || y > segment.offsetY + LINE_HEIGHT) {
            continue;
        }
        for (let j = 0; j < segment.blocks.length; j++) {
            const block = segment.blocks[j];
            const start = segment.offsetX + block.offset;
            if (x < start || x > Math.max(start + block.size, start + minHitWidth)) {
                continue;
            }
            const { blocks, ...newSegment } = segment;
            return { type: 'block', data: { ...newSegment, blocks: [{ ...block, colorIndex: j }] } };
        }
        const { blocks, ...newSegment } = segment; // 去除blocks属性
        return { type: 'segment', data: { ...newSegment, blocks: [] } };
    }
    return null; // 没有找到匹配的数据块
};

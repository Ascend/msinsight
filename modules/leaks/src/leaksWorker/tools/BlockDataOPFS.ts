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

const DIR_NAME = 'block-data';
const MAX_HIT_TEST_CACHE_BYTES = 8 * 1024 * 1024;

type AsyncHandle = FileSystemFileHandle;

const isInWorker = typeof self !== 'undefined' && typeof self.document === 'undefined';

export interface PackedBlockPath {
    chunks: ReadonlyArray<Float32Array | Float64Array>;
    pathLength: number;
    pathStartTimestamp: number;
    pathEndTimestamp: number;
}

interface PendingBlock {
    block: Block;
    packedPath?: PackedBlockPath;
    pathLength: number;
    pathStartTimestamp: number;
    pathEndTimestamp: number;
}

interface BatchPathRange {
    byteOffset: number;
    byteLength: number;
}

export class BlockDataOPFS {
    private readonly storageKey: string;
    private dirHandle: FileSystemDirectoryHandle | null = null;
    private pathHandle: SyncHandle | null = null;
    private readonly asyncHandles: Map<number, AsyncHandle> = new Map();
    private readonly blockMetas: Map<number, BlockMeta[]> = new Map();
    private batchCount: number = 0;
    private batchTimeRanges: BatchTimeRange[] = [];
    private batchPathRanges: BatchPathRange[] = [];
    private pendingBlocks: PendingBlock[] = [];
    private pendingPathPoints: number = 0;
    private storedPathBytes: number = 0;
    private readonly hitTestCache: Map<number, BatchData> = new Map();
    private hitTestCacheBytes: number = 0;
    private dataVersion: number = 0;
    private readonly batchSize: number;
    private readonly maxBatchPathPoints: number = 300000;
    private readonly isWorker: boolean;

    constructor(storageKey: string = 'default', batchSize: number = 1000) {
        this.storageKey = storageKey;
        this.batchSize = batchSize;
        this.isWorker = isInWorker;
    }

    async init(): Promise<void> {
        const root = await navigator.storage.getDirectory();
        this.dirHandle = await root.getDirectoryHandle(`${DIR_NAME}-${this.storageKey}`, { create: true });
    }

    async clear(): Promise<void> {
        this.dataVersion++;
        const previousBatchCount = this.batchCount;
        this.pathHandle?.close();
        this.pathHandle = null;
        this.asyncHandles.clear();
        this.blockMetas.clear();
        this.batchCount = 0;
        this.batchTimeRanges = [];
        this.batchPathRanges = [];
        this.pendingBlocks = [];
        this.pendingPathPoints = 0;
        this.storedPathBytes = 0;
        this.hitTestCache.clear();
        this.hitTestCacheBytes = 0;
        if (this.dirHandle?.removeEntry) {
            if (this.isWorker) {
                try {
                    await this.dirHandle.removeEntry(this.getWorkerPathFileName());
                } catch {
                    // A missing stale file is already in the desired state.
                }
            } else {
                for (let index = 0; index < previousBatchCount; index++) {
                    try {
                        await this.dirHandle.removeEntry(this.getPathFileName(index));
                    } catch {
                        // A missing stale file is already in the desired state.
                    }
                }
            }
        }
    }

    getBatchCount(): number {
        return this.batchCount;
    }

    getStoredPathBytes(): number {
        return this.storedPathBytes;
    }

    getMaxBatchPathFloats(): number {
        return this.maxBatchPathPoints * 2;
    }

    findBatchesByTimestamp(timestamp: number): number[] {
        const result: number[] = [];
        for (let i = 0; i < this.batchTimeRanges.length; i++) {
            const range = this.batchTimeRanges[i];
            if (timestamp >= range.minStartTimestamp && timestamp <= range.maxEndTimestamp) {
                result.push(i);
            }
        }
        return result;
    }

    findBatchesOverlappingRange(
        startTimestamp: number,
        endTimestamp: number,
        startBatch: number = 0,
        endBatch: number = this.batchTimeRanges.length,
    ): number[] {
        const result: number[] = [];
        const firstBatch = Math.max(0, startBatch);
        const lastBatch = Math.min(endBatch, this.batchTimeRanges.length);
        for (let i = firstBatch; i < lastBatch; i++) {
            const range = this.batchTimeRanges[i];
            if (range.maxEndTimestamp >= startTimestamp && range.minStartTimestamp <= endTimestamp) {
                result.push(i);
            }
        }
        return result;
    }

    async findBlockById(blockId: number): Promise<Block | null> {
        const fragments: Block[] = [];
        for (let i = 0; i < this.batchCount; i++) {
            const metas = this.blockMetas.get(i);
            if (!metas) {
                continue;
            }
            const matchingMetas = metas.filter(item => item.id === blockId);
            if (matchingMetas.length === 0) {
                continue;
            }
            const batchData = await this.readBatchAsync(i);
            if (!batchData) {
                continue;
            }
            for (const meta of matchingMetas) {
                fragments.push(blockFromMeta(meta, batchData.pathData));
            }
        }
        if (fragments.length === 0) {
            return null;
        }
        fragments.sort((a, b) => (a.path[0]?.[0] ?? 0) - (b.path[0]?.[0] ?? 0));
        const path: Array<[number, number]> = [];
        for (const fragment of fragments) {
            for (const point of fragment.path) {
                const lastPoint = path[path.length - 1];
                if (lastPoint?.[0] === point[0] && lastPoint[1] === point[1]) {
                    continue;
                }
                path.push(point);
            }
        }
        const firstFragment = fragments[0];
        return { ...firstFragment, path };
    }

    private getPathFileName(index: number): string {
        return `paths-${index}`;
    }

    private getWorkerPathFileName(): string {
        return 'paths';
    }

    private async getOrCreatePathHandle(): Promise<SyncHandle> {
        if (this.pathHandle) {
            return this.pathHandle;
        }
        if (!this.dirHandle) {
            await this.init();
        }
        const dir = this.dirHandle;
        if (!dir) {
            throw new Error('Block path OPFS directory is unavailable');
        }
        const fileHandle = await dir.getFileHandle(this.getWorkerPathFileName(), { create: true });
        this.pathHandle = await fileHandle.createSyncAccessHandle();
        return this.pathHandle;
    }

    private async getOrCreateAsyncHandle(index: number): Promise<AsyncHandle> {
        const existing = this.asyncHandles.get(index);
        if (existing) {
            return existing;
        }
        if (!this.dirHandle) {
            await this.init();
        }
        const dir = this.dirHandle;
        if (!dir) {
            throw new Error('Block path OPFS directory is unavailable');
        }
        const fileHandle = await dir.getFileHandle(this.getPathFileName(index), { create: true });
        this.asyncHandles.set(index, fileHandle);
        return fileHandle;
    }

    private async writePendingBlocks(): Promise<void> {
        if (this.pendingBlocks.length === 0) {
            return;
        }
        let maxEndTimestamp = Number.NEGATIVE_INFINITY;
        for (const pendingBlock of this.pendingBlocks) {
            maxEndTimestamp = Math.max(maxEndTimestamp, pendingBlock.pathEndTimestamp);
        }
        this.pendingBlocks.sort((a, b) => a.pathStartTimestamp - b.pathStartTimestamp);
        const minStartTimestamp = this.pendingBlocks[0].pathStartTimestamp;
        if (this.isWorker) {
            await this.writeBatchInternalSync(this.batchCount, this.pendingBlocks, minStartTimestamp, maxEndTimestamp);
        } else {
            await this.writeBatchInternalAsync(this.batchCount, this.pendingBlocks, minStartTimestamp, maxEndTimestamp);
        }
        this.batchCount++;
        this.pendingBlocks = [];
        this.pendingPathPoints = 0;
    }

    private async writeBatchInternalSync(
        index: number,
        blocks: PendingBlock[],
        minStartTimestamp: number,
        maxEndTimestamp: number,
    ): Promise<void> {
        const pathHandle = await this.getOrCreatePathHandle();

        const metas: BlockMeta[] = [];
        let totalPathPoints = 0;
        for (const pendingBlock of blocks) {
            totalPathPoints += pendingBlock.pathLength;
        }

        const pathBuffer = new Float32Array(totalPathPoints * 2);
        let bufferOffset = 0;

        for (const pendingBlock of blocks) {
            const { block, packedPath, pathLength, pathStartTimestamp, pathEndTimestamp } = pendingBlock;
            const pathOffset = bufferOffset / 2;

            if (packedPath) {
                let remainingFloats = pathLength * 2;
                for (const chunk of packedPath.chunks) {
                    const copyLength = Math.min(chunk.length, remainingFloats);
                    pathBuffer.set(chunk.subarray(0, copyLength), bufferOffset);
                    bufferOffset += copyLength;
                    remainingFloats -= copyLength;
                    if (remainingFloats === 0) {
                        break;
                    }
                }
            } else {
                for (let i = 0; i < pathLength; i++) {
                    pathBuffer[bufferOffset++] = block.path[i][0];
                    pathBuffer[bufferOffset++] = block.path[i][1];
                }
            }

            metas.push({
                id: block.id,
                addr: block.addr,
                _startTimestamp: block._startTimestamp,
                _endTimestamp: block._endTimestamp,
                pathStartTimestamp,
                pathEndTimestamp,
                size: block.size,
                pathOffset,
                pathLength,
            });
        }

        const byteOffset = this.storedPathBytes;
        if (byteOffset === 0) {
            pathHandle.truncate(0);
        }
        const pathBytes = new Uint8Array(pathBuffer.buffer);
        const bytesWritten = pathHandle.write(pathBytes, { at: byteOffset });
        if (bytesWritten !== pathBytes.byteLength) {
            throw new Error(`Failed to write complete block path batch: ${bytesWritten}/${pathBytes.byteLength}`);
        }
        pathHandle.flush();
        this.batchTimeRanges[index] = { minStartTimestamp, maxEndTimestamp };
        this.batchPathRanges[index] = { byteOffset, byteLength: pathBuffer.byteLength };
        this.blockMetas.set(index, metas);
        this.storedPathBytes += pathBuffer.byteLength;
        for (const pendingBlock of blocks) {
            pendingBlock.block.path = [];
        }
    }

    private async writeBatchInternalAsync(
        index: number,
        blocks: PendingBlock[],
        minStartTimestamp: number,
        maxEndTimestamp: number,
    ): Promise<void> {
        const metas: BlockMeta[] = [];
        let totalPathPoints = 0;
        for (const pendingBlock of blocks) {
            totalPathPoints += pendingBlock.pathLength;
        }

        const pathBuffer = new Float32Array(totalPathPoints * 2);
        let bufferOffset = 0;

        for (const pendingBlock of blocks) {
            const { block, packedPath, pathLength, pathStartTimestamp, pathEndTimestamp } = pendingBlock;
            const pathOffset = bufferOffset / 2;

            if (packedPath) {
                let remainingFloats = pathLength * 2;
                for (const chunk of packedPath.chunks) {
                    const copyLength = Math.min(chunk.length, remainingFloats);
                    pathBuffer.set(chunk.subarray(0, copyLength), bufferOffset);
                    bufferOffset += copyLength;
                    remainingFloats -= copyLength;
                    if (remainingFloats === 0) {
                        break;
                    }
                }
            } else {
                for (let i = 0; i < pathLength; i++) {
                    pathBuffer[bufferOffset++] = block.path[i][0];
                    pathBuffer[bufferOffset++] = block.path[i][1];
                }
            }

            metas.push({
                id: block.id,
                addr: block.addr,
                _startTimestamp: block._startTimestamp,
                _endTimestamp: block._endTimestamp,
                pathStartTimestamp,
                pathEndTimestamp,
                size: block.size,
                pathOffset,
                pathLength,
            });
        }

        const fileHandle = await this.getOrCreateAsyncHandle(index);
        const writable = await fileHandle.createWritable();
        await writable.write(new Uint8Array(pathBuffer.buffer));
        await writable.close();
        this.batchTimeRanges[index] = { minStartTimestamp, maxEndTimestamp };
        this.blockMetas.set(index, metas);
        this.storedPathBytes += pathBuffer.byteLength;
        for (const pendingBlock of blocks) {
            pendingBlock.block.path = [];
        }
    }

    async addBlock(block: Block): Promise<void> {
        const pathPoints = block.path.length;
        await this.addPendingBlock({
            block,
            pathLength: pathPoints,
            pathStartTimestamp: block.path[0]?.[0] ?? block._startTimestamp,
            pathEndTimestamp: block.path[pathPoints - 1]?.[0] ?? block._endTimestamp,
        });
    }

    async addPackedBlock(block: Block, packedPath: PackedBlockPath): Promise<void> {
        await this.addPendingBlock({
            block,
            packedPath,
            pathLength: packedPath.pathLength,
            pathStartTimestamp: packedPath.pathStartTimestamp,
            pathEndTimestamp: packedPath.pathEndTimestamp,
        });
    }

    private async addPendingBlock(pendingBlock: PendingBlock): Promise<void> {
        const pathPoints = pendingBlock.pathLength;
        if (pathPoints > this.maxBatchPathPoints) {
            throw new RangeError(`Block path exceeds the hard batch limit: ${pathPoints}`);
        }
        if (this.pendingBlocks.length > 0 &&
            (this.pendingBlocks.length >= this.batchSize ||
                this.pendingPathPoints + pathPoints > this.maxBatchPathPoints)) {
            await this.writePendingBlocks();
        }
        this.pendingBlocks.push(pendingBlock);
        this.pendingPathPoints += pathPoints;
        if (this.pendingBlocks.length >= this.batchSize || this.pendingPathPoints >= this.maxBatchPathPoints) {
            await this.writePendingBlocks();
        }
    }

    async flush(): Promise<void> {
        await this.writePendingBlocks();
    }

    readBatch(index: number, target?: Float32Array): BatchData | null {
        const metas = this.blockMetas.get(index);
        const pathRange = this.batchPathRanges[index];
        if (!metas || !pathRange) {
            return null;
        }

        if (this.isWorker) {
            const pathHandle = this.pathHandle;
            if (!pathHandle) {
                return null;
            }
            if (pathRange.byteLength === 0) {
                return null;
            }
            const buffer = target !== undefined && target.buffer.byteLength >= pathRange.byteLength
                ? target.buffer
                : new ArrayBuffer(pathRange.byteLength);
            pathHandle.read(
                new Uint8Array(buffer, 0, pathRange.byteLength),
                { at: pathRange.byteOffset },
            );
            return {
                metas,
                pathData: new Float32Array(
                    buffer,
                    0,
                    pathRange.byteLength / Float32Array.BYTES_PER_ELEMENT,
                ),
            };
        }
        return null;
    }

    async readBatchAsync(index: number, target?: Float32Array): Promise<BatchData | null> {
        if (this.isWorker) {
            return this.readBatch(index, target);
        }
        const metas = this.blockMetas.get(index);
        if (!metas) {
            return null;
        }
        const fileHandle = await this.getOrCreateAsyncHandle(index);
        const file = await fileHandle.getFile();
        if (file.size === 0) {
            return null;
        }
        const sourceBuffer = await file.arrayBuffer();
        const buffer = target !== undefined && target.buffer.byteLength >= sourceBuffer.byteLength
            ? target.buffer
            : new ArrayBuffer(sourceBuffer.byteLength);
        new Uint8Array(buffer, 0, sourceBuffer.byteLength).set(new Uint8Array(sourceBuffer));
        return {
            metas,
            pathData: new Float32Array(buffer, 0, sourceBuffer.byteLength / Float32Array.BYTES_PER_ELEMENT),
        };
    }

    async readBatchForHitTest(index: number): Promise<BatchData | null> {
        const cached = this.hitTestCache.get(index);
        if (cached) {
            this.hitTestCache.delete(index);
            this.hitTestCache.set(index, cached);
            return cached;
        }
        const readVersion = this.dataVersion;
        const batchData = await this.readBatchAsync(index);
        if (!batchData || readVersion !== this.dataVersion) {
            return null;
        }
        this.hitTestCache.set(index, batchData);
        this.hitTestCacheBytes += batchData.pathData.byteLength;
        while (this.hitTestCacheBytes > MAX_HIT_TEST_CACHE_BYTES && this.hitTestCache.size > 1) {
            const oldestIndex = this.hitTestCache.keys().next().value;
            if (oldestIndex === undefined) {
                break;
            }
            const oldest = this.hitTestCache.get(oldestIndex);
            this.hitTestCache.delete(oldestIndex);
            this.hitTestCacheBytes -= oldest?.pathData.byteLength ?? 0;
        }
        return batchData;
    }
}

export const getPointFromPathData = (pathData: Float32Array, pathOffset: number, pointIndex: number): [number, number] => {
    const idx = (pathOffset + pointIndex) * 2;
    return [pathData[idx], pathData[idx + 1]];
};

export const blockFromMeta = (meta: BlockMeta, pathData: Float32Array): Block => {
    const path: Array<[number, number]> = [];
    for (let k = 0; k < meta.pathLength; k++) {
        path.push(getPointFromPathData(pathData, meta.pathOffset, k));
    }
    return {
        id: meta.id,
        addr: meta.addr,
        _startTimestamp: meta._startTimestamp,
        _endTimestamp: meta._endTimestamp,
        size: meta.size,
        path,
    };
};

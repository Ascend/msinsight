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

import {
    BLOCK_PATH_CACHE_VERSION,
    createBlockPathCacheStorageKey,
    getLeaksOpfsDirectory,
    getLeaksOpfsRoot,
    normalizeLeaksFileHash,
} from './opfsConfig';

const DIR_NAME = 'block-data';
const CACHE_MANIFEST_FILE = 'cache-manifest.json';
// 单独保存 LRU 状态，避免淘汰缓存时反序列化包含大量块数据的 manifest。
const CACHE_STATE_FILE = 'cache-state.json';
// 标识 manifest 和状态文件的存储结构版本，与前端 storageKey 版本相互独立。
const CACHE_MANIFEST_VERSION = 1;
// Worker 和 fallback 中具有相同文件 hash 的缓存按照同一份数据计算。
const MAX_PERSISTENT_CACHE_COUNT = 10;
const MAX_HIT_TEST_CACHE_BYTES = 8 * 1024 * 1024;
const CACHE_DIRECTORY_PATTERN =
    /^block-data-(?:main|main-thread)-cache-(.+)-([0-9a-f]{64}(?:-[a-z0-9_-]+){0,2})$/;

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

interface BlockDataCacheManifest {
    version: number;
    fileHash: string;
    status: 'building' | 'complete';
    lastAccessedAt?: number;
    metadata?: BlockGraphMetadata;
    batchCount?: number;
    batchTimeRanges?: BatchTimeRange[];
    batchPathRanges?: BatchPathRange[];
    blockMetas?: BlockMeta[][];
    storedPathBytes?: number;
}

type CompleteBlockDataCacheManifest = BlockDataCacheManifest & {
    metadata: BlockGraphMetadata;
    batchCount: number;
    batchTimeRanges: BatchTimeRange[];
    batchPathRanges: BatchPathRange[];
    blockMetas: BlockMeta[][];
    storedPathBytes: number;
};

interface BlockDataCacheState {
    // 缓存状态结构版本，用于拒绝过期或不兼容的缓存元数据。
    version: number;
    // 源文件 hash，用于将 Worker 和 fallback 缓存归为同一份数据。
    fileHash: string;
    // building 表示缓存尚未写入完成，将在下一次导入时清理。
    status: 'building' | 'complete';
    // 缓存命中时更新该时间，使淘汰顺序与数据最近打开顺序保持一致。
    lastAccessedAt: number;
}

interface PersistentCacheEntry {
    directoryName: string;
    fileHash: string;
    lastAccessedAt: number;
}

interface PrepareBlockDataStorageOptions {
    fileHash: unknown;
    temporaryStorageKey: string;
    blockDataOPFS: BlockDataOPFS;
    mainThread?: boolean;
    beforeStorageChange: () => Promise<void>;
}

interface BlockDataStorageResult {
    fileHash: string;
    blockDataOPFS: BlockDataOPFS;
    available: boolean;
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

    private static async readJsonFile<T>(
        directory: FileSystemDirectoryHandle,
        fileName: string,
    ): Promise<T | null> {
        try {
            const fileHandle = await directory.getFileHandle(fileName);
            const file = await fileHandle.getFile();
            return JSON.parse(new TextDecoder().decode(await file.arrayBuffer())) as T;
        } catch {
            return null;
        }
    }

    private static async removeCacheDirectory(
        root: FileSystemDirectoryHandle,
        directoryName: string,
    ): Promise<void> {
        try {
            await root.removeEntry?.(directoryName, { recursive: true });
        } catch {
            // 其它渲染实例可能已经删除了同一个缓存目录。
        }
    }

    private static isCompleteCacheState(
        state: BlockDataCacheState | null,
        fileHash: string,
    ): state is BlockDataCacheState {
        return state !== null && state.version === CACHE_MANIFEST_VERSION && state.status === 'complete' &&
            state.fileHash === fileHash && Number.isFinite(state.lastAccessedAt);
    }

    private static async writeJsonFile(
        directory: FileSystemDirectoryHandle,
        fileName: string,
        data: unknown,
    ): Promise<void> {
        const fileHandle = await directory.getFileHandle(fileName, { create: true });
        const writable = await fileHandle.createWritable();
        await writable.write(new TextEncoder().encode(JSON.stringify(data)));
        await writable.close();
    }

    /**
     * 清理写入中断或状态无效的缓存，并仅保留最近打开的数据缓存。
     * 当前正在导入的数据即使尚未完成缓存写入，也会预先占用一个缓存名额。
     */
    static async prunePersistentCaches(
        currentFileHash: string,
        maxCacheCount: number = MAX_PERSISTENT_CACHE_COUNT,
    ): Promise<void> {
        const root = await getLeaksOpfsRoot();
        if (!root) {
            return;
        }
        const completeEntries: PersistentCacheEntry[] = [];
        for await (const directoryName of root.keys()) {
            const match = CACHE_DIRECTORY_PATTERN.exec(directoryName);
            if (!match) {
                continue;
            }
            const directoryCacheVersion = match[1];
            const directoryFileHash = match[2];
            if (directoryCacheVersion !== BLOCK_PATH_CACHE_VERSION) {
                await this.removeCacheDirectory(root, directoryName);
                continue;
            }
            let directory: FileSystemDirectoryHandle;
            try {
                directory = await root.getDirectoryHandle(directoryName);
            } catch {
                continue;
            }
            let state = await this.readJsonFile<BlockDataCacheState>(directory, CACHE_STATE_FILE);
            if (!this.isCompleteCacheState(state, directoryFileHash)) {
                const manifest = await this.readJsonFile<BlockDataCacheManifest>(directory, CACHE_MANIFEST_FILE);
                if (manifest?.version === CACHE_MANIFEST_VERSION && manifest.status === 'complete' &&
                    manifest.fileHash === directoryFileHash && Number.isFinite(manifest.lastAccessedAt ?? 0)) {
                    state = {
                        version: manifest.version,
                        fileHash: manifest.fileHash,
                        status: manifest.status,
                        lastAccessedAt: manifest.lastAccessedAt ?? 0,
                    };
                    try {
                        await this.writeJsonFile(directory, CACHE_STATE_FILE, state);
                    } catch {
                        // 本次状态迁移失败后，可以在下一次导入时重试。
                    }
                }
            }
            if (!this.isCompleteCacheState(state, directoryFileHash)) {
                await this.removeCacheDirectory(root, directoryName);
                continue;
            }
            completeEntries.push({
                directoryName,
                fileHash: state.fileHash,
                lastAccessedAt: state.lastAccessedAt,
            });
        }

        const latestAccessByHash = new Map<string, number>();
        for (const entry of completeEntries) {
            latestAccessByHash.set(
                entry.fileHash,
                Math.max(latestAccessByHash.get(entry.fileHash) ?? 0, entry.lastAccessedAt),
            );
        }
        if (currentFileHash) {
            // 将当前正在导入的数据视为最近打开的缓存候选项。
            latestAccessByHash.delete(currentFileHash);
            latestAccessByHash.set(currentFileHash, Date.now());
        }
        const retainedHashes = new Set(
            [...latestAccessByHash.entries()]
                .sort((first, second) => second[1] - first[1])
                .slice(0, Math.max(0, maxCacheCount))
                .map(([fileHash]) => fileHash),
        );
        await Promise.all(completeEntries
            .filter(entry => !retainedHashes.has(entry.fileHash))
            .map(entry => this.removeCacheDirectory(root, entry.directoryName)));
    }

    async init(): Promise<boolean> {
        this.dirHandle = await getLeaksOpfsDirectory(`${DIR_NAME}-${this.storageKey}`);
        return this.dirHandle !== null;
    }

    private resetMemoryState(): void {
        this.dataVersion++;
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
    }

    dispose(): void {
        this.resetMemoryState();
        this.dirHandle = null;
    }

    async removeStorage(): Promise<void> {
        this.resetMemoryState();
        const root = await getLeaksOpfsRoot();
        if (root?.removeEntry) {
            try {
                await root.removeEntry(`${DIR_NAME}-${this.storageKey}`, { recursive: true });
            } catch {
                // 临时缓存目录不存在时，无需再次删除。
            }
        }
        this.dirHandle = null;
    }

    async clear(): Promise<void> {
        const previousBatchCount = this.batchCount;
        this.resetMemoryState();
        if (this.dirHandle?.removeEntry) {
            try {
                await this.dirHandle.removeEntry(CACHE_MANIFEST_FILE);
            } catch {
                // manifest 不存在时，缓存已经处于失效状态。
            }
            try {
                await this.dirHandle.removeEntry(CACHE_STATE_FILE);
            } catch {
                // 状态文件不存在时，缓存已经处于失效状态。
            }
            if (this.isWorker) {
                try {
                    await this.dirHandle.removeEntry(this.getWorkerPathFileName());
                } catch {
                    // 过期路径文件不存在时，无需再次删除。
                }
            } else {
                for (let index = 0; index < previousBatchCount; index++) {
                    try {
                        await this.dirHandle.removeEntry(this.getPathFileName(index));
                    } catch {
                        // 过期路径文件不存在时，无需再次删除。
                    }
                }
            }
        }
    }

    private async readManifest(): Promise<BlockDataCacheManifest | null> {
        if (!this.dirHandle) {
            await this.init();
        }
        return this.dirHandle
            ? BlockDataOPFS.readJsonFile<BlockDataCacheManifest>(this.dirHandle, CACHE_MANIFEST_FILE)
            : null;
    }

    private async writeManifest(manifest: BlockDataCacheManifest): Promise<void> {
        if (!this.dirHandle) {
            await this.init();
        }
        if (!this.dirHandle) {
            throw new Error('Block path cache manifest is unavailable');
        }
        await BlockDataOPFS.writeJsonFile(this.dirHandle, CACHE_MANIFEST_FILE, manifest);
    }

    private async writeCacheState(
        fileHash: string,
        status: BlockDataCacheState['status'],
        lastAccessedAt: number = Date.now(),
    ): Promise<void> {
        if (!this.dirHandle) {
            await this.init();
        }
        if (this.dirHandle) {
            await BlockDataOPFS.writeJsonFile(this.dirHandle, CACHE_STATE_FILE, {
                version: CACHE_MANIFEST_VERSION,
                fileHash,
                status,
                lastAccessedAt,
            } satisfies BlockDataCacheState);
        }
    }

    async markCacheBuilding(fileHash: string): Promise<void> {
        await this.writeCacheState(fileHash, 'building');
        await this.writeManifest({
            version: CACHE_MANIFEST_VERSION,
            fileHash,
            status: 'building',
        });
    }

    async saveCompleteCache(fileHash: string, metadata: BlockGraphMetadata): Promise<void> {
        await this.flush();
        const lastAccessedAt = Date.now();
        await this.writeManifest({
            version: CACHE_MANIFEST_VERSION,
            fileHash,
            status: 'complete',
            lastAccessedAt,
            metadata,
            batchCount: this.batchCount,
            batchTimeRanges: this.batchTimeRanges,
            batchPathRanges: this.batchPathRanges,
            blockMetas: Array.from({ length: this.batchCount }, (_, index) => this.blockMetas.get(index) ?? []),
            storedPathBytes: this.storedPathBytes,
        });
        await this.writeCacheState(fileHash, 'complete', lastAccessedAt);
    }

    private isCompleteManifest(
        manifest: BlockDataCacheManifest,
        fileHash: string,
    ): manifest is CompleteBlockDataCacheManifest {
        const batchCount = manifest.batchCount;
        const storedPathBytes = manifest.storedPathBytes;
        const validMetas = manifest.blockMetas?.every(metas => Array.isArray(metas) && metas.every(meta =>
            meta !== null && typeof meta === 'object' && Number.isFinite(meta.id) && typeof meta.addr === 'string' &&
            Number.isFinite(meta._startTimestamp) && Number.isFinite(meta._endTimestamp) &&
            Number.isFinite(meta.pathStartTimestamp) && Number.isFinite(meta.pathEndTimestamp) &&
            Number.isFinite(meta.size) && Number.isInteger(meta.pathOffset) && meta.pathOffset >= 0 &&
            Number.isInteger(meta.pathLength) && meta.pathLength >= 0,
        ));
        const validTimeRanges = manifest.batchTimeRanges?.every(range =>
            range !== null && typeof range === 'object' &&
            Number.isFinite(range.minStartTimestamp) && Number.isFinite(range.maxEndTimestamp),
        );
        const validPathRanges = manifest.batchPathRanges?.every(range =>
            range !== null && typeof range === 'object' && Number.isInteger(range.byteOffset) && range.byteOffset >= 0 &&
            Number.isInteger(range.byteLength) && range.byteLength >= 0,
        );
        return manifest.version === CACHE_MANIFEST_VERSION && manifest.fileHash === fileHash &&
            manifest.status === 'complete' && manifest.metadata !== undefined && manifest.metadata !== null &&
            typeof manifest.metadata === 'object' &&
            batchCount !== undefined && Number.isInteger(batchCount) && batchCount >= 0 &&
            Array.isArray(manifest.batchTimeRanges) && Array.isArray(manifest.batchPathRanges) &&
            Array.isArray(manifest.blockMetas) && storedPathBytes !== undefined && Number.isFinite(storedPathBytes) &&
            storedPathBytes >= 0 && manifest.metadata.batchCount === batchCount && validMetas === true &&
            validTimeRanges === true && validPathRanges === true && manifest.batchTimeRanges.length === batchCount &&
            manifest.blockMetas.length === batchCount &&
            (this.isWorker ? manifest.batchPathRanges.length === batchCount : true);
    }

    private async validateStoredPaths(manifest: CompleteBlockDataCacheManifest): Promise<boolean> {
        if (!this.dirHandle) {
            return false;
        }
        if (manifest.batchCount === 0) {
            return manifest.storedPathBytes === 0;
        }
        if (this.isWorker) {
            try {
                const fileHandle = await this.dirHandle.getFileHandle(this.getWorkerPathFileName());
                this.pathHandle = await fileHandle.createSyncAccessHandle();
                return this.pathHandle.getSize() === manifest.storedPathBytes;
            } catch {
                return false;
            }
        }
        let storedBytes = 0;
        try {
            for (let index = 0; index < manifest.batchCount; index++) {
                const fileHandle = await this.dirHandle.getFileHandle(this.getPathFileName(index));
                const file = await fileHandle.getFile();
                const expectedBytes = manifest.blockMetas[index].reduce(
                    (sum, meta) => sum + meta.pathLength * 2 * Float32Array.BYTES_PER_ELEMENT,
                    0,
                );
                if (file.size !== expectedBytes) {
                    return false;
                }
                storedBytes += file.size;
                this.asyncHandles.set(index, fileHandle);
            }
        } catch {
            return false;
        }
        return storedBytes === manifest.storedPathBytes;
    }

    async loadCompleteCache(fileHash: string): Promise<BlockGraphMetadata | null> {
        this.resetMemoryState();
        const manifest = await this.readManifest();
        if (!manifest || !this.isCompleteManifest(manifest, fileHash) ||
            !await this.validateStoredPaths(manifest)) {
            this.resetMemoryState();
            return null;
        }
        this.batchCount = manifest.batchCount;
        this.batchTimeRanges = manifest.batchTimeRanges;
        this.batchPathRanges = manifest.batchPathRanges;
        this.storedPathBytes = manifest.storedPathBytes;
        manifest.blockMetas.forEach((metas, index) => this.blockMetas.set(index, metas));
        try {
            await this.writeCacheState(fileHash, 'complete');
        } catch {
            // 访问时间更新失败不能使有效的缓存命中变为缓存未命中。
        }
        return manifest.metadata;
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

    async release(temporaryStorageKey: string): Promise<void> {
        if (this.storageKey === temporaryStorageKey) {
            await this.removeStorage();
        } else {
            this.dispose();
        }
    }

    static async prepareStorage(
        options: PrepareBlockDataStorageOptions,
    ): Promise<BlockDataStorageResult> {
        const fileHash = normalizeLeaksFileHash(options.fileHash);
        const nextStorageKey = fileHash
            ? createBlockPathCacheStorageKey(fileHash, options.mainThread)
            : options.temporaryStorageKey;
        let blockDataOPFS = options.blockDataOPFS;
        if (nextStorageKey !== blockDataOPFS.storageKey) {
            await options.beforeStorageChange();
            await blockDataOPFS.release(options.temporaryStorageKey);
            blockDataOPFS = new BlockDataOPFS(nextStorageKey);
        }
        try {
            await BlockDataOPFS.prunePersistentCaches(fileHash);
        } catch {
            // 缓存淘汰失败不能阻止当前数据继续加载。
        }
        const available = await blockDataOPFS.init();
        return { fileHash, blockDataOPFS, available };
    }

    async loadCompleteCacheForBuild(fileHash: string): Promise<BlockGraphMetadata | null> {
        if (!fileHash) {
            return null;
        }
        const metadata = await this.loadCompleteCache(fileHash);
        if (!metadata) {
            await this.removeStorage();
            await this.init();
        }
        return metadata;
    }

    async tryMarkCacheBuilding(fileHash: string): Promise<void> {
        if (!fileHash) {
            return;
        }
        try {
            await this.markCacheBuilding(fileHash);
        } catch {
            // 缓存元数据写入失败不能阻止当前块图继续绘制。
        }
    }

    async trySaveCompleteCache(fileHash: string, metadata: BlockGraphMetadata): Promise<void> {
        if (!fileHash) {
            return;
        }
        try {
            await this.saveCompleteCache(fileHash, metadata);
        } catch {
            // 未生成完整 manifest 时，下一次导入会重新构建缓存。
        }
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

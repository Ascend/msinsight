/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { BlockDataOPFS } from './BlockDataOPFS';
import { isLeaksOpfsEnabled } from './opfsConfig';
import { TextDecoder, TextEncoder } from 'util';

Object.assign(globalThis, { TextDecoder, TextEncoder });

class FakeFileStore {
    bytes = new Uint8Array();
}

const createNotFoundError = (message: string): Error => {
    const error = new Error(message);
    error.name = 'NotFoundError';
    return error;
};

class FakeSyncAccessHandle {
    constructor(private readonly store: FakeFileStore) {}

    close(): void {}

    flush(): void {}

    getSize(): number {
        return this.store.bytes.byteLength;
    }

    read(buffer: ArrayBuffer | ArrayBufferView, options?: { at?: number }): number {
        const target = buffer instanceof ArrayBuffer
            ? new Uint8Array(buffer)
            : new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        const source = this.store.bytes.subarray(options?.at ?? 0, (options?.at ?? 0) + target.byteLength);
        target.set(source);
        return source.byteLength;
    }

    truncate(size: number): void {
        this.store.bytes = this.store.bytes.slice(0, size);
    }

    write(buffer: ArrayBuffer | ArrayBufferView, options?: { at?: number }): number {
        const source = buffer instanceof ArrayBuffer
            ? new Uint8Array(buffer)
            : new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        const offset = options?.at ?? 0;
        const next = new Uint8Array(Math.max(this.store.bytes.byteLength, offset + source.byteLength));
        next.set(this.store.bytes);
        next.set(source, offset);
        this.store.bytes = next;
        return source.byteLength;
    }
}

class FakeFileHandle {
    getFileError: Error | null = null;

    constructor(readonly store: FakeFileStore) {}

    async createSyncAccessHandle(): Promise<FileSystemSyncAccessHandle> {
        return new FakeSyncAccessHandle(this.store) as unknown as FileSystemSyncAccessHandle;
    }

    async createWritable(): Promise<FileSystemWritableFileStream> {
        let bytes = new Uint8Array();
        const writable = {
            write: async (data: ArrayBuffer | ArrayBufferView) => {
                const source = data instanceof ArrayBuffer
                    ? new Uint8Array(data)
                    : new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
                bytes = source.slice();
            },
            close: async () => {
                this.store.bytes = bytes;
            },
        };
        return writable as unknown as FileSystemWritableFileStream;
    }

    async getFile(): Promise<File> {
        if (this.getFileError) {
            throw this.getFileError;
        }
        const file = {
            size: this.store.bytes.byteLength,
            arrayBuffer: async () => this.store.bytes.slice().buffer,
        };
        return file as unknown as File;
    }
}

class FakeDirectoryHandle {
    readonly directories = new Map<string, FakeDirectoryHandle>();
    readonly files = new Map<string, FakeFileHandle>();

    async getDirectoryHandle(name: string, options?: { create?: boolean }): Promise<FileSystemDirectoryHandle> {
        const existing = this.directories.get(name);
        if (existing) {
            return existing as unknown as FileSystemDirectoryHandle;
        }
        if (!options?.create) {
            throw createNotFoundError('Directory not found');
        }
        const directory = new FakeDirectoryHandle();
        this.directories.set(name, directory);
        return directory as unknown as FileSystemDirectoryHandle;
    }

    async getFileHandle(name: string, options?: { create?: boolean }): Promise<FileSystemFileHandle> {
        const existing = this.files.get(name);
        if (existing) {
            return existing as unknown as FileSystemFileHandle;
        }
        if (!options?.create) {
            throw createNotFoundError('File not found');
        }
        const file = new FakeFileHandle(new FakeFileStore());
        this.files.set(name, file);
        return file as unknown as FileSystemFileHandle;
    }

    async * keys(): AsyncIterableIterator<string> {
        yield * this.directories.keys();
        yield * this.files.keys();
    }

    async removeEntry(name: string): Promise<void> {
        this.files.delete(name);
        this.directories.delete(name);
    }
}

const FILE_HASH = 'a'.repeat(64);
const STORAGE_KEY = `main-cache-v1-${FILE_HASH}`;
const METADATA: BlockGraphMetadata = {
    minTimestamp: 1,
    maxTimestamp: 3,
    minSize: 0,
    maxSize: 16,
    batchCount: 1,
};

describe('BlockDataOPFS persistent cache', () => {
    const originalStorage = Object.getOwnPropertyDescriptor(navigator, 'storage');
    let root: FakeDirectoryHandle;

    beforeEach(() => {
        root = new FakeDirectoryHandle();
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: async () => root as unknown as FileSystemDirectoryHandle },
        });
    });

    afterEach(() => {
        jest.restoreAllMocks();
    });

    afterAll(() => {
        if (originalStorage) {
            Object.defineProperty(navigator, 'storage', originalStorage);
        }
    });

    const writeCompleteCache = async (
        fileHash: string = FILE_HASH,
        storageKey: string = STORAGE_KEY,
    ): Promise<void> => {
        const cache = new BlockDataOPFS(storageKey);
        await cache.init();
        await cache.addBlock({
            id: 7,
            addr: '0x1000',
            _startTimestamp: 1,
            _endTimestamp: 3,
            size: 16,
            path: [[1, 0], [2, 16], [3, 0]],
        });
        await cache.saveCompleteCache(fileHash, METADATA);
        cache.dispose();
    };

    const writeJsonFile = async (
        directory: FakeDirectoryHandle,
        fileName: string,
        value: unknown,
    ): Promise<void> => {
        const fileHandle = await directory.getFileHandle(fileName, { create: true });
        const writable = await fileHandle.createWritable();
        await writable.write(new TextEncoder().encode(JSON.stringify(value)));
        await writable.close();
    };

    const readJsonFile = async <T>(directory: FakeDirectoryHandle, fileName: string): Promise<T> => {
        const fileHandle = await directory.getFileHandle(fileName);
        const file = await fileHandle.getFile();
        return JSON.parse(new TextDecoder().decode(await file.arrayBuffer())) as T;
    };

    it('restores a complete cache and its block metadata', async () => {
        await writeCompleteCache();

        const restored = new BlockDataOPFS(STORAGE_KEY);
        const metadata = await restored.loadCompleteCache(FILE_HASH);
        const batch = await restored.readBatchAsync(0);

        expect(metadata).toEqual(METADATA);
        expect(batch?.metas).toHaveLength(1);
        expect(batch?.metas[0]).toMatchObject({ id: 7, addr: '0x1000', pathLength: 3 });
        expect(Array.from(batch?.pathData ?? [])).toEqual([1, 0, 2, 16, 3, 0]);
    });

    it('rejects a cache whose manifest is still building', async () => {
        await writeCompleteCache();
        const interrupted = new BlockDataOPFS(STORAGE_KEY);
        await interrupted.init();
        await interrupted.markCacheBuilding(FILE_HASH);
        interrupted.dispose();

        const restored = new BlockDataOPFS(STORAGE_KEY);
        await expect(restored.loadCompleteCache(FILE_HASH)).resolves.toBeNull();
    });

    it('rejects a complete manifest when the path file is truncated', async () => {
        await writeCompleteCache();
        const directory = root.directories.get(`block-data-${STORAGE_KEY}`);
        const pathFile = directory?.files.get('paths-0');
        if (!pathFile) {
            throw new Error('Expected path file was not written');
        }
        pathFile.store.bytes = pathFile.store.bytes.slice(0, pathFile.store.bytes.byteLength - 4);

        const restored = new BlockDataOPFS(STORAGE_KEY);
        await expect(restored.loadCompleteCache(FILE_HASH)).resolves.toBeNull();
    });

    it('does not reuse a complete cache for a different hash', async () => {
        await writeCompleteCache();
        const restored = new BlockDataOPFS(STORAGE_KEY);
        await expect(restored.loadCompleteCache('b'.repeat(64))).resolves.toBeNull();
    });

    it('treats a persistent cache file access error as a one-time cache failure', async () => {
        await writeCompleteCache();
        const directory = root.directories.get(`block-data-${STORAGE_KEY}`);
        const manifestFile = directory?.files.get('cache-manifest.json');
        if (!manifestFile) {
            throw new Error('Expected cache manifest to exist');
        }
        const accessError = new Error('The file is temporarily locked');
        accessError.name = 'NoModificationAllowedError';
        manifestFile.getFileError = accessError;

        const restored = new BlockDataOPFS(STORAGE_KEY);

        await expect(restored.loadCompleteCache(FILE_HASH)).resolves.toBeNull();
        expect(restored.consumeCacheAccessFailure()).toBe(true);
        expect(restored.consumeCacheAccessFailure()).toBe(false);
        expect(isLeaksOpfsEnabled()).toBe(true);
    });

    it('keeps a missing cache manifest as a normal cache miss', async () => {
        const restored = new BlockDataOPFS(STORAGE_KEY);

        await expect(restored.loadCompleteCache(FILE_HASH)).resolves.toBeNull();
        expect(restored.consumeCacheAccessFailure()).toBe(false);
    });

    it('removes an invalid cache directory before rebuilding it', async () => {
        await writeCompleteCache();
        const cache = new BlockDataOPFS(STORAGE_KEY);
        await cache.init();

        await cache.removeStorage();

        expect(root.directories.has(`block-data-${STORAGE_KEY}`)).toBe(false);
    });

    it('keeps only the ten most recently opened data caches', async () => {
        const hashes = Array.from({ length: 12 }, (_, index) => index.toString(16).padStart(64, '0'));
        const now = jest.spyOn(Date, 'now');
        for (let index = 0; index < hashes.length; index++) {
            now.mockReturnValue(index + 1);
            await writeCompleteCache(hashes[index], `main-cache-v1-${hashes[index]}`);
        }
        now.mockReturnValue(100);

        await BlockDataOPFS.prunePersistentCaches(hashes[11]);

        expect(root.directories.has(`block-data-main-cache-v1-${hashes[0]}`)).toBe(false);
        expect(root.directories.has(`block-data-main-cache-v1-${hashes[1]}`)).toBe(false);
        expect(root.directories.size).toBe(10);
    });

    it('refreshes the recency of a cache when it is opened', async () => {
        const hashes = Array.from({ length: 11 }, (_, index) => (index + 1).toString(16).padStart(64, '0'));
        const now = jest.spyOn(Date, 'now');
        for (let index = 0; index < hashes.length; index++) {
            now.mockReturnValue(index + 1);
            await writeCompleteCache(hashes[index], `main-cache-v1-${hashes[index]}`);
        }
        now.mockReturnValue(100);
        const reopened = new BlockDataOPFS(`main-cache-v1-${hashes[0]}`);
        await expect(reopened.loadCompleteCache(hashes[0])).resolves.toEqual(METADATA);
        reopened.dispose();
        now.mockReturnValue(101);

        await BlockDataOPFS.prunePersistentCaches(hashes[10]);

        expect(root.directories.has(`block-data-main-cache-v1-${hashes[0]}`)).toBe(true);
        expect(root.directories.has(`block-data-main-cache-v1-${hashes[1]}`)).toBe(false);
    });

    it('removes interrupted building caches during pruning', async () => {
        const interruptedHash = 'b'.repeat(64);
        const interrupted = new BlockDataOPFS(`main-cache-v1-${interruptedHash}`);
        await interrupted.init();
        await interrupted.markCacheBuilding(interruptedHash);
        interrupted.dispose();

        await BlockDataOPFS.prunePersistentCaches(FILE_HASH);

        expect(root.directories.has(`block-data-main-cache-v1-${interruptedHash}`)).toBe(false);
    });

    it('removes caches created by an outdated frontend cache version', async () => {
        await writeCompleteCache(FILE_HASH, `main-cache-v0-${FILE_HASH}`);

        await BlockDataOPFS.prunePersistentCaches(FILE_HASH);

        expect(root.directories.has(`block-data-main-cache-v0-${FILE_HASH}`)).toBe(false);
    });

    it('repairs a stale building state when the manifest is already complete', async () => {
        await writeCompleteCache();
        const directory = root.directories.get(`block-data-${STORAGE_KEY}`);
        if (!directory) {
            throw new Error('Expected cache directory was not written');
        }
        await writeJsonFile(directory, 'cache-state.json', {
            version: 1,
            fileHash: FILE_HASH,
            status: 'building',
            lastAccessedAt: 1,
        });

        await BlockDataOPFS.prunePersistentCaches(FILE_HASH);

        expect(root.directories.has(`block-data-${STORAGE_KEY}`)).toBe(true);
        await expect(readJsonFile(directory, 'cache-state.json')).resolves.toMatchObject({
            version: 1,
            fileHash: FILE_HASH,
            status: 'complete',
        });
    });

    it('reports the current storage operation unavailable without disabling later OPFS attempts', async () => {
        const getDirectory = jest.fn().mockRejectedValue(new Error('OPFS access denied'));
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory },
        });
        const warn = jest.spyOn(console, 'warn').mockImplementation(() => undefined);
        const beforeStorageChange = jest.fn().mockResolvedValue(undefined);

        const result = await BlockDataOPFS.prepareStorage({
            fileHash: FILE_HASH,
            temporaryStorageKey: 'temporary',
            blockDataOPFS: new BlockDataOPFS('temporary'),
            beforeStorageChange,
        });

        expect(result.available).toBe(false);
        expect(beforeStorageChange).toHaveBeenCalledTimes(1);
        const attemptsAfterPrepare = getDirectory.mock.calls.length;
        expect(attemptsAfterPrepare).toBeGreaterThan(1);
        expect(isLeaksOpfsEnabled()).toBe(true);
        await expect(result.blockDataOPFS.removeStorage()).resolves.toBeUndefined();
        expect(getDirectory.mock.calls.length).toBeGreaterThan(attemptsAfterPrepare);
        warn.mockRestore();
    });
});

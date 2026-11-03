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

import { getLeaksOpfsDirectory, reportLeaksOpfsAccessFailure } from './opfsConfig';

const DIR_NAME = 'memory-block-batches';

export class BatchOPFS {
    private readonly accessHandles: Map<number, FileSystemSyncAccessHandle> = new Map();
    private readonly memoryBatches: Map<number, Float32Array> = new Map();
    private readonly storageKey: string;
    private dirHandle: FileSystemDirectoryHandle | null = null;
    private fileCount: number = 0;
    private initialized = false;

    constructor(storageKey: string = 'default') {
        this.storageKey = storageKey;
    }

    async init(): Promise<void> {
        this.dirHandle = await getLeaksOpfsDirectory(`${DIR_NAME}-${this.storageKey}`);
        this.initialized = true;
    }

    async clear(): Promise<void> {
        const previousFileCount = this.fileCount;
        for (const handle of this.accessHandles.values()) {
            handle.close();
        }
        this.accessHandles.clear();
        this.memoryBatches.clear();
        this.fileCount = 0;
        if (this.dirHandle?.removeEntry) {
            for (let index = 0; index < previousFileCount; index++) {
                try {
                    await this.dirHandle.removeEntry(this.getFileName(index));
                } catch {
                    // A missing stale file is already in the desired state.
                }
            }
        }
    }

    private getFileName(index: number): string {
        return `batch-${index}`;
    }

    private async getOrCreateAccessHandle(index: number): Promise<FileSystemSyncAccessHandle> {
        const existing = this.accessHandles.get(index);
        if (existing) {
            return existing;
        }
        if (!this.initialized) {
            await this.init();
        }
        const dir = this.dirHandle;
        if (!dir) {
            throw new Error('Render batch OPFS directory is unavailable');
        }
        const fileHandle = await dir.getFileHandle(this.getFileName(index), { create: true });
        const accessHandle = await fileHandle.createSyncAccessHandle();
        this.accessHandles.set(index, accessHandle);
        return accessHandle;
    }

    private fallbackToMemory(index: number, data: Float32Array, error: unknown): void {
        const failedHandle = this.accessHandles.get(index);
        try {
            failedHandle?.close();
        } catch {
            // The failed handle must not be reused.
        }
        this.accessHandles.delete(index);
        this.dirHandle = null;
        this.initialized = true;
        reportLeaksOpfsAccessFailure('render batch write', error);
        this.memoryBatches.set(index, data.slice());
        this.fileCount = Math.max(this.fileCount, index + 1);
    }

    async write(index: number, data: Float32Array): Promise<void> {
        if (!this.initialized) {
            await this.init();
        }
        if (!this.dirHandle) {
            this.memoryBatches.set(index, data.slice());
            this.fileCount = Math.max(this.fileCount, index + 1);
            return;
        }
        try {
            const accessHandle = await this.getOrCreateAccessHandle(index);
            const buffer = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
            accessHandle.truncate(0);
            const bytesWritten = accessHandle.write(buffer, { at: 0 });
            if (bytesWritten !== buffer.byteLength) {
                throw new Error(`Failed to write complete render batch: ${bytesWritten}/${buffer.byteLength}`);
            }
            accessHandle.flush();
            this.fileCount = Math.max(this.fileCount, index + 1);
        } catch (error) {
            this.fallbackToMemory(index, data, error);
        }
    }

    read(index: number, target?: Float32Array): Float32Array | null {
        const accessHandle = this.accessHandles.get(index);
        if (!accessHandle) {
            const memoryBatch = this.memoryBatches.get(index);
            if (!memoryBatch) {
                return null;
            }
            if (target !== undefined && target.length >= memoryBatch.length) {
                target.set(memoryBatch);
                return target.subarray(0, memoryBatch.length);
            }
            return memoryBatch.slice();
        }
        const size = accessHandle.getSize();
        if (size === 0) {
            return null;
        }
        const buffer = target !== undefined && target.buffer.byteLength >= size
            ? target.buffer
            : new ArrayBuffer(size);
        accessHandle.read(new Uint8Array(buffer, 0, size), { at: 0 });
        return new Float32Array(buffer, 0, size / Float32Array.BYTES_PER_ELEMENT);
    }
}

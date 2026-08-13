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

let opfsUnavailableWarned = false;
let opfsRuntimeUnavailable = false;
const OPFS_PROBE_FILE = 'probe';

export const markLeaksOpfsUnavailable = (error?: unknown): void => {
    opfsRuntimeUnavailable = true;
    if (opfsUnavailableWarned) {
        return;
    }
    opfsUnavailableWarned = true;
    // eslint-disable-next-line no-console
    console.warn('[leaks] OPFS unavailable, non-cached loading requires user confirmation.', error);
};

// 块路径算法或 OPFS 存储结构发生变化时需要升级此前端缓存版本。
// 该版本会参与生成 storageKey，确保不兼容的数据不会复用旧缓存。
export const BLOCK_PATH_CACHE_VERSION = 'v1';

export const normalizeLeaksFileHash = (fileHash: unknown): string => {
    if (typeof fileHash !== 'string') {
        return '';
    }
    const normalized = fileHash.trim().toLowerCase();
    return /^[0-9a-f]{64}(?:-[a-z0-9_-]+){0,2}$/.test(normalized) ? normalized : '';
};

export const createBlockPathCacheStorageKey = (fileHash: string, mainThread: boolean = false): string =>
    `${mainThread ? 'main-thread' : 'main'}-cache-${BLOCK_PATH_CACHE_VERSION}-${fileHash}`;

export const createLeaksOpfsRuntimeId = (): string => {
    const randomId = globalThis.crypto?.randomUUID?.();
    if (randomId) {
        return randomId;
    }
    return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
};

export const isLeaksOpfsEnabled = (): boolean => {
    const globalFlag = (globalThis as { LEAKS_OPFS_ENABLED?: boolean }).LEAKS_OPFS_ENABLED;
    if (globalFlag === false || opfsRuntimeUnavailable) {
        return false;
    }
    const available = typeof navigator !== 'undefined' &&
        typeof navigator.storage?.getDirectory === 'function';
    if (!available && !opfsUnavailableWarned) {
        markLeaksOpfsUnavailable();
    }
    return available;
};

export const getLeaksOpfsRoot = async (): Promise<FileSystemDirectoryHandle | null> => {
    if (!isLeaksOpfsEnabled()) {
        return null;
    }
    try {
        return await navigator.storage.getDirectory();
    } catch (error) {
        markLeaksOpfsUnavailable(error);
        return null;
    }
};

export const getLeaksOpfsDirectory = async (name: string): Promise<FileSystemDirectoryHandle | null> => {
    const root = await getLeaksOpfsRoot();
    if (!root) {
        return null;
    }
    try {
        return await root.getDirectoryHandle(name, { create: true });
    } catch (error) {
        markLeaksOpfsUnavailable(error);
        return null;
    }
};

const removeProbeDirectory = async (
    root: FileSystemDirectoryHandle,
    directoryName: string,
): Promise<void> => {
    try {
        await root.removeEntry?.(directoryName, { recursive: true });
    } catch {
        // Probe cleanup must not change the capability result.
    }
};

const createProbeDirectoryName = (): string => `leaks-opfs-probe-${createLeaksOpfsRuntimeId()}`;

export const checkLeaksOpfsAvailability = async (): Promise<boolean> => {
    const root = await getLeaksOpfsRoot();
    if (!root) {
        return false;
    }
    const directoryName = createProbeDirectoryName();
    try {
        const directory = await root.getDirectoryHandle(directoryName, { create: true });
        const file = await directory.getFileHandle(OPFS_PROBE_FILE, { create: true });
        const writable = await file.createWritable();
        await writable.write(new Uint8Array([1]));
        await writable.close();
        return true;
    } catch (error) {
        markLeaksOpfsUnavailable(error);
        return false;
    } finally {
        await removeProbeDirectory(root, directoryName);
    }
};

export const checkLeaksOpfsSyncAvailability = async (): Promise<boolean> => {
    const root = await getLeaksOpfsRoot();
    if (!root) {
        return false;
    }
    const directoryName = createProbeDirectoryName();
    let accessHandle: FileSystemSyncAccessHandle | null = null;
    try {
        const directory = await root.getDirectoryHandle(directoryName, { create: true });
        const file = await directory.getFileHandle(OPFS_PROBE_FILE, { create: true });
        accessHandle = await file.createSyncAccessHandle();
        accessHandle.truncate(0);
        const probe = new Uint8Array([1]);
        if (accessHandle.write(probe, { at: 0 }) !== probe.byteLength) {
            throw new Error('Failed to write OPFS probe data');
        }
        accessHandle.flush();
        const result = new Uint8Array(1);
        if (accessHandle.read(result, { at: 0 }) !== result.byteLength || result[0] !== probe[0]) {
            throw new Error('Failed to read OPFS probe data');
        }
        return true;
    } catch (error) {
        markLeaksOpfsUnavailable(error);
        return false;
    } finally {
        try {
            accessHandle?.close();
        } catch {
            // A failed probe handle no longer needs to be reused.
        }
        await removeProbeDirectory(root, directoryName);
    }
};

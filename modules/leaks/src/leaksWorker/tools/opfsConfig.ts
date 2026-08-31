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
const OPFS_OPERATION_ATTEMPTS = 2;
const OPFS_RETRY_DELAY_MS = 20;
const DEFINITIVE_OPFS_UNAVAILABLE_ERRORS = new Set(['SecurityError', 'NotSupportedError']);

const getErrorName = (error: unknown): string => (
    error !== null && typeof error === 'object' && 'name' in error && typeof error.name === 'string'
        ? error.name
        : ''
);

export const isDefinitiveLeaksOpfsUnavailableError = (error: unknown): boolean =>
    DEFINITIVE_OPFS_UNAVAILABLE_ERRORS.has(getErrorName(error));

export const markLeaksOpfsUnavailable = (error?: unknown): void => {
    opfsRuntimeUnavailable = true;
    if (opfsUnavailableWarned) {
        return;
    }
    opfsUnavailableWarned = true;
    // eslint-disable-next-line no-console
    console.warn('[leaks] OPFS unavailable, non-cached loading requires user confirmation.', error);
};

export const reportLeaksOpfsAccessFailure = (operation: string, error: unknown): boolean => {
    if (isDefinitiveLeaksOpfsUnavailableError(error)) {
        markLeaksOpfsUnavailable(error);
        return true;
    }
    // eslint-disable-next-line no-console
    console.warn(`[leaks] Transient OPFS failure during ${operation}; the next load may retry.`, error);
    return false;
};

const waitBeforeOpfsRetry = async (): Promise<void> => {
    await new Promise<void>(resolve => setTimeout(resolve, OPFS_RETRY_DELAY_MS));
};

interface OpfsOperationResult<T> {
    status: OpfsAvailabilityStatus;
    value: T | null;
}

const runOpfsOperationWithStatus = async <T>(
    operation: string,
    action: () => Promise<T>,
): Promise<OpfsOperationResult<T>> => {
    let lastError: unknown;
    for (let attempt = 0; attempt < OPFS_OPERATION_ATTEMPTS; attempt++) {
        try {
            return { status: 'available', value: await action() };
        } catch (error) {
            lastError = error;
            if (isDefinitiveLeaksOpfsUnavailableError(error)) {
                markLeaksOpfsUnavailable(error);
                return { status: 'unavailable', value: null };
            }
            if (attempt + 1 < OPFS_OPERATION_ATTEMPTS) {
                await waitBeforeOpfsRetry();
            }
        }
    }
    reportLeaksOpfsAccessFailure(operation, lastError);
    return { status: 'transient', value: null };
};

const runOpfsOperation = async <T>(operation: string, action: () => Promise<T>): Promise<T | null> => {
    return (await runOpfsOperationWithStatus(operation, action)).value;
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
    return runOpfsOperation('root directory access', () => navigator.storage.getDirectory());
};

export const getLeaksOpfsDirectory = async (name: string): Promise<FileSystemDirectoryHandle | null> => {
    const root = await getLeaksOpfsRoot();
    if (!root) {
        return null;
    }
    return runOpfsOperation('directory access', () => root.getDirectoryHandle(name, { create: true }));
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

const runOpfsProbe = async (
    probe: (root: FileSystemDirectoryHandle, directoryName: string) => Promise<void>,
): Promise<OpfsAvailabilityStatus> => {
    if (!isLeaksOpfsEnabled()) {
        return 'unavailable';
    }
    const rootResult = await runOpfsOperationWithStatus(
        'root directory access',
        () => navigator.storage.getDirectory(),
    );
    if (rootResult.status !== 'available' || !rootResult.value) {
        return rootResult.status;
    }
    const root = rootResult.value;
    let lastError: unknown;
    for (let attempt = 0; attempt < OPFS_OPERATION_ATTEMPTS; attempt++) {
        const directoryName = createProbeDirectoryName();
        try {
            await probe(root, directoryName);
            return 'available';
        } catch (error) {
            lastError = error;
            if (isDefinitiveLeaksOpfsUnavailableError(error)) {
                markLeaksOpfsUnavailable(error);
                return 'unavailable';
            }
        } finally {
            await removeProbeDirectory(root, directoryName);
        }
        if (attempt + 1 < OPFS_OPERATION_ATTEMPTS) {
            await waitBeforeOpfsRetry();
        }
    }
    reportLeaksOpfsAccessFailure('capability probe', lastError);
    return 'transient';
};

export const checkLeaksOpfsAvailability = async (): Promise<OpfsAvailabilityStatus> => {
    return runOpfsProbe(async (root, directoryName) => {
        const directory = await root.getDirectoryHandle(directoryName, { create: true });
        const file = await directory.getFileHandle(OPFS_PROBE_FILE, { create: true });
        const writable = await file.createWritable();
        await writable.write(new Uint8Array([1]));
        await writable.close();
    });
};

export const checkLeaksOpfsSyncAvailability = async (): Promise<OpfsAvailabilityStatus> => {
    return runOpfsProbe(async (root, directoryName) => {
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
        } finally {
            try {
                accessHandle?.close();
            } catch {
                // A failed probe handle no longer needs to be reused.
            }
        }
    });
};

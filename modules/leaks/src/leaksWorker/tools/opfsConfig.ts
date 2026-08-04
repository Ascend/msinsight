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
    if (globalFlag === false) {
        return false;
    }
    const available = typeof navigator !== 'undefined' &&
        typeof navigator.storage?.getDirectory === 'function';
    if (!available && !opfsUnavailableWarned) {
        opfsUnavailableWarned = true;
        // eslint-disable-next-line no-console
        console.warn('[leaks] OPFS unavailable, falling back to in-memory block path storage.');
    }
    return available;
};

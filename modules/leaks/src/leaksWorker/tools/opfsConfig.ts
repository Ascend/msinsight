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

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
import type { FrameTree, FrameTreeNodeDTO, FrameTreePage, FrameTreePageRequest, FrameTreeQuery } from './types';
import { validateFrameTree } from './model';

const MAX_NODES = 100000;
export class FrameTreeTimeoutError extends Error {}
const abortError = (): Error => new DOMException('Frame tree query cancelled', 'AbortError');
const checkActive = (signal: AbortSignal): void => { if (signal.aborted) throw abortError(); };

function withTimeout<T>(operation: Promise<T>, signal: AbortSignal, timeout: number): Promise<T> {
    return new Promise((resolve, reject) => {
        const abort = (): void => finish(() => reject(abortError()));
        const timer = setTimeout(() => finish(() => reject(new FrameTreeTimeoutError('Frame tree request timed out'))), timeout);
        const finish = (callback: () => void): void => {
            clearTimeout(timer);
            signal.removeEventListener('abort', abort);
            callback();
        };
        signal.addEventListener('abort', abort, { once: true });
        operation.then(value => finish(() => resolve(value)), error => finish(() => reject(error)));
        if (signal.aborted) abort();
    });
}

export async function loadFrameTree(
    query: FrameTreeQuery, request: FrameTreePageRequest, signal: AbortSignal,
    progress?: (received: number, total: number) => void,
): Promise<FrameTree> {
    if (!query.deviceId || !Number.isSafeInteger(query.eventId) || query.eventId < 0 ||
        typeof query.includeStatic !== 'boolean' || !Number.isSafeInteger(query.pageSize) ||
        query.pageSize < 1 || query.pageSize > 1000) throw new Error('Invalid frame tree query');
    const context = Object.freeze({ ...query });
    const collected: FrameTreeNodeDTO[] = [];
    const seen = new Set<string>();
    let total = 0;
    for (let currentPage = 1; currentPage === 1 || collected.length < total; currentPage++) {
        checkActive(signal);
        const params = Object.freeze({ ...context, currentPage });
        let page: FrameTreePage;
        try {
            page = await withTimeout(request(params, signal), signal, 30000);
        } catch (error) {
            checkActive(signal);
            if (!(error instanceof FrameTreeTimeoutError || error instanceof TypeError)) throw error;
            await withTimeout(new Promise(resolve => setTimeout(resolve, 500)), signal, 1000);
            checkActive(signal);
            page = await withTimeout(request(params, signal), signal, 30000);
        }
        checkActive(signal);
        if (!page || !Number.isSafeInteger(page.total) || page.total < 1 || !Array.isArray(page.nodes)) {
            throw new Error(`Invalid memory frame tree page ${currentPage}`);
        }
        if (page.total > MAX_NODES) throw new Error('Memory frame tree exceeds the 100,000-node display limit');
        if (currentPage === 1) total = page.total;
        const expected = Math.min(context.pageSize, total - collected.length);
        if (page.total !== total || page.nodes.length !== expected) throw new Error(`Incomplete memory frame tree page ${currentPage}`);
        for (const node of page.nodes) {
            if (!node || typeof node.nodeId !== 'string' || seen.has(node.nodeId)) {
                throw new Error(`Duplicate or invalid memory frame tree node on page ${currentPage}`);
            }
            seen.add(node.nodeId);
            collected.push(node);
        }
        progress?.(collected.length, total);
    }
    checkActive(signal);
    const tree = validateFrameTree(collected, total);
    checkActive(signal);
    return tree;
}

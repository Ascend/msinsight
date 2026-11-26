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
import type { ThreadTrace } from '../entity/data';

// Match PythonStackHelper::RestoreThreadIdAndMetaType, not cache-file suffixes.
export function isTextTrace(params: { metaType?: unknown; threadId?: unknown }): boolean {
    if (typeof params.threadId === 'string' && params.threadId.startsWith('python_stack:')) {
        return params.threadId.startsWith('python_stack:text:') && params.threadId.length > 'python_stack:text:'.length;
    }
    return params.metaType === 'TEXT';
}

export function hasTextTraceSource(metadata: unknown): boolean {
    if (metadata === null || typeof metadata !== 'object') { return false; }
    const thread = metadata as { metaType?: string; threadId?: string; threadSourceList?: Array<{ metaType?: string; threadId?: string }> };
    return !Array.isArray(thread.threadSourceList)
        ? isTextTrace(thread)
        : thread.threadSourceList.some(source => isTextTrace({ ...source, metaType: source.metaType ?? thread.metaType }));
}

export interface ThreadTraceResponse {
    data: ThreadTrace[][];
    maxDepth: number;
    currentMaxDepth: number;
    havePythonFunction?: boolean;
    isLoading?: boolean;
}

interface Limits {
    entries: number;
    slices: number;
    estimatedBytes: number;
}

// Wide, shallow traces may have hundreds of lanes. Bound payload size as well as lane count.
const DEFAULT_LIMITS: Limits = { entries: 1024, slices: 100_000, estimatedBytes: 32 * 1024 * 1024 };

function sourceIdentity(source: unknown): unknown {
    if (source === null || typeof source !== 'object') { return source; }
    // Explorer children do not participate in routing and can contain an entire file tree.
    return Object.fromEntries(Object.entries(source).filter(([key]) => key !== 'children').sort(([a], [b]) => a.localeCompare(b)));
}

export function threadTraceRequestKey(source: unknown, params: Record<string, unknown>): string {
    const sortedParams = Object.fromEntries(Object.keys(params).sort().map(key => [
        key, key === 'dataSource' ? sourceIdentity(params[key]) : params[key],
    ]));
    return JSON.stringify([sourceIdentity(source), sortedParams]);
}

interface Entry {
    response: ThreadTraceResponse;
    slices: number;
    bytes: number;
}

/** Session-owned raw responses; mapped bars remain local to each chart/view. */
export class ThreadTraceCache {
    private readonly entries = new Map<string, Entry>();
    private readonly pending = new Map<string, Promise<ThreadTraceResponse | undefined>>();
    private generation = 0;
    private sliceCount = 0;
    private bytes = 0;

    constructor(private readonly limits: Limits = DEFAULT_LIMITS) {}

    fetch(key: string, load: () => Promise<ThreadTraceResponse | undefined>): Promise<ThreadTraceResponse | undefined> {
        const cached = this.entries.get(key);
        if (cached !== undefined) {
            this.entries.delete(key);
            this.entries.set(key, cached);
            return Promise.resolve(cached.response);
        }
        const pending = this.pending.get(key);
        if (pending !== undefined) { return pending; }
        // Limit bookkeeping, not the number of lanes the user can load.
        if (this.pending.size >= this.limits.entries) { return load(); }
        const generation = this.generation;
        const request = Promise.resolve().then(load).then(response => {
            if (generation === this.generation) { this.store(key, response); }
            return response;
        }).finally(() => {
            if (this.pending.get(key) === request) { this.pending.delete(key); }
        });
        this.pending.set(key, request);
        return request;
    }

    clear(): void {
        this.generation++;
        this.entries.clear();
        this.pending.clear();
        this.sliceCount = 0;
        this.bytes = 0;
    }

    private store(key: string, response: ThreadTraceResponse | undefined): void {
        if (response === undefined || response === null || response.isLoading === true ||
            !Array.isArray(response.data) || !Number.isFinite(response.maxDepth) || !Number.isFinite(response.currentMaxDepth)) {
            return;
        }
        let slices = 0;
        let bytes = 256 + key.length * 2 + response.data.length * 32;
        for (const row of response.data) {
            if (!Array.isArray(row)) { return; }
            slices += row.length;
            if (slices > this.limits.slices) { return; }
            for (const slice of row) {
                if (slice === null || typeof slice !== 'object') { return; }
                bytes += 192;
                // Wire rows are plain JSON objects; avoid an extra values array per slice.
                for (const field in slice) {
                    const value = slice[field as keyof ThreadTrace];
                    if (typeof value === 'string') { bytes += value.length * 2; }
                    // Unknown nested payloads have no bounded estimate; load normally without retaining them.
                    if (typeof value === 'object' && value !== null) { return; }
                }
                if (bytes > this.limits.estimatedBytes) { return; }
            }
        }
        if (bytes > this.limits.estimatedBytes || this.limits.entries < 1) { return; }
        while (this.entries.size > 0 && (this.entries.size >= this.limits.entries ||
            this.sliceCount + slices > this.limits.slices || this.bytes + bytes > this.limits.estimatedBytes)) {
            const oldest = this.entries.keys().next().value as string;
            const removed = this.entries.get(oldest) as Entry;
            this.sliceCount -= removed.slices;
            this.bytes -= removed.bytes;
            this.entries.delete(oldest);
        }
        this.entries.set(key, { response, slices, bytes });
        this.sliceCount += slices;
        this.bytes += bytes;
    }
}

/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { getPmuDataSourceKey, type PmuCounterRequest, type PmuCounterResponse } from '../entity/pmuCounter';

interface CacheEntry {
    response: PmuCounterResponse;
    bytes: number;
}

export class PmuRequestCancelled extends Error {
    constructor() { super('PMU data source changed'); }
}

export function createPmuRequestKey(dataSource: DataSource, params: PmuCounterRequest): string {
    return JSON.stringify([getPmuDataSourceKey(dataSource), params.dbPath, params.counterId,
        params.startTime, params.endTime, params.targetBuckets]);
}

/** Stores data coordinates only. Offset-dependent display coordinates never enter this cache. */
export class PmuCounterCache {
    private readonly entries = new Map<string, CacheEntry>();
    private readonly pending = new Map<string, Promise<PmuCounterResponse>>();
    private readonly queue: Array<() => void> = [];
    private active = 0;
    private bytes = 0;
    private currentGeneration = 0;

    constructor(private readonly maxBytes = 32 * 1024 * 1024, private readonly concurrency = 6) {
        if (maxBytes < 0 || concurrency < 1) { throw new Error('Invalid PMU cache limits'); }
    }

    get generation(): number { return this.currentGeneration; }

    /** Read completed data synchronously without changing LRU order during render. */
    peek(dataSource: DataSource, params: PmuCounterRequest): PmuCounterResponse | undefined {
        return this.entries.get(createPmuRequestKey(dataSource, params))?.response;
    }

    fetch(dataSource: DataSource, params: PmuCounterRequest): Promise<PmuCounterResponse> {
        const key = createPmuRequestKey(dataSource, params);
        const cached = this.entries.get(key);
        if (cached !== undefined) {
            this.entries.delete(key);
            this.entries.set(key, cached);
            return Promise.resolve(cached.response);
        }
        const pending = this.pending.get(key);
        if (pending !== undefined) { return pending; }
        const generation = this.currentGeneration;
        const request = this.load(dataSource, params, generation).then(response => {
            if (generation !== this.currentGeneration) { throw new PmuRequestCancelled(); }
            const bytes = JSON.stringify(response).length * 2;
            if (bytes <= this.maxBytes) {
                this.entries.set(key, { response, bytes });
                this.bytes += bytes;
                this.evict();
            }
            return response;
        }).finally(() => {
            if (this.pending.get(key) === request) { this.pending.delete(key); }
        });
        this.pending.set(key, request);
        return request;
    }

    clear(): void {
        this.currentGeneration++;
        this.entries.clear();
        this.pending.clear();
        this.bytes = 0;
    }

    private evict(): void {
        while (this.bytes > this.maxBytes && this.entries.size !== 0) {
            const oldest = this.entries.entries().next().value as [string, CacheEntry];
            this.bytes -= oldest[1].bytes;
            this.entries.delete(oldest[0]);
        }
    }

    private async load(dataSource: DataSource, params: PmuCounterRequest, generation: number): Promise<PmuCounterResponse> {
        await this.acquire();
        try {
            if (generation !== this.currentGeneration) { throw new PmuRequestCancelled(); }
            const result = await window.request(dataSource, { command: 'unit/pmu-counter', params: { ...params } }, { silent: true });
            if (!result || !Array.isArray(result.segments) ||
                !['ok', 'empty', 'unsupported', 'invalid-data'].includes(result.status)) {
                throw new Error('Invalid PMU counter response');
            }
            return result as PmuCounterResponse;
        } finally {
            this.release();
        }
    }

    private acquire(): Promise<void> {
        if (this.active < this.concurrency) {
            this.active++;
            return Promise.resolve();
        }
        return new Promise(resolve => this.queue.push(resolve));
    }

    private release(): void {
        const next = this.queue.shift();
        if (next !== undefined) { next(); } else { this.active--; }
    }
}

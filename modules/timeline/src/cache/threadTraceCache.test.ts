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
import { hasTextTraceSource, isTextTrace, ThreadTraceCache, threadTraceRequestKey, type ThreadTraceResponse } from './threadTraceCache';

const response = (id = 'slice', count = 1): ThreadTraceResponse => ({
    data: [Array.from({ length: count }, () => ({ id, name: 'operator', startTime: 10, endTime: 20, duration: 10, depth: 0, threadId: '2', cname: '', dbPath: '' }))],
    maxDepth: 1,
    currentMaxDepth: 1,
    havePythonFunction: true,
});

describe('session thread trace cache', () => {
    it.each([response(), { data: [], maxDepth: 0, currentMaxDepth: 0 }])('shares in-flight work and retains complete or empty responses (%o)', async data => {
        const cache = new ThreadTraceCache();
        const load = jest.fn().mockResolvedValue(data);
        const first = cache.fetch('lane', load);
        const second = cache.fetch('lane', load);
        expect(second).toBe(first);
        expect(await first).toBe(data);
        expect(await cache.fetch('lane', load)).toBe(data);
        expect(load).toHaveBeenCalledTimes(1);
    });

    it('does not retain rejected, missing or still-loading responses', async () => {
        const cache = new ThreadTraceCache();
        const load = jest.fn().mockRejectedValueOnce(new Error('retry')).mockResolvedValueOnce(undefined)
            .mockResolvedValueOnce({ ...response(), isLoading: true }).mockResolvedValue(response());
        await expect(cache.fetch('lane', load)).rejects.toThrow('retry');
        expect(await cache.fetch('lane', load)).toBeUndefined();
        expect((await cache.fetch('lane', load))?.isLoading).toBe(true);
        expect((await cache.fetch('lane', load))?.data[0][0].id).toBe('slice');
        await cache.fetch('lane', load);
        expect(load).toHaveBeenCalledTimes(4);
    });

    it('does not serialize a different viewport behind an unfinished cold window', async () => {
        const cache = new ThreadTraceCache();
        const source = { remote: 'local', projectName: 'project' };
        const lane = { cardId: 'rank', threadId: 'thread', metaType: 'TEXT' };
        let finishOld!: (data: ThreadTraceResponse) => void;
        let oldFinished = false;
        const old = cache.fetch(threadTraceRequestKey(source, { ...lane, startTime: 0, endTime: 100 }),
            () => new Promise(resolve => { finishOld = resolve; })).then(data => {
            oldFinished = true;
            return data;
        });
        const current = cache.fetch(threadTraceRequestKey(source, { ...lane, startTime: 100, endTime: 200 }),
            () => Promise.resolve(response('current')));
        expect((await current)?.data[0][0].id).toBe('current');
        expect(oldFinished).toBe(false);
        finishOld(response('old'));
        expect((await old)?.data[0][0].id).toBe('old');
    });

    it('clears in-flight generations without letting the old completion replace or remove new work', async () => {
        const cache = new ThreadTraceCache();
        let finishOld!: (data: ThreadTraceResponse) => void;
        let finishNew!: (data: ThreadTraceResponse) => void;
        const old = cache.fetch('lane', () => new Promise(resolve => { finishOld = resolve; }));
        await Promise.resolve();
        cache.clear();
        const current = cache.fetch('lane', () => new Promise(resolve => { finishNew = resolve; }));
        await Promise.resolve();
        finishOld(response('old'));
        expect((await old)?.data[0][0].id).toBe('old');
        expect(cache.fetch('lane', jest.fn())).toBe(current);
        finishNew(response('new'));
        await current;
        expect((await cache.fetch('lane', jest.fn()))?.data[0][0].id).toBe('new');
    });

    it('evicts the least recently used entry and resets all budgets on clear', async () => {
        const cache = new ThreadTraceCache({ entries: 2, slices: 2, estimatedBytes: 4096 });
        const load = jest.fn().mockResolvedValue(response());
        await cache.fetch('a', load);
        await cache.fetch('b', load);
        await cache.fetch('a', load);
        await cache.fetch('c', load);
        await cache.fetch('a', load);
        expect(load).toHaveBeenCalledTimes(3);
        await cache.fetch('b', load);
        expect(load).toHaveBeenCalledTimes(4);
        cache.clear();
        await cache.fetch('a', load);
        await cache.fetch('b', load);
        await cache.fetch('a', load);
        expect(load).toHaveBeenCalledTimes(6);
    });

    it.each([
        [{ entries: 1, slices: 0, estimatedBytes: 4096 }, false],
        [{ entries: 1, slices: 10, estimatedBytes: 1 }, false],
        [{ entries: 0, slices: 10, estimatedBytes: 4096 }, false],
        [{ entries: 1, slices: 10, estimatedBytes: 4096 }, true],
    ] as const)('returns complete responses without retaining oversized or nested payloads (%o, nested: %s)', async (limits, nested) => {
        const cache = new ThreadTraceCache(limits);
        const data = response('large', 4);
        if (nested) { Object.assign(data.data[0][0], { unknownNested: { value: 'not estimated' } }); }
        const load = jest.fn().mockResolvedValue(data);
        expect(await cache.fetch('lane', load)).toBe(data);
        expect(await cache.fetch('lane', load)).toBe(data);
        expect(load).toHaveBeenCalledTimes(2);
    });

    it('keeps both wide traces and bounded in-flight bookkeeping without limiting functionality', async () => {
        const cache = new ThreadTraceCache();
        const load = jest.fn().mockResolvedValue(response());
        for (let i = 0; i < 768; i++) { await cache.fetch(`lane-${i}`, load); }
        for (let i = 0; i < 768; i++) { await cache.fetch(`lane-${i}`, load); }
        expect(load).toHaveBeenCalledTimes(768);
        const bounded = new ThreadTraceCache({ entries: 1, slices: 10, estimatedBytes: 4096 });
        let finish!: (data: ThreadTraceResponse) => void;
        const pending = bounded.fetch('busy', () => new Promise(resolve => { finish = resolve; }));
        expect(await bounded.fetch('other', load)).toEqual(response());
        finish(response());
        await pending;
    });
});

describe('thread trace request identity', () => {
    it.each([
        [{ metaType: 'TEXT', threadId: '2' }, true],
        [{ metaType: 'PYTORCH_API_PYTHON_STACK', threadId: 'python_stack:text:2' }, true],
        [{ metaType: 'TEXT', threadId: 'python_stack:2' }, false],
        [{ metaType: 'PYTORCH_API', threadId: '2' }, false],
        [{ dbPath: 'json.db' }, false],
    ])('uses restored source identity rather than file suffix %o', (params, eligible) => {
        expect(isTextTrace({ metaType: undefined, ...params })).toBe(eligible);
    });
    it('lets explicit all-native sources override an outer TEXT label', () => {
        expect(hasTextTraceSource({ metaType: 'TEXT', threadSourceList: [{ metaType: 'PYTORCH_API', threadId: '2' }] })).toBe(false);
        expect(hasTextTraceSource(null)).toBe(false);
        expect(hasTextTraceSource({ metaType: 'TEXT', threadSourceList: [] })).toBe(false);
    });
    it.each([null, {}, 'legacy'])('falls back to the outer identity for a non-array source list %p', threadSourceList => {
        expect(hasTextTraceSource({ metaType: 'TEXT', threadSourceList })).toBe(true);
        expect(hasTextTraceSource({ metaType: 'PYTORCH_API', threadSourceList })).toBe(false);
    });
    const source = { remote: 'local', port: 9000, projectName: 'project', projectPath: ['p'], children: [] };
    const params = { cardId: 'rank', dbPath: 'a.db', processId: 'p', threadId: 't', metaType: 'TEXT', startTime: 0, endTime: 100, timePerPx: 1 };

    it('normalizes property order and ignores explorer children only', () => {
        const key = threadTraceRequestKey(source, { ...params, dataSource: source });
        expect(threadTraceRequestKey({ ...source, children: [{ name: 'new child' }] }, { dataSource: source, ...params })).toBe(key);
    });

    it.each([
        ...[{ cardId: 'baseline' }, { dbPath: 'b.db' }, { processId: 'other' }, { threadId: 'other' },
            { threadIdList: ['a', 'b'] }, { metaType: 'PYTORCH_API_PYTHON_STACK' }, { startTime: 1 }, { endTime: 200 },
            { timePerPx: 2 }, { isFilterPythonFunction: true }, { isHideFlagEvents: true }].map(change => [source, { ...params, ...change }] as const),
        ...[{ remote: 'other' }, { port: 9001 }, { projectName: 'other' }, { projectPath: ['q'] }, { isBaseLine: true }]
            .map(change => [{ ...source, ...change }, params] as const),
    ])('isolates routing and request conditions %o %o', (requestSource, requestParams) => {
        expect(threadTraceRequestKey(requestSource, requestParams)).not.toBe(threadTraceRequestKey(source, params));
    });
});

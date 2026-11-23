/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { PmuCounterCache, PmuRequestCancelled, createPmuRequestKey } from './pmuCounterCache';
import type { PmuCounterRequest, PmuCounterResponse } from '../entity/pmuCounter';
import { pmuSource, pmuResponse, deferred } from '../components/charts/__test__/pmuTestUtils';

const request: PmuCounterRequest = { dbPath: '/db', rankId: 'rank0', counterId: '1', startTime: 0, endTime: 100, targetBuckets: 256 };

describe('PMU dedicated cache', () => {
    beforeEach(() => { window.request = jest.fn(); });

    it('deduplicates inflight reads and caches the direct response body', async () => {
        const pending = deferred<PmuCounterResponse>();
        (window.request as jest.Mock).mockReturnValue(pending.promise);
        const cache = new PmuCounterCache();
        const first = cache.fetch(pmuSource, request);
        const second = cache.fetch(pmuSource, { ...request, rankId: 'rank1' });
        expect(cache.peek(pmuSource, request)).toBeUndefined();
        expect(first).toBe(second);
        await Promise.resolve();
        expect(window.request).toHaveBeenCalledTimes(1);
        const body = pmuResponse();
        pending.resolve(body);
        expect(await first).toBe(body);
        expect(cache.peek(pmuSource, request)).toBe(body);
        expect(cache.peek({ ...pmuSource, port: 9001 }, request)).toBeUndefined();
        expect(await cache.fetch(pmuSource, request)).toBe(body);
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(window.request).toHaveBeenCalledWith(pmuSource, { command: 'unit/pmu-counter', params: request }, { silent: true });
    });

    it('isolates connections, databases, counters, ranges and widths', () => {
        const key = createPmuRequestKey(pmuSource, request);
        for (const change of [{ dbPath: '/other' }, { counterId: '2' },
            { startTime: 1 }, { endTime: 101 }, { targetBuckets: 512 }]) {
            expect(createPmuRequestKey(pmuSource, { ...request, ...change })).not.toBe(key);
        }
        expect(createPmuRequestKey({ ...pmuSource, port: 9001 }, request)).not.toBe(key);
    });

    it('discards an old response after reset without deleting a replacement inflight read', async () => {
        const old = deferred<PmuCounterResponse>();
        const next = deferred<PmuCounterResponse>();
        (window.request as jest.Mock).mockReturnValueOnce(old.promise).mockReturnValueOnce(next.promise);
        const cache = new PmuCounterCache();
        const oldResult = cache.fetch(pmuSource, request).catch(error => error);
        await Promise.resolve();
        cache.clear();
        expect(cache.peek(pmuSource, request)).toBeUndefined();
        const nextResult = cache.fetch(pmuSource, request);
        old.resolve(pmuResponse({ counterId: 'old' }));
        expect(await oldResult).toBeInstanceOf(PmuRequestCancelled);
        expect(cache.fetch(pmuSource, request)).toBe(nextResult);
        next.resolve(pmuResponse({ counterId: 'new' }));
        expect((await nextResult).counterId).toBe('new');
        expect((await cache.fetch(pmuSource, request)).counterId).toBe('new');
    });

    it('limits active requests to six and lets queued requests proceed', async () => {
        const jobs = Array.from({ length: 8 }, () => deferred<PmuCounterResponse>());
        (window.request as jest.Mock).mockImplementation((_source, payload) => jobs[Number(payload.params.counterId)].promise);
        const cache = new PmuCounterCache();
        const responses = jobs.map((_job, counterId) => cache.fetch(pmuSource, { ...request, counterId: String(counterId) }));
        await Promise.resolve();
        expect(window.request).toHaveBeenCalledTimes(6);
        jobs[0].resolve(pmuResponse());
        await responses[0];
        expect(window.request).toHaveBeenCalledTimes(7);
        jobs[1].resolve(pmuResponse());
        await responses[1];
        expect(window.request).toHaveBeenCalledTimes(8);
        jobs.forEach(job => job.resolve(pmuResponse()));
        await Promise.all(responses);
    });

    it('evicts the least recently used entry and keeps the byte limit', async () => {
        const body = pmuResponse();
        (window.request as jest.Mock).mockResolvedValue(body);
        const cache = new PmuCounterCache(JSON.stringify(body).length * 2 * 2);
        const read = (counterId: string): Promise<PmuCounterResponse> => cache.fetch(pmuSource, { ...request, counterId });
        await read('1'); await read('2'); await read('1'); await read('3'); await read('1');
        expect(window.request).toHaveBeenCalledTimes(3);
        await read('2');
        expect(window.request).toHaveBeenCalledTimes(4);
    });

    it('does not cache failed or malformed responses and allows retry', async () => {
        (window.request as jest.Mock).mockRejectedValueOnce(new Error('offline')).mockResolvedValueOnce({ data: [] }).mockResolvedValue(pmuResponse());
        const cache = new PmuCounterCache();
        await expect(cache.fetch(pmuSource, request)).rejects.toThrow('offline');
        await expect(cache.fetch(pmuSource, request)).rejects.toThrow('Invalid PMU');
        await expect(cache.fetch(pmuSource, request)).resolves.toMatchObject({ status: 'ok' });
    });
});

/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import type { NumaOverview } from '@/entities/numa/types';
import { NumaStore } from '@/features/overview/model/NumaStore';
import { FULL_TIME_RANGE } from '@/features/overview/model/timeRange';
import { deferred } from '@/testUtils/deferred';

const overview = (startTime: number): NumaOverview => ({
    range: { startTime, endTime: startTime + 10 },
    totalMetrics: [],
    sockets: [],
    connections: [],
});

describe('NumaStore', () => {
    it('loads the selected directory with the normalized Timeline range', async () => {
        const loader = jest.fn(async () => overview(100));
        const store = new NumaStore(loader);
        await store.setTimeAnalysisRange([10.2, 20.1]);
        await store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });

        expect(loader).toHaveBeenLastCalledWith({
            rankId: 'rank-0', dbPath: 'db-path', startTime: 10, endTime: 21,
        });
        expect(store.data).toEqual(overview(100));
    });

    it('does not issue an identical request twice', async () => {
        const loader = jest.fn(async () => overview(0));
        const store = new NumaStore(loader);
        await store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });
        await store.activate();
        expect(loader).toHaveBeenCalledTimes(1);
    });

    it('does not let an older response replace newer data', async () => {
        const first = deferred<NumaOverview>();
        const second = deferred<NumaOverview>();
        const store = new NumaStore(jest.fn()
            .mockReturnValueOnce(first.promise)
            .mockReturnValueOnce(second.promise));

        const firstLoad = store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'first' });
        const secondLoad = store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'second' });
        second.resolve(overview(200));
        await secondLoad;
        first.resolve(overview(100));
        await firstLoad;

        expect(store.data).toEqual(overview(200));
    });

    it('clears previous data and selection while reloading a new request', async () => {
        const next = deferred<NumaOverview>();
        const loader = jest.fn()
            .mockResolvedValueOnce(overview(100))
            .mockReturnValueOnce(next.promise);
        const store = new NumaStore(loader);
        await store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'first' });
        store.select({ kind: 'socket', id: 'socket-0', title: 'Socket 0', description: '', metrics: [] });

        const reload = store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'second' });

        expect(store.data).toBeNull();
        expect(store.selected).toBeNull();
        expect(store.loading).toBe(true);

        next.resolve(overview(200));
        await reload;
        expect(store.data).toEqual(overview(200));
    });

    it.each(['reset', 'incomplete directory'])('invalidates an in-flight request after %s', async (action) => {
        const pending = deferred<NumaOverview>();
        const store = new NumaStore(() => pending.promise);
        const loading = store.switchDirectory({ rankId: 'rank-0', selectedFilePath: 'db-path' });

        if (action === 'reset') store.reset();
        else await store.switchDirectory({ rankId: '', selectedFilePath: '' });
        pending.resolve(overview(100));
        await loading;

        expect(store.data).toBeNull();
        expect(store.loading).toBe(false);
        expect(store.analysisRange).toEqual(FULL_TIME_RANGE);
    });

    it('exposes request errors and allows the same request to retry', async () => {
        const loader = jest.fn()
            .mockRejectedValueOnce(new Error('query failed'))
            .mockResolvedValueOnce(overview(0));
        const store = new NumaStore(loader);
        const directory = { rankId: 'rank-0', selectedFilePath: 'db-path' };

        await store.switchDirectory(directory);
        expect(store.error).toBe('query failed');
        expect(store.loading).toBe(false);
        await store.switchDirectory(directory);

        expect(loader).toHaveBeenCalledTimes(2);
        expect(store.data).toEqual(overview(0));
    });
});

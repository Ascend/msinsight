/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { readFileSync } from 'fs';
import { join } from 'path';
import * as ts from 'typescript';

type WorkerApi = typeof import('./worker');

const query: BlockQuery = { sortBy: 'size', limit: 5 };
const queryResult: BlockQueryResult = {
    blocks: [], scannedEntries: 0, matchedEntries: 0, limit: 5, sortBy: 'size',
};
const createRenderData = (): RenderData => ({
    blocks: [], maxTimestamp: 10, minTimestamp: 0, maxSize: 20, minSize: 0,
});

const createHarness = (useWorker = true) => {
    let messageHandler: (event: { data: unknown }) => void = () => undefined;
    let dispose: () => void = () => undefined;
    const worker = {
        postMessage: jest.fn(),
        terminate: jest.fn(),
        addEventListener: jest.fn((type: string, handler: typeof messageHandler) => {
            expect(type).toBe('message');
            messageHandler = handler;
        }),
    };
    const mainThread = {
        loadMemoryBlockCacheHandler: jest.fn(),
        setMemoryBlockDataHandler: jest.fn(),
        queryBlocksHandler: jest.fn(),
        destroyHandler: jest.fn(),
    };
    const checkOpfs = jest.fn();
    const removeCaches = jest.fn();
    const packedData = { format: 'packed-test-data' };
    const transfers = [new ArrayBuffer(8)];
    const pack = jest.fn(() => packedData);
    const transferList = jest.fn(() => transfers);
    const dependencies: Record<string, unknown> = {
        '../tools/detection': { isWebGL2Supported: () => useWorker },
        '../tools/opfsConfig': { checkLeaksOpfsAvailability: checkOpfs },
        '../tools/packedBlockData': { packRenderData: pack, getPackedRenderDataTransferList: transferList },
        './mainThreadindex': { MainThreadRender: class { constructor() { return mainThread; } } },
        '../tools/BlockDataOPFS': { BlockDataOPFS: { removePersistentCachesBySourceHash: removeCaches } },
    };
    // CRA/Jest uses CommonJS; replace only the bundler's URL expression, not any bridge logic.
    const source = readFileSync(join(__dirname, 'worker.ts'), 'utf8')
        .replace('import.meta.url', JSON.stringify('file:///block-worker/worker.ts'));
    const compiled = ts.transpileModule(source, {
        compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
        fileName: 'worker.ts',
    }).outputText;
    const exports = {};
    const testModule = { exports, hot: { dispose: (callback: () => void) => { dispose = callback; } } };
    const evaluate = new Function('require', 'module', 'exports', 'Worker', 'window', `${compiled}
        return {
            api: exports,
            pendingCounts: () => [pendingWorkerLoads.size, pendingCacheLoads.size,
                pendingOpfsProbes.size, pendingCacheRemovals.size, pendingBlockQueries.size],
        };
    `);
    const loaded = evaluate((name: string) => {
        if (!(name in dependencies)) throw new Error(`Unexpected worker dependency: ${name}`);
        return dependencies[name];
    }, testModule, exports, class { constructor() { return worker; } }, { devicePixelRatio: 1 }) as {
        api: WorkerApi;
        pendingCounts: () => number[];
    };
    return {
        ...loaded, worker, mainThread, checkOpfs, removeCaches, packedData, transfers, pack, transferList,
        emit: (data: unknown) => messageHandler({ data }),
        dispose: () => dispose(),
        lastRequest: () => worker.postMessage.mock.calls[worker.postMessage.mock.calls.length - 1][0],
    };
};

describe('block worker query and TARGET cache compatibility', () => {
    it('correlates string query IDs and resolves the original result', async () => {
        const h = createHarness();
        const first = h.api.workerQueryBlocks(query);
        const firstId = h.lastRequest().requestId;
        const second = h.api.workerQueryBlocks(query);
        const secondId = h.lastRequest().requestId;
        expect(h.lastRequest()).toEqual({ type: 'queryBlocks', requestId: secondId, query });
        expect(typeof firstId).toBe('string');
        expect(secondId).not.toBe(firstId);
        h.emit({ type: 'queryBlocksResult', requestId: secondId, result: queryResult });
        h.emit({ type: 'queryBlocksResult', requestId: firstId, result: queryResult });
        await expect(first).resolves.toBe(queryResult);
        await expect(second).resolves.toBe(queryResult);
        expect(h.pendingCounts()).toEqual([0, 0, 0, 0, 0]);
    });

    it('rejects a query error and clears its pending entry', async () => {
        const h = createHarness();
        const completion = h.api.workerQueryBlocks(query);
        h.emit({ type: 'queryBlocksResult', requestId: h.lastRequest().requestId, error: 'query failed' });
        await expect(completion).rejects.toThrow('query failed');
        expect(h.pendingCounts()[4]).toBe(0);
    });

    it.each([undefined, null])('rejects an absent query payload (%s)', async result => {
        const h = createHarness();
        const completion = h.api.workerQueryBlocks(query);
        h.emit({ type: 'queryBlocksResult', requestId: h.lastRequest().requestId, result });
        await expect(completion).rejects.toThrow('Block worker returned an invalid query result');
        expect(h.pendingCounts()[4]).toBe(0);
    });

    it('ignores unknown, wrong-type and duplicate query IDs', async () => {
        const h = createHarness();
        const completion = h.api.workerQueryBlocks(query);
        const requestId = h.lastRequest().requestId;
        h.emit({ type: 'queryBlocksResult', requestId: 'unknown', result: queryResult });
        h.emit({ type: 'queryBlocksResult', requestId: 1, result: queryResult });
        expect(h.pendingCounts()[4]).toBe(1);
        h.emit({ type: 'queryBlocksResult', requestId, result: queryResult });
        h.emit({ type: 'queryBlocksResult', requestId, error: 'late duplicate' });
        await expect(completion).resolves.toBe(queryResult);
        expect(h.pendingCounts()[4]).toBe(0);
    });

    it.each(['hit', 'miss', 'transient', 'unavailable'] as BlockPathCacheLoadStatus[])(
        'preserves the %s cache status instead of returning boolean', async status => {
            const h = createHarness();
            const completion = h.api.workerLoadMemoryBlockCache({ fileHash: 'cache-source' });
            const generation = h.lastRequest().generation;
            h.emit({ type: 'blockPathCacheLoadCompleted', generation, status });
            await expect(completion).resolves.toBe(status);
            expect(h.pendingCounts()[1]).toBe(0);
        },
    );

    it('preserves cache miss defaults, cancellation and failure', async () => {
        const h = createHarness();
        const defaultMiss = h.api.workerLoadMemoryBlockCache({});
        h.emit({ type: 'blockPathCacheLoadCompleted', generation: h.lastRequest().generation });
        await expect(defaultMiss).resolves.toBe('miss');
        const cancelled = h.api.workerLoadMemoryBlockCache({});
        h.emit({ type: 'renderCancelled', generation: h.lastRequest().generation });
        await expect(cancelled).resolves.toBe('miss');
        const failed = h.api.workerLoadMemoryBlockCache({});
        h.emit({ type: 'renderFailed', generation: h.lastRequest().generation, error: 'cache failed' });
        await expect(failed).rejects.toThrow('cache failed');
        expect(h.pendingCounts()[1]).toBe(0);
    });

    it('keeps numeric probe/removal requests separate from query IDs', async () => {
        const h = createHarness();
        const probe = h.api.workerCheckOpfsAvailability();
        const probeId = h.lastRequest().requestId;
        const removal = h.api.workerRemoveMemoryBlockCaches({ fileHash: 'cache-source' });
        const removalId = h.lastRequest().requestId;
        const queried = h.api.workerQueryBlocks(query);
        const queryId = h.lastRequest().requestId;
        expect(typeof probeId).toBe('number');
        expect(typeof removalId).toBe('number');
        h.emit({ type: 'opfsAvailabilityChecked', requestId: String(probeId), availabilityStatus: 'unavailable' });
        h.emit({ type: 'blockPathCacheRemovalCompleted', requestId: String(removalId) });
        h.emit({ type: 'opfsAvailabilityChecked', requestId: queryId, availabilityStatus: 'unavailable' });
        expect(h.pendingCounts()).toEqual([0, 0, 1, 1, 1]);
        h.emit({ type: 'opfsAvailabilityChecked', requestId: probeId, availabilityStatus: 'available' });
        h.emit({ type: 'blockPathCacheRemovalCompleted', requestId: removalId });
        h.emit({ type: 'queryBlocksResult', requestId: queryId, result: queryResult });
        await expect(probe).resolves.toBe('available');
        await expect(removal).resolves.toBeUndefined();
        await expect(queried).resolves.toBe(queryResult);
        expect(h.pendingCounts()).toEqual([0, 0, 0, 0, 0]);
    });

    it('retains transient probe defaults and removal failure', async () => {
        const h = createHarness();
        const probe = h.api.workerCheckOpfsAvailability();
        h.emit({ type: 'opfsAvailabilityChecked', requestId: h.lastRequest().requestId });
        await expect(probe).resolves.toBe('transient');
        const removal = h.api.workerRemoveMemoryBlockCaches({ fileHash: 'cache-source' });
        h.emit({ type: 'blockPathCacheRemovalFailed', requestId: h.lastRequest().requestId, error: 'remove failed' });
        await expect(removal).rejects.toThrow('remove failed');
        expect(h.pendingCounts()).toEqual([0, 0, 0, 0, 0]);
    });

    it('preserves packed transfer lists and generation-based render cancellation', async () => {
        const h = createHarness();
        const data = createRenderData();
        const completion = h.api.workerSetMemoryBlockData({ data, fileHash: 'cache-source' });
        const generation = h.lastRequest().generation;
        expect(h.pack).toHaveBeenCalledWith(data);
        expect(data.blocks).toEqual([]);
        expect(h.worker.postMessage).toHaveBeenLastCalledWith({
            type: 'setMemoryBlockData', generation, data: h.packedData, fileHash: 'cache-source',
        }, h.transfers);
        expect(h.api.isCurrentBlockWorkerGeneration(generation)).toBe(true);
        h.api.workerDestroy();
        expect(h.api.isCurrentBlockWorkerGeneration(generation)).toBe(false);
        h.emit({ type: 'renderCancelled', generation });
        await expect(completion).resolves.toBeUndefined();
        expect(h.pendingCounts()[0]).toBe(0);
    });

    it('rejects every pending load/removal/query and resolves probes transient on HMR, clearing all maps', async () => {
        const h = createHarness();
        const pending = [
            h.api.workerSetMemoryBlockData({ data: createRenderData() }),
            h.api.workerLoadMemoryBlockCache({}),
            h.api.workerCheckOpfsAvailability(),
            h.api.workerRemoveMemoryBlockCaches({ fileHash: 'cache-source' }),
            h.api.workerQueryBlocks(query),
        ];
        expect(h.pendingCounts()).toEqual([1, 1, 1, 1, 1]);
        const settled = Promise.allSettled(pending);
        h.dispose();
        const results = await settled;
        results.forEach((result, index) => {
            if (index === 2) expect(result).toEqual({ status: 'fulfilled', value: 'transient' });
            else {
                expect(result.status).toBe('rejected');
                if (result.status === 'rejected') expect(result.reason.message).toBe('Block worker replaced by hot reload');
            }
        });
        expect(h.worker.terminate).toHaveBeenCalledTimes(1);
        expect(h.pendingCounts()).toEqual([0, 0, 0, 0, 0]);
        h.dispose();
        expect(h.pendingCounts()).toEqual([0, 0, 0, 0, 0]);
    });
});

describe('main-thread fallback compatibility', () => {
    it('delegates block queries and preserves OPFS availability/cache status', async () => {
        const h = createHarness(false);
        h.mainThread.queryBlocksHandler.mockReturnValue(queryResult);
        h.mainThread.loadMemoryBlockCacheHandler.mockResolvedValue('unavailable');
        h.checkOpfs.mockResolvedValue('transient');
        await expect(h.api.workerQueryBlocks(query)).resolves.toBe(queryResult);
        expect(h.mainThread.queryBlocksHandler).toHaveBeenCalledWith(query);
        await expect(h.api.workerCheckOpfsAvailability()).resolves.toBe('transient');
        await expect(h.api.workerLoadMemoryBlockCache({ fileHash: 'cache-source' })).resolves.toBe('unavailable');
        expect(h.worker.postMessage).not.toHaveBeenCalled();
    });

    it('skips a superseded cache load and continues the fallback queue after failure', async () => {
        const h = createHarness(false);
        h.mainThread.loadMemoryBlockCacheHandler.mockRejectedValueOnce(new Error('cache failed'));
        const superseded = h.api.workerLoadMemoryBlockCache({ fileHash: 'old' });
        const latest = h.api.workerLoadMemoryBlockCache({ fileHash: 'latest' });
        await expect(superseded).resolves.toBe('miss');
        await expect(latest).rejects.toThrow('cache failed');
        expect(h.mainThread.loadMemoryBlockCacheHandler).toHaveBeenCalledTimes(1);
        const [, shouldCancel] = h.mainThread.loadMemoryBlockCacheHandler.mock.calls[0];
        expect(shouldCancel()).toBe(false);
        h.mainThread.loadMemoryBlockCacheHandler.mockResolvedValue('hit');
        await expect(h.api.workerLoadMemoryBlockCache({ fileHash: 'next' })).resolves.toBe('hit');
        expect(shouldCancel()).toBe(true);
    });

    it('keeps cache removal and destroy behind pending packed data loads', async () => {
        const h = createHarness(false);
        let finishLoad: () => void = () => undefined;
        h.mainThread.setMemoryBlockDataHandler.mockImplementation(() => new Promise<void>(resolve => {
            finishLoad = resolve;
        }));
        const loading = h.api.workerSetMemoryBlockData({ data: createRenderData(), fileHash: 'cache-source' });
        await Promise.resolve();
        const removal = h.api.workerRemoveMemoryBlockCaches({ fileHash: 'cache-source' });
        expect(h.removeCaches).not.toHaveBeenCalled();
        expect(h.mainThread.setMemoryBlockDataHandler.mock.calls[0][0].data).toBe(h.packedData);
        finishLoad();
        await loading;
        await removal;
        expect(h.removeCaches).toHaveBeenCalledWith('cache-source');
        await h.api.workerDestroy();
        expect(h.mainThread.destroyHandler).toHaveBeenCalledTimes(1);
    });
});

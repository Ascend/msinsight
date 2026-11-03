/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createBlockRenderer } from '../common/leaksWorker/blockWorker/worker';
import { createStateRenderer } from '../rendering/stateWorker/worker';
import { createNativeWorker as createBlockWorker } from '../common/leaksWorker/blockWorker/createWorker';
import { createNativeWorker as createStateWorker } from '../rendering/stateWorker/createWorker';
import { MainThreadRender as BlockFallback } from '../common/leaksWorker/blockWorker/mainThreadindex';
import { MainThreadRender as StateFallback } from '../rendering/stateWorker/mainThreadindex';
import type { MemoryContext } from '../common/shared/state/MemoryContext';
import type { MemSnapshotContext } from '../state/MemSnapshotContext';

jest.mock('../common/leaksWorker/blockWorker/createWorker', () => ({ createNativeWorker: jest.fn() }));
jest.mock('../rendering/stateWorker/createWorker', () => ({ createNativeWorker: jest.fn() }));
jest.mock('../common/leaksWorker/tools/detection', () => ({ isWebGL2Supported: () => true }));
jest.mock('../common/leaksWorker/tools/opfsConfig', () => ({ checkLeaksOpfsAvailability: jest.fn().mockResolvedValue('available') }));
jest.mock('../common/leaksWorker/tools/BlockDataOPFS', () => ({ BlockDataOPFS: { removePersistentCachesBySourceHash: jest.fn() } }));
jest.mock('../common/leaksWorker/tools/packedBlockData', () => ({ packRenderData: (data: object) => data, getPackedRenderDataTransferList: () => [] }));
jest.mock('../common/leaksWorker/blockWorker/mainThreadindex', () => ({ MainThreadRender: jest.fn() }));
jest.mock('../rendering/stateWorker/mainThreadindex', () => ({ MainThreadRender: jest.fn() }));

class FakeWorker extends EventTarget {
    postMessage = jest.fn();
    terminate = jest.fn();
    emit(data: object): void { this.dispatchEvent(new MessageEvent('message', { data })); }
}
const owner = (): MemoryContext => ({ module: 'leaks' } as MemoryContext);
const snapshotOwner = (): MemSnapshotContext => ({ module: 'memsnapshot' } as MemSnapshotContext);
const canvas = (): HTMLCanvasElement => ({ transferControlToOffscreen: jest.fn(() => ({})) } as unknown as HTMLCanvasElement);

describe('owner-scoped renderer instances', () => {
    let workers: FakeWorker[];
    beforeEach(() => {
        jest.clearAllMocks();
        workers = [];
        const create = (): Worker => {
            const worker = new FakeWorker();
            workers.push(worker);
            return worker as unknown as Worker;
        };
        (createBlockWorker as jest.Mock).mockImplementation(create);
        (createStateWorker as jest.Mock).mockImplementation(create);
    });

    it('starts neither backend on construction or unused reset; scope starts only its block Worker', () => {
        const block = createBlockRenderer(owner());
        const state = createStateRenderer(snapshotOwner());
        block.destroy(); state.destroy();
        expect(workers).toHaveLength(0);
        expect(BlockFallback).not.toHaveBeenCalled();
        expect(StateFallback).not.toHaveBeenCalled();
        const unbind = block.subscribe(jest.fn());
        expect(createBlockWorker).toHaveBeenCalledTimes(1);
        expect(createStateWorker).not.toHaveBeenCalled();
        unbind(); block.dispose(); workers[0].emit({ type: 'disposed' }); state.dispose();
    });

    it('does not transfer a canvas twice during effect replay', () => {
        const state = createStateRenderer(snapshotOwner());
        const target = canvas();
        state.initCanvas({ canvas: target, width: 100, height: 100 });
        state.initCanvas({ canvas: target, width: 100, height: 100 });
        expect(target.transferControlToOffscreen).toHaveBeenCalledTimes(1);
        expect(createStateWorker).toHaveBeenCalledTimes(1);
        state.dispose();
        expect(workers[0].terminate).toHaveBeenCalledTimes(1);
    });

    it('keeps pending responses isolated even when owners have identical generation numbers', async () => {
        const first = createBlockRenderer(owner());
        const second = createBlockRenderer(owner());
        const firstDone = jest.fn(); const secondDone = jest.fn();
        const firstLoad = first.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData }).then(firstDone);
        const secondLoad = second.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData }).then(secondDone);
        workers[0].emit({ type: 'renderCompleted', generation: 1 });
        await firstLoad;
        expect(secondDone).not.toHaveBeenCalled();
        first.dispose(); workers[0].emit({ type: 'disposed' });
        expect(workers[1].terminate).not.toHaveBeenCalled();
        workers[1].emit({ type: 'renderCompleted', generation: 1 });
        await secondLoad;
        expect(secondDone).toHaveBeenCalledTimes(1);
        second.dispose(); workers[1].emit({ type: 'disposed' });
    });

    it('settles all pending task kinds and ignores messages after disposal', async () => {
        const renderer = createBlockRenderer(owner());
        const listener = jest.fn(); renderer.subscribe(listener);
        const load = renderer.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData });
        const cache = renderer.loadMemoryBlockCache({ fileHash: 'sample' });
        const probe = renderer.checkOpfsAvailability();
        const removal = renderer.removeMemoryBlockCaches({ fileHash: 'sample' }).catch(error => error.message);
        renderer.dispose(); renderer.dispose();
        await expect(load).resolves.toBeUndefined();
        await expect(cache).resolves.toBe('miss');
        await expect(probe).resolves.toBe('transient');
        await expect(removal).resolves.toBe('Renderer disposed');
        workers[0].emit({ type: 'hoverItemResult', generation: 2, result: { id: 4 } });
        expect(listener).not.toHaveBeenCalled();
        expect(renderer.isCurrentGeneration(2)).toBe(false);
        renderer.transform({ transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 } });
        await renderer.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData });
        expect(workers).toHaveLength(1);
        workers[0].emit({ type: 'disposed' });
        expect(workers[0].terminate).toHaveBeenCalledTimes(1);
    });

    it('terminates an unresponsive Worker after the bounded cleanup interval', () => {
        jest.useFakeTimers();
        const renderer = createBlockRenderer(owner());
        renderer.subscribe(jest.fn());
        renderer.dispose();
        jest.advanceTimersByTime(1000);
        expect(workers[0].terminate).toHaveBeenCalledTimes(1);
        jest.useRealTimers();
    });

    it('creates independent fallback renderers without any Workers and detaches owners immediately', async () => {
        const fallbackInstances: Array<{ session?: MemoryContext; initCanvasHandler: jest.Mock; destroyHandler: jest.Mock }> = [];
        (BlockFallback as jest.Mock).mockImplementation(() => {
            const fallback = { session: undefined, initCanvasHandler: jest.fn(), destroyHandler: jest.fn() };
            fallbackInstances.push(fallback);
            return fallback;
        });
        const firstOwner = owner(); const secondOwner = owner();
        const first = createBlockRenderer(firstOwner, false);
        const second = createBlockRenderer(secondOwner, false);
        await first.initCanvas({ canvas: canvas(), width: 100, height: 100 });
        await second.initCanvas({ canvas: canvas(), width: 100, height: 100 });
        expect(workers).toHaveLength(0);
        expect(fallbackInstances[0].session).toBe(firstOwner);
        expect(fallbackInstances[1].session).toBe(secondOwner);
        first.dispose();
        expect(fallbackInstances[0].session).toBeUndefined();
        expect(fallbackInstances[1].session).toBe(secondOwner);
        second.dispose();
        await Promise.resolve(); await Promise.resolve();
        expect(fallbackInstances[0].destroyHandler).toHaveBeenCalledTimes(1);
        expect(fallbackInstances[1].destroyHandler).toHaveBeenCalledTimes(1);
    });
    it('ignores messages and calls when the same session has switched feature before cleanup', () => {
        const session = owner();
        const renderer = createBlockRenderer(session);
        const listener = jest.fn(); renderer.subscribe(listener);
        session.module = 'memsnapshot';
        workers[0].emit({ type: 'hoverItemResult' });
        renderer.transform({ transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 } });
        expect(listener).not.toHaveBeenCalled();
        expect(workers[0].postMessage).not.toHaveBeenCalled();
        expect(renderer.isCurrentGeneration(undefined)).toBe(false);
        renderer.dispose(); workers[0].emit({ type: 'disposed' });
    });

    it('rejects pending data loads and settles probes when the Worker fails', async () => {
        const renderer = createBlockRenderer(owner());
        const load = renderer.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData }).catch(error => error.message);
        const cache = renderer.loadMemoryBlockCache({ fileHash: 'sample' }).catch(error => error.message);
        const probe = renderer.checkOpfsAvailability();
        workers[0].dispatchEvent(new Event('error'));
        await expect(load).resolves.toBe('Block renderer failed');
        await expect(cache).resolves.toBe('Block renderer failed');
        await expect(probe).resolves.toBe('transient');
        workers[0].emit({ type: 'disposed' });
    });

    it('cancels queued fallback data work after disposal without starting a renderer', async () => {
        const renderer = createBlockRenderer(owner(), false);
        const load = renderer.setMemoryBlockData({ data: { blocks: [] } as unknown as RenderData });
        renderer.dispose();
        await load;
        expect(BlockFallback).not.toHaveBeenCalled();
        expect(workers).toHaveLength(0);
    });
});

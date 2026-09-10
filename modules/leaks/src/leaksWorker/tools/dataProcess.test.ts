/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { BlockDataOPFS } from './BlockDataOPFS';
import { buildBlockViewPathAndWriteToOPFS, getZoom, searchBlockDataByPointFromOPFS } from './dataProcess';

const createStorage = (): BlockDataOPFS => ({
    clear: jest.fn().mockResolvedValue(undefined),
    addPackedBlock: jest.fn().mockResolvedValue(undefined),
    addPackedBlocks: jest.fn().mockResolvedValue(undefined),
    flush: jest.fn().mockResolvedValue(undefined),
    getBatchCount: jest.fn().mockReturnValue(0),
    getStoredPathBytes: jest.fn().mockReturnValue(0),
    getMaxBatchPathFloats: jest.fn().mockReturnValue(600000),
} as unknown as BlockDataOPFS);

describe('buildBlockViewPathAndWriteToOPFS', () => {
    it('closes blocks at the window boundary without replaying every boundary free', async () => {
        const blockCount = 1000;
        const data: RenderData = {
            minTimestamp: 0,
            maxTimestamp: 500000,
            minSize: 0,
            maxSize: blockCount,
            blocks: Array.from({ length: blockCount }, (_, index) => ({
                id: index,
                addr: `${index}`,
                _startTimestamp: index,
                _endTimestamp: 500000,
                size: 1,
                path: [],
            })),
        };

        const storage = createStorage();
        const result = await buildBlockViewPathAndWriteToOPFS(data, storage);

        expect(result.metrics?.eventCount).toBe(blockCount);
        expect(result.metrics?.pathPoints).toBe(blockCount * 2);
        expect(result.maxTimestamp).toBe(500000);
        expect(storage.addPackedBlocks).toHaveBeenCalledTimes(16);
        expect(storage.addPackedBlock).not.toHaveBeenCalled();
    });

    it('derives the lifecycle window when legacy MemScope metadata is empty', async () => {
        const data: RenderData = {
            minTimestamp: 0,
            maxTimestamp: 0,
            minSize: 0,
            maxSize: 0,
            blocks: [
                { id: 1, addr: '1', _startTimestamp: 10, _endTimestamp: 30, size: 10, path: [] },
                { id: 2, addr: '2', _startTimestamp: 20, _endTimestamp: 40, size: 20, path: [] },
            ],
        };

        const result = await buildBlockViewPathAndWriteToOPFS(data, createStorage());

        expect(result.minTimestamp).toBe(10);
        expect(result.maxTimestamp).toBe(40);
        expect(result.maxSize).toBe(30);
        expect(result.metrics?.eventCount).toBe(3);
    });

    it('gives a single-event slice a non-zero display span', async () => {
        const data: RenderData = {
            minTimestamp: 8091,
            maxTimestamp: 8091,
            minSize: 0,
            maxSize: 10,
            blocks: [
                { id: -320, addr: '1', _startTimestamp: 8091, _endTimestamp: 8091, size: 10, path: [] },
                { id: -319, addr: '2', _startTimestamp: 8091, _endTimestamp: 8091, size: 20, path: [] },
            ],
        };

        const result = await buildBlockViewPathAndWriteToOPFS(data, createStorage());
        const zoom = getZoom(result, { width: 100, height: 50 } as HTMLCanvasElement);

        expect(result.minTimestamp).toBe(8091);
        expect(result.maxTimestamp).toBe(8092);
        expect(Number.isFinite(zoom.x)).toBe(true);
        expect(zoom.x).toBe(100);
    });
});

describe('getZoom', () => {
    it('does not produce Infinity for equal timestamps', () => {
        const zoom = getZoom(
            { minTimestamp: 8091, maxTimestamp: 8091, maxSize: 10, minSize: 0, batchCount: 1, blocks: [] },
            { width: 200, height: 50 } as HTMLCanvasElement,
        );

        expect(Number.isFinite(zoom.x)).toBe(true);
        expect(zoom.x).toBe(200);
        expect(zoom.offset).toBe(8091);
    });
});

describe('searchBlockDataByPointFromOPFS', () => {
    it('expands a hit-tested path fragment to the full cached block path', async () => {
        const fullPath: Array<[number, number]> = [[0, 0], [10, 0], [20, 8], [40, 8]];
        const storage = {
            findBatchesByTimestamp: (): number[] => [0],
            findBatchesOverlappingRange: (): number[] => [0],
            readBatchForHitTest: async () => ({
                metas: [{
                    id: 7,
                    addr: '0x1',
                    _startTimestamp: 0,
                    _endTimestamp: 40,
                    pathStartTimestamp: 20,
                    pathEndTimestamp: 40,
                    size: 8,
                    pathOffset: 0,
                    pathLength: 2,
                }],
                pathData: new Float32Array([20, 8, 40, 8]),
            }),
            findBlockById: async (blockId: number) => blockId === 7
                ? {
                    id: 7,
                    addr: '0x1',
                    _startTimestamp: 0,
                    _endTimestamp: 40,
                    size: 8,
                    path: fullPath,
                }
                : null,
        } as unknown as BlockDataOPFS;

        const result = await searchBlockDataByPointFromOPFS(
            storage,
            { clientX: 30, clientY: 12 },
            { x: 0, y: 0, scaleX: 1, scaleY: 1 },
            { x: 1, y: 1, offset: 0 },
        );

        expect(result?.path).toEqual(fullPath);
    });
});

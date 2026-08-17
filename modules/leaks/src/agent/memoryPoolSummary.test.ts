/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { summarizeMemoryPool } from './memoryPoolSummary';

const segment = (address: string, size: number, blockSizes: number[]): Segment => ({
    address,
    stream: 1,
    size,
    blocks: blockSizes.map((blockSize, id) => ({ id, offset: 0, size: blockSize })),
    offsetX: 0,
    offsetY: 0,
    allocOrMapEventId: 1,
});

test('summarizes memory pool data without returning full block arrays', () => {
    const result = summarizeMemoryPool(42, [
        segment('small', 100, [20, 30]),
        segment('large', 500, [200]),
    ]);

    expect(result).toMatchObject({
        eventId: 42,
        segmentCount: 2,
        blockCount: 3,
        capacityBytes: 600,
        usedBytes: 250,
        freeBytes: 350,
        overcommittedBytes: 0,
        utilizationRatio: 250 / 600,
        emptySegmentCount: 0,
        lowUtilizationSegmentCount: 0,
    });
    expect(result.largestSegments[0]).toMatchObject({ address: 'large', capacityBytes: 500, usedBytes: 200 });
    expect(result.largestSegments.some(item => 'blocks' in item)).toBe(false);
});

test('limits largest segment details to five entries', () => {
    const result = summarizeMemoryPool(1, Array.from({ length: 8 }, (_, index) => segment(String(index), index + 1, [])));

    expect(result.largestSegments).toHaveLength(5);
    expect(result.largestSegments.map(item => item.capacityBytes)).toEqual([8, 7, 6, 5, 4]);
});

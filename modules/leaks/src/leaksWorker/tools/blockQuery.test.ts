/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { queryBlockSummaries } from './blockQuery';

const block = (id: number, size: number, start: number, end: number, addr: string = `0x${id}`): Block => ({
    id,
    addr,
    size,
    _startTimestamp: start,
    _endTimestamp: end,
    path: [],
});

test('returns bounded block summaries ranked without paths', () => {
    const result = queryBlockSummaries([
        block(1, 100, 0, 50),
        block(2, 300, 0, 10),
        block(3, 200, 0, 100),
    ], { sortBy: 'size', limit: 2 });

    expect(result.blocks.map(item => item.id)).toEqual([2, 3]);
    expect(result.blocks).toHaveLength(2);
    expect(result.blocks.some(item => 'path' in item)).toBe(false);
});

test('filters by address and thresholds and deduplicates retained fragments', () => {
    const result = queryBlockSummaries([
        block(7, 128, 20, 40, '0xABC'),
        block(7, 128, 10, 50, '0xABC'),
        block(8, 64, 0, 100, '0xDEF'),
    ], { sortBy: 'lifetime', limit: 5, address: 'abc', minSize: 100, minLifetime: 20 });

    expect(result.blocks).toEqual([{
        id: 7,
        address: '0xABC',
        startTimestamp: 10,
        endTimestamp: 50,
        lifetime: 40,
        sizeBytes: 128,
    }]);
});

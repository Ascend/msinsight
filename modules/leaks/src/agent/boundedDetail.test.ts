/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { toBoundedDetail } from './boundedDetail';

test('bounds arrays, strings, object fields, and nesting depth', () => {
    const result = toBoundedDetail({
        frames: ['frame-1', 'frame-2', 'frame-3'],
        long: '123456789',
        nested: { next: { value: 1 } },
        extra: true,
    }, { maxArrayItems: 2, maxDepth: 2, maxObjectKeys: 3, maxStringLength: 5 });

    expect(result).toEqual({
        frames: ['frame...[truncated]', 'frame...[truncated]', '[1 more items]'],
        long: '12345...[truncated]',
        nested: { next: '[max depth reached]' },
        __truncatedFields: 1,
    });
});

test('handles circular values without recursing indefinitely', () => {
    const value: { self?: unknown } = {};
    value.self = value;

    expect(toBoundedDetail(value, { maxArrayItems: 10 })).toEqual({ self: '[circular]' });
});

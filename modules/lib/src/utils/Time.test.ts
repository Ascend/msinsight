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
import { formatRelativeTime, sortByTimeDescending } from './Time';

const NOW = '2026-08-21T12:00:00.000Z';

test.each([
    ['2026-08-21T11:59:40.000Z', '<1m'],
    ['2026-08-21T11:57:00.000Z', '3m'],
    ['2026-08-21T09:00:00.000Z', '3h'],
    ['2026-08-18T12:00:00.000Z', '3d'],
])('formats %s as %s', (value, expected) => {
    expect(formatRelativeTime(value, NOW)).toBe(expected);
});

test('supports timestamps in seconds and milliseconds', () => {
    const now = Date.parse(NOW);
    expect(formatRelativeTime((now - 2 * 60 * 1000) / 1000, now)).toBe('2m');
    expect(formatRelativeTime(now - 2 * 60 * 60 * 1000, now)).toBe('2h');
});

test('preserves values that are not valid dates', () => {
    expect(formatRelativeTime('Creating...', NOW)).toBe('Creating...');
});

test('sorts valid dates newest first and preserves invalid date order', () => {
    const items = [
        { id: 'invalid-first', updatedAt: 'Earlier' },
        { id: 'old', updatedAt: '2026-08-18T12:00:00.000Z' },
        { id: 'new', updatedAt: '2026-08-21T11:00:00.000Z' },
        { id: 'invalid-second', updatedAt: undefined },
    ];

    expect(sortByTimeDescending(items, (item) => item.updatedAt).map((item) => item.id)).toEqual([
        'new',
        'old',
        'invalid-first',
        'invalid-second',
    ]);
    expect(items.map((item) => item.id)).toEqual(['invalid-first', 'old', 'new', 'invalid-second']);
});

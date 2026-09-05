/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { formatBytes, formatBytesWithFullPrecision, formatBytesWithTruncatedPrecision, isHostMemoryEventType } from './utils';

jest.mock('@insight/lib/utils', () => ({
    safeJSONParse: (value: string) => JSON.parse(value),
}), { virtual: true });

describe('byte formatting', () => {
    it('keeps the existing rounded format and provides truncated and full precision formats in the same unit', () => {
        const memoryBytes = 536872960;
        expect(formatBytes(memoryBytes)).toBe('512.002 MB');
        expect(formatBytesWithTruncatedPrecision(memoryBytes)).toBe('512.001... MB');
        expect(formatBytesWithFullPrecision(memoryBytes)).toBe('512.001953125 MB');
    });

    it('uses the same unit selection for negative values', () => {
        expect(formatBytes(-1025)).toBe('-1.001 KB');
        expect(formatBytesWithTruncatedPrecision(-1025)).toBe('-1.000... KB');
        expect(formatBytesWithFullPrecision(-1025)).toBe('-1.0009765625 KB');
    });

    it('omits the ellipsis when the value has no precision to hide', () => {
        expect(formatBytesWithTruncatedPrecision(1024)).toBe('1.000 KB');
    });
});

describe('isHostMemoryEventType', () => {
    it('treats HOST and legacy HOST_PINNED as host memory', () => {
        expect(isHostMemoryEventType('HOST')).toBe(true);
        expect(isHostMemoryEventType('HOST_PINNED')).toBe(true);
        expect(isHostMemoryEventType('PTA')).toBe(false);
    });
});

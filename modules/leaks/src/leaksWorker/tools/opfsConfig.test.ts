/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import {
    createBlockPathCacheStorageKey,
    normalizeLeaksFileHash,
} from './opfsConfig';

describe('OPFS block path cache configuration', () => {
    it('normalizes a SHA-256 hash before using it as a persistent key', () => {
        const hash = 'ABCDEF'.repeat(10) + 'ABCD';

        expect(normalizeLeaksFileHash(` ${hash} `)).toBe(hash.toLowerCase());
        expect(createBlockPathCacheStorageKey(hash.toLowerCase())).toBe(
            `main-cache-v1-${hash.toLowerCase()}`,
        );
    });

    it.each([undefined, null, '', 'not-a-sha256', 'a'.repeat(63), 'g'.repeat(64)])(
        'keeps the temporary-cache behavior for an invalid hash: %p',
        hash => {
            expect(normalizeLeaksFileHash(hash)).toBe('');
        },
    );

    it('accepts a cache hash containing device and event type', () => {
        const fileHash = 'a'.repeat(64);
        const device0Block = `${fileHash}-0-BLOCK`;
        const device1Block = `${fileHash}-1-BLOCK`;
        const device0Segment = `${fileHash}-0-SEGMENT`;

        expect(normalizeLeaksFileHash(device0Block)).toBe(device0Block.toLowerCase());
        expect(normalizeLeaksFileHash(device1Block)).not.toBe(normalizeLeaksFileHash(device0Block));
        expect(normalizeLeaksFileHash(device0Segment)).not.toBe(normalizeLeaksFileHash(device0Block));
    });
});

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

import { createMemoryBlockContextKey, isMemoryBlockLoadReady } from '../blockLoadState';

const createContext = (): any => ({
    fileHash: 'a'.repeat(64),
    module: 'memsnapshot',
    deviceId: '0',
    eventType: 'BLOCK',
    loadedMemoryBlockContextKey: '',
});

describe('memory block load state', () => {
    it('only reports ready for the matching snapshot context', () => {
        const context = createContext();
        context.loadedMemoryBlockContextKey = createMemoryBlockContextKey(context);
        expect(isMemoryBlockLoadReady(context)).toBe(true);

        context.deviceId = '1';
        expect(isMemoryBlockLoadReady(context)).toBe(false);
    });

    it('does not gate non-snapshot event requests', () => {
        const context = { ...createContext(), module: 'leaks' };
        expect(isMemoryBlockLoadReady(context)).toBe(true);
    });
});

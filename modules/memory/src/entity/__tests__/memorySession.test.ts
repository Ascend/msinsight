/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { MemorySession } from '../memorySession';

describe('MemorySession selectedRange history', () => {
    it('pushes and pops zoom ranges step by step', () => {
        const session = new MemorySession();

        session.pushSelectedRangeHistory();
        session.selectedRange = { startTs: 1000, endTs: 5000 };
        session.pushSelectedRangeHistory();
        session.selectedRange = { startTs: 2000, endTs: 4000 };

        expect(session.selectedRangeStack).toEqual([
            undefined,
            { startTs: 1000, endTs: 5000 },
        ]);

        expect(session.popSelectedRangeHistory()).toBe(true);
        expect(session.selectedRange).toEqual({ startTs: 1000, endTs: 5000 });

        expect(session.popSelectedRangeHistory()).toBe(true);
        expect(session.selectedRange).toBeUndefined();

        expect(session.popSelectedRangeHistory()).toBe(false);
        expect(session.selectedRange).toBeUndefined();
    });

    it('clears zoom history and current range', () => {
        const session = new MemorySession();
        session.selectedRange = { startTs: 1, endTs: 2 };
        session.selectedRangeStack = [undefined, { startTs: 1, endTs: 2 }];

        session.clearSelectedRangeHistory();

        expect(session.selectedRange).toBeUndefined();
        expect(session.selectedRangeStack).toEqual([]);
    });
});

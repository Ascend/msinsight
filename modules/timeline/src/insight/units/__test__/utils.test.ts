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

import type { InsightUnit } from '../../../entity/insight';
import type { Session } from '../../../entity/session';
import { setTimeOffsetForUnitTree } from '../utils';

const createUnit = (metadata: Record<string, unknown>, children?: InsightUnit[]): InsightUnit => ({
    metadata,
    children,
} as Partial<InsightUnit> as InsightUnit);

describe('timeline unit offset utils', () => {
    it('sets timestamp offset for deep metric descendants', () => {
        const counter = createUnit({ cardId: 'rank0', processId: '14083671101', threadName: 'HBM 0/Read' });
        const hbm = createUnit({ cardId: 'rank0', processId: '14083671101', processName: 'HBM', label: 'NPU' }, [counter]);
        const npuMetrics = createUnit({ cardId: 'rank0', processId: '__npu_metrics__', processName: 'NPU Metrics' }, [hbm]);
        const card = createUnit({ cardId: 'rank0' }, [npuMetrics]);
        const session = { units: [card] } as Partial<Session> as Session;
        const timestampOffsetConfig: Record<string, number> = {};

        setTimeOffsetForUnitTree(session, card, 123, timestampOffsetConfig);

        expect(timestampOffsetConfig.rank0).toBe(123);
        expect(timestampOffsetConfig.rank0____npu_metrics__).toBe(123);
        expect(timestampOffsetConfig.rank0__3).toBe(123);
        expect(card.alignStartTimestamp).toBe(123);
        expect(npuMetrics.alignStartTimestamp).toBe(123);
        expect(hbm.alignStartTimestamp).toBe(123);
        expect(counter.alignStartTimestamp).toBe(123);
    });

    it('does not mutate offset config when offset is undefined', () => {
        const card = createUnit({ cardId: 'rank0' });
        const session = { units: [card] } as Partial<Session> as Session;
        const timestampOffsetConfig: Record<string, number> = { rank0: 1 };

        setTimeOffsetForUnitTree(session, card, undefined, timestampOffsetConfig);

        expect(timestampOffsetConfig).toEqual({ rank0: 1 });
        expect(card.alignStartTimestamp).toBeUndefined();
    });
});

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

import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { doUnitsFilter, getUnitFilterName, normalizeUnitFilterName, useUnitsNameSet } from '../UnitsFilter';

const createUnit = (name: string, metadata: Record<string, unknown>, children?: InsightUnit[]): InsightUnit => ({
    name,
    metadata,
    children,
    isDisplay: true,
    isExpanded: false,
    collapsible: true,
    isMultiDeviceHidden: false,
} as Partial<InsightUnit> as InsightUnit);

describe('UnitsFilter hardware metrics filtering', () => {
    it('normalizes process id suffix consistently', () => {
        expect(normalizeUnitFilterName('LLC (14083671201)')).toBe('LLC');
        expect(normalizeUnitFilterName('NPU_MEM')).toBe('NPU_MEM');
        expect(normalizeUnitFilterName('AI Core Freq (3)')).toBe('AI Core Freq');
    });

    it('uses normalized label names when collecting options and filtering deep hardware metric children', () => {
        const counter = createUnit('Counter', { threadName: 'LLC 0 Read/Hit Rate' });
        const llc = createUnit('Label', { processName: 'LLC (14083671201)' }, [counter]);
        const hardwareMetrics = createUnit('Label', { processName: 'Hardware Metrics' }, [llc]);
        const card = createUnit('Card', { cardName: 'rank0' }, [hardwareMetrics]);
        const session = { units: [card] } as Partial<Session> as Session;

        const { unitNames } = useUnitsNameSet(session);
        expect(unitNames.has('LLC')).toBe(true);
        expect(getUnitFilterName(llc)).toBe('LLC');

        doUnitsFilter(session.units, ['LLC']);

        expect(card.isDisplay).toBe(true);
        expect(hardwareMetrics.isDisplay).toBe(true);
        expect(llc.isDisplay).toBe(true);
        expect(counter.isDisplay).toBe(true);
        expect(card.isExpanded).toBe(true);
        expect(hardwareMetrics.isExpanded).toBe(true);
        expect(llc.isExpanded).toBe(true);
    });

    it('hides the whole hardware metrics branch when no selected name matches', () => {
        const counter = createUnit('Counter', { threadName: 'HBM 0/Read' });
        const hbm = createUnit('Label', { processName: 'HBM (14083671101)' }, [counter]);
        const hardwareMetrics = createUnit('Label', { processName: 'Hardware Metrics' }, [hbm]);
        const card = createUnit('Card', { cardName: 'rank0' }, [hardwareMetrics]);

        doUnitsFilter([card], ['Missing metric']);

        expect(card.isDisplay).toBe(false);
        expect(hardwareMetrics.isDisplay).toBe(false);
        expect(hbm.isDisplay).toBe(false);
        expect(counter.isDisplay).toBe(false);
    });
});

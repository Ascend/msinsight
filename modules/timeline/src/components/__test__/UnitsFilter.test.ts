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

const createCollapsedThread = (threadName: string, children?: InsightUnit[]): InsightUnit => {
    const thread = createUnit('Thread', { threadName }, children);
    const chart = {
        type: 'stackStatus' as const,
        mapFunc: async (): Promise<[]> => [],
        height: 25,
        config: { isCollapse: true, maxDepth: 3, rowHeight: 20 },
    };
    thread.chart = chart;
    thread.collapseAction = jest.fn(() => {
        chart.config.isCollapse = !chart.config.isCollapse;
        chart.height = chart.config.isCollapse ? 25 : chart.config.maxDepth * chart.config.rowHeight;
    });
    return thread;
};

describe('UnitsFilter thread expansion', () => {
    it.each([undefined, []] as Array<InsightUnit[] | undefined>)('expands a matching leaf thread with children=%j', (children) => {
        const thread = createCollapsedThread('PyTorch', children);
        thread.onceExpand = false;
        const hiddenThread = createCollapsedThread('CANN');
        const process = createUnit('Process', { processName: 'process 1' }, [thread, hiddenThread]);
        const card = createUnit('Card', { cardName: 'rank0' }, [process]);

        doUnitsFilter([card], ['PyTorch']);
        doUnitsFilter([card], ['PyTorch']);

        expect(card.isExpanded).toBe(true);
        expect(process.isExpanded).toBe(true);
        expect(thread.isExpanded).toBe(true);
        expect(thread.chart).toMatchObject({ height: 60, config: { isCollapse: false } });
        expect(thread.collapseAction).toHaveBeenCalledTimes(1);
        expect(thread.onceExpand).toBeUndefined();
        expect(hiddenThread.isDisplay).toBe(false);
        expect(hiddenThread.isExpanded).toBe(false);
        expect(hiddenThread.collapseAction).not.toHaveBeenCalled();
    });

    it.each([
        ['CANN', 'acl'],
        ['Ascend Hardware', 'Stream 1'],
    ])('preserves child expansion states when filtering %s', (groupName, threadName) => {
        const thread = createCollapsedThread(threadName);
        thread.onceExpand = false;
        const expandedThread = createCollapsedThread('Expanded thread');
        expandedThread.collapseAction?.(expandedThread);
        expandedThread.isExpanded = true;
        const group = createUnit('Label', { processName: groupName }, [thread, expandedThread]);
        const card = createUnit('Card', { cardName: 'rank0' }, [group]);

        doUnitsFilter([card], [groupName]);

        expect(card.isExpanded).toBe(true);
        expect(group.isExpanded).toBe(true);
        expect(thread.isDisplay).toBe(true);
        expect(thread.isExpanded).toBe(false);
        expect(thread.chart).toMatchObject({ height: 25, config: { isCollapse: true } });
        expect(thread.collapseAction).not.toHaveBeenCalled();
        expect(thread.onceExpand).toBe(false);
        expect(expandedThread.isDisplay).toBe(true);
        expect(expandedThread.isExpanded).toBe(true);
        expect(expandedThread.chart).toMatchObject({ height: 60, config: { isCollapse: false } });
        expect(expandedThread.collapseAction).toHaveBeenCalledTimes(1);
    });
});

describe('UnitsFilter metrics filtering', () => {
    it('normalizes process id suffix consistently', () => {
        expect(normalizeUnitFilterName('LLC (14083671201)')).toBe('LLC');
        expect(normalizeUnitFilterName('NPU_MEM')).toBe('NPU_MEM');
        expect(normalizeUnitFilterName('AI Core Freq (3)')).toBe('AI Core Freq');
    });

    it('uses normalized label names when collecting options and filtering deep hardware metric children', () => {
        const counter = createUnit('Counter', { threadName: 'LLC 0 Read/Hit Rate' });
        const llc = createUnit('Label', { processName: 'LLC (14083671201)' }, [counter]);
        const npuMetrics = createUnit('Label', { processName: 'NPU Metrics' }, [llc]);
        const card = createUnit('Card', { cardName: 'rank0' }, [npuMetrics]);
        const session = { units: [card] } as Partial<Session> as Session;

        const { unitNames } = useUnitsNameSet(session);
        expect(unitNames.has('LLC')).toBe(true);
        expect(getUnitFilterName(llc)).toBe('LLC');

        doUnitsFilter(session.units, ['LLC']);

        expect(card.isDisplay).toBe(true);
        expect(npuMetrics.isDisplay).toBe(true);
        expect(llc.isDisplay).toBe(true);
        expect(counter.isDisplay).toBe(true);
        expect(card.isExpanded).toBe(true);
        expect(npuMetrics.isExpanded).toBe(true);
        expect(llc.isExpanded).toBe(true);
        expect(counter.isExpanded).toBe(false);
    });

    it('hides the whole metrics branch when no selected name matches', () => {
        const counter = createUnit('Counter', { threadName: 'HBM 0/Read' });
        const hbm = createUnit('Label', { processName: 'HBM (14083671101)' }, [counter]);
        const npuMetrics = createUnit('Label', { processName: 'NPU Metrics' }, [hbm]);
        const card = createUnit('Card', { cardName: 'rank0' }, [npuMetrics]);

        doUnitsFilter([card], ['Missing metric']);

        expect(card.isDisplay).toBe(false);
        expect(npuMetrics.isDisplay).toBe(false);
        expect(hbm.isDisplay).toBe(false);
        expect(counter.isDisplay).toBe(false);
    });
});

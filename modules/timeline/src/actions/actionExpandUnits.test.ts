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

import {
    actionExpandAllUnits,
    getAllUnitsToggleState,
    toggleAllUnits,
} from './actionExpandUnits';
import type { InsightUnit } from '../entity/insight';
import type { Session } from '../entity/session';

const createUnit = (name: string, children: InsightUnit[] = [], cardId = '0'): InsightUnit => ({
    name,
    children,
    isExpanded: false,
    collapsible: true,
    hasExpanded: false,
    metadata: { cardId, processId: name, threadId: name },
    ...(name === 'Thread' && {
        chart: { type: 'stackStatus', config: { isCollapse: false } },
        collapseAction: jest.fn(),
    }),
} as unknown as InsightUnit);

const createSession = (units: InsightUnit[], selectedUnits: InsightUnit[] = []): Session => ({
    units,
    selectedUnits,
    threadsToFetch: new Map(),
    renderTrigger: true,
} as unknown as Session);

describe('global lane expansion toggle', () => {
    it('toggles all card trees and their thread fetch queue', () => {
        const threads = [createUnit('Thread', [], '0'), createUnit('Thread', [], '1')];
        threads[1].chart = undefined;
        threads[1].collapseAction = undefined;
        const processes = threads.map((thread, cardId) => createUnit('Process', [thread], String(cardId)));
        const roots = processes.map((process, cardId) => createUnit('Card', [process], String(cardId)));
        roots[0].onceExpand = false;
        processes[0].onceExpand = false;
        threads[0].onceExpand = false;
        const session = createSession(roots);

        expect(getAllUnitsToggleState(session)).toBe('expand');
        toggleAllUnits(session);
        expect([...roots, ...processes, ...threads].every(unit => unit.isExpanded)).toBe(true);
        expect([roots[0].onceExpand, processes[0].onceExpand, threads[0].onceExpand])
            .toEqual([undefined, undefined, undefined]);
        expect([...session.threadsToFetch.values()]).toEqual([threads[0]]);
        expect(threads[0].collapseAction).toHaveBeenCalled();
        expect(getAllUnitsToggleState(session)).toBe('collapse');

        toggleAllUnits(session);
        expect([...roots, ...processes, ...threads].every(unit => !unit.isExpanded)).toBe(true);
        expect(session.threadsToFetch.size).toBe(0);
        expect(getAllUnitsToggleState(session)).toBe('expand');
    });

    it('does nothing when the session has no collapsible descendants', () => {
        const session = createSession([createUnit('Leaf')]);

        expect(getAllUnitsToggleState(session)).toBe('idle');
        toggleAllUnits(session);

        expect(session.renderTrigger).toBe(true);
    });

    it('keeps the context-menu action scoped to the selected lane tree', () => {
        const [selectedChild, otherChild] = [createUnit('Process'), createUnit('Process')];
        const [selectedRoot, otherRoot] = [createUnit('Card', [selectedChild]), createUnit('Card', [otherChild])];
        const session = createSession([selectedRoot, otherRoot], [selectedRoot]);

        actionExpandAllUnits.perform(session);

        expect(selectedChild.isExpanded).toBe(true);
        expect(otherChild.isExpanded).toBe(false);
    });
});

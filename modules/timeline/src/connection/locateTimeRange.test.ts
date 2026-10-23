/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import { locateTimeRangeHandler } from './locateTimeRange';
import { store } from '../store';
import type { Session } from '../entity/session';
import type { InsightUnit } from '../entity/insight';
import { CardUnit } from '../insight/units/AscendUnit';
import type { CardMetaData } from '../entity/data';
import { renderHook, act } from '@testing-library/react';
import { useJumpTarget, type OrderOptions } from '../components/ChartContainer/Units/hooks/useLocate';
import { preOrderFlatten } from '../entity/common';

jest.mock('../store', () => ({ store: { sessionStore: { activeSession: undefined } } }));
jest.mock('../insight/units/AscendUnit', () => ({
    CardUnit: class MockCardUnit {
        metadata: unknown;
        isUnitVisible = true;
        isMerged = false;
        isExpanded = false;
        collapsible = true;
        height(): number { return 28; }
        constructor(metadata: unknown) { this.metadata = metadata; }
    },
}));

const createSession = (): Session => ({
    units: [new CardUnit({ cardId: 'rank-1' } as CardMetaData), new CardUnit({ cardId: 'rank-2' } as CardMetaData)],
    unitsConfig: { offsetConfig: { timestampOffset: {
        'rank-1__device': 1000000, 'rank-2__device': -3000000, 'rank-2__host': 9000000,
    } } },
    timelineMaker: { oldMarkedRange: [0, 1000] },
    selectedRangeIsLock: true,
    selectedData: { id: 'old-slice' },
    domainRange: { domainStart: 0, domainEnd: 1000 },
} as unknown as Session);

describe('Memory time range navigation', () => {
    beforeEach(() => { store.sessionStore.activeSession = createSession(); });

    it('uses the current rank Device offset and requests vertical navigation to that rank', () => {
        const session = store.sessionStore.activeSession as Session;
        locateTimeRangeHandler({ cardId: 'rank-2', startMs: 143, endMs: 147, rankOffsetNs: 7000000 });
        expect(session.selectedRange).toEqual([153000000, 157000000]);
        expect(session.domainRange).toEqual({ domainStart: 153000000, domainEnd: 157000000 });
        expect(session.selectedRangeIsLock).toBe(false);
        expect(session.selectedData).toBeUndefined();
        expect(session.timelineMaker.oldMarkedRange).toBeUndefined();
        expect(session.locateUnit?.target(session.units[0])).toBe(false);
        expect(session.locateUnit?.target(session.units[1])).toBe(true);
        expect(session.locateUnit?.showDetail).toBe(false);
        expect(session.locateUnit?.tuneToSelectedSlice).toBe(false);
    });

    it('reads the latest offset, including changes made after viewing Memory', () => {
        const session = store.sessionStore.activeSession as Session;
        session.unitsConfig.offsetConfig.timestampOffset['rank-2__device'] = 5000000;
        locateTimeRangeHandler({ cardId: 'rank-2', startMs: 143, endMs: 147, rankOffsetNs: 7000000 });
        expect(session.selectedRange).toEqual([145000000, 149000000]);
        const deviceLane = { metadata: { cardId: 'rank-2', metaType: 'Ascend Hardware' } } as unknown as InsightUnit;
        const hostLane = { metadata: { cardId: 'rank-2', offsetSide: 'host' } } as unknown as InsightUnit;
        expect(session.locateUnit?.target(deviceLane)).toBe(true);
        expect(session.locateUnit?.target(hostLane)).toBe(false);
    });

    it('expands the target rank parents and scrolls to its lane through the existing locator', () => {
        jest.useFakeTimers();
        const session = store.sessionStore.activeSession as Session;
        locateTimeRangeHandler({ cardId: 'rank-2', startMs: 143, endMs: 147, rankOffsetNs: 7000000 });
        const [rank1, rank2] = session.units;
        const root = {
            metadata: { cardId: 'host' }, children: [rank1, rank2],
            isUnitVisible: true, isMerged: false, collapsible: true, isExpanded: false,
            height: () => 28, collapseAction: jest.fn(),
        } as unknown as InsightUnit;
        session.units = [root];
        const dom = document.createElement('div');
        dom.scrollTo = jest.fn();
        const orderOptions: OrderOptions = { preOrderFlatten, options: {} };
        renderHook(() => useJumpTarget(session, session.units, true, orderOptions, dom));
        act(() => { jest.runOnlyPendingTimers(); });
        expect(root.isExpanded).toBe(true);
        expect(rank2.isExpanded).toBe(true);
        expect(session.selectedUnits).toEqual([rank2]);
        expect(dom.scrollTo).toHaveBeenCalledWith(0, 48);
        expect(session.locateUnit).toBeUndefined();
        expect(session.domainRange).toEqual({ domainStart: 153000000, domainEnd: 157000000 });
    });

    it.each([
        { startMs: 147, endMs: 143 }, { startMs: 143, endMs: 143 },
        { startMs: NaN, endMs: 147 }, { startMs: 143, endMs: Infinity },
    ])('ignores invalid ranges %j', (range) => {
        const session = store.sessionStore.activeSession as Session;
        locateTimeRangeHandler({ cardId: 'rank-2', rankOffsetNs: 0, ...range });
        expect(session.domainRange).toEqual({ domainStart: 0, domainEnd: 1000 });
        expect(session.locateUnit).toBeUndefined();
    });
});

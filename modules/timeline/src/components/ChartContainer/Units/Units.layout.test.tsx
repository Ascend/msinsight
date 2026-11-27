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
import React from 'react';
import { act, fireEvent, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import { Session } from '../../../entity/session';
import { chart, unit, UnitHeight, type ChartDesc } from '../../../entity/insight';
import type { ThreadMetaData } from '../../../entity/data';
import { RefUnits } from './Units';
import { switchPinned } from '../unitPin';
import { PinnedUnits } from '../PinnedUnits';

jest.mock('../../charts', () => ({
    Chart: ({ metadata }: { metadata: ThreadMetaData }): JSX.Element => <span data-testid="chart">{metadata.threadId}</span>,
}));
jest.mock('./UnitInfo', () => ({ UnitInfo: (): JSX.Element => <span/> }));
jest.mock('../../../insight/units/AscendUnit', () => ({ CardUnit: jest.fn() }));
jest.mock('../../charts/ChartInteractor/draw', () => ({ checkIsSliceMode: () => false, PAGE_PADDING: 0 }));
jest.mock('../../charts/ChartInteractor/actions', () => ({ MouseButton: { LEFT: 0 } }));
jest.mock('./hooks', () => ({ useJumpTarget: jest.fn() }));
jest.mock('./hooks/useSelectUnit', () => ({
    useSelectUnit: () => jest.fn(), useSelectUnits: () => jest.fn(), useDeselectUnits: () => jest.fn(),
}));
jest.mock('../../../utils/useWatchDomResize', () => ({
    useWatchResize: (): [number, React.RefObject<HTMLDivElement>] => [100, { current: null }],
}));

const TestUnit = unit<ThreadMetaData>({
    name: 'Thread',
    pinType: 'move',
    chart: chart({ type: 'stackStatus', height: UnitHeight.COLL, config: { rowHeight: UnitHeight.STANDARD, isCollapse: false }, mapFunc: async () => [] }),
});

function setup(metaType = 'TEXT'): { session: Session; units: Array<InstanceType<typeof TestUnit>> } {
    const units = Array.from({ length: 30 }, (_, i) => new TestUnit({
        cardId: 'rank', dbPath: 'a.db', processId: 'p', threadId: String(i), threadName: `lane-${i}`, metaType,
    } as ThreadMetaData));
    const session = new Session({ units });
    session.phase = 'download';
    return { session, units };
}

describe('observed virtual lane layout', () => {
    const originalScrollTo = Object.getOwnPropertyDescriptor(HTMLElement.prototype, 'scrollTo');
    beforeAll(() => { Object.defineProperty(HTMLElement.prototype, 'scrollTo', { configurable: true, value: jest.fn() }); });
    afterAll(() => {
        if (originalScrollTo) { Object.defineProperty(HTMLElement.prototype, 'scrollTo', originalScrollTo); } else { delete (HTMLElement.prototype as any).scrollTo; }
    });

    it.each(['TEXT', 'PYTORCH_API'])('reuses %s off-screen heights during scroll, but responds to height and visibility changes', metaType => {
        const { session, units } = setup(metaType);
        const height = jest.spyOn(units[29], 'height');
        const ref = React.createRef<HTMLDivElement>();
        const { container } = render(<RefUnits session={session} height={200} hasPinButton={true} laneInfoWidth={100} ref={ref}/>);
        const reads = height.mock.calls.length;
        expect(reads).toBeGreaterThan(0);
        for (const top of [1, 2, 3]) { fireEvent.scroll(ref.current as HTMLDivElement, { target: { scrollTop: top } }); }
        expect(height).toHaveBeenCalledTimes(reads);
        expect(session.totalHeight).toBe(30 * 26);
        act(() => { runInAction(() => { (units[0].chart as ChartDesc<'stackStatus'>).height = UnitHeight.UPPER; }); });
        expect(session.totalHeight).toBe(30 * 26 + 15);
        act(() => { runInAction(() => { units[0].isDisplay = false; }); });
        expect(session.totalHeight).toBe(29 * 26);
        expect(Array.from(container.querySelectorAll('[data-testid="chart"]')).map(node => node.textContent)).not.toContain('0');
    });

    it.each(['TEXT', 'PYTORCH_API'])('invalidates the %s main layout when a non-observable pin symbol moves a lane out and back', metaType => {
        const { session, units } = setup(metaType);
        const ref = React.createRef<HTMLDivElement>();
        const { container } = render(<RefUnits session={session} height={200} hasPinButton={true} laneInfoWidth={100} ref={ref}/>);
        const labels = (): Array<string | null> => Array.from(container.querySelectorAll('[data-testid="chart"]')).map(node => node.textContent);
        expect(labels()).toContain('0');
        act(() => { runInAction(() => { switchPinned(units[0]); session.pinnedUnits = [units[0]]; }); });
        expect(labels()).not.toContain('0');
        expect(session.totalHeight).toBe(29 * 26);
        act(() => { runInAction(() => { switchPinned(units[0]); session.pinnedUnits = []; }); });
        expect(labels()).toContain('0');
        expect(session.totalHeight).toBe(30 * 26);
    });

    it.each(['TEXT', 'PYTORCH_API'])('keeps %s pinned offsets responsive to heights, hiding and reordering', metaType => {
        const { session, units } = setup(metaType);
        units.forEach(unit => { switchPinned(unit); });
        session.pinnedUnits = units;
        const height = jest.spyOn(units[29], 'height');
        const ref = React.createRef<HTMLDivElement>();
        const { container } = render(<PinnedUnits session={session} height={200} laneInfoWidth={100} ref={ref}/>);
        const wrapper = container.querySelector('.invisible-units-placeholder')?.parentElement;
        expect(wrapper?.style.height).toBe('780px');
        const reads = height.mock.calls.length;
        for (const top of [1, 2, 3]) { fireEvent.scroll(ref.current as HTMLDivElement, { target: { scrollTop: top } }); }
        expect(height).toHaveBeenCalledTimes(reads);
        act(() => { runInAction(() => { (units[0].chart as ChartDesc<'stackStatus'>).height = UnitHeight.UPPER; }); });
        expect(wrapper?.style.height).toBe('795px');
        act(() => { runInAction(() => { units[1].isDisplay = false; }); });
        expect(wrapper?.style.height).toBe('769px');
        act(() => { runInAction(() => { session.pinnedUnits.reverse(); }); });
        expect(container.querySelector('[data-testid="chart"]')?.textContent).toBe('29');
        act(() => { runInAction(() => { session.pinnedUnits = []; }); });
        expect(wrapper?.style.height).toBe('0px');
        expect(container.querySelector('[data-testid="chart"]')).toBeNull();
    });
});

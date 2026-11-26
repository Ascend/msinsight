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

import { act, renderHook, type RenderHookResult } from '@testing-library/react';
import type { MapFunc, StatusData } from '../../entity/chart';
import type { InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { useData } from './hooks';
import { SimpleCache } from '../../cache/simplecache';

const createSession = (): Session => ({
    phase: 'download',
    domainRange: { domainStart: 0, domainEnd: 100 },
    endTimeAll: 100,
    units: [],
    unitsConfig: {
        filterConfig: { pythonFunction: false },
        offsetConfig: { timestampOffset: {} },
    },
    autoAdjustUnitHeight: false,
    areFlagEventsHidden: false,
    alignRender: false,
    threadsToFetch: new Map(),
} as unknown as Session);

const createUnit = (): InsightUnit => ({
    name: 'Thread',
    metadata: { cardId: 'rank0', metaType: 'TEXT', threadId: '2' },
    phase: 'download',
} as unknown as InsightUnit);

const renderPendingThread = (sessionOverrides: Partial<Session> = {}, unitOverrides: Partial<InsightUnit> = {}):
RenderHookResult<StatusData[], unknown> & {
    session: Session;
    unit: InsightUnit;
    pending: Array<(data: StatusData[]) => void>;
    mapFunc: jest.Mock<Promise<StatusData[]>, []>;
    processor: jest.Mock<StatusData[], [StatusData[]]>;
} => {
    const session = Object.assign(createSession(), sessionOverrides);
    const unit = Object.assign(createUnit(), unitOverrides);
    const pending: Array<(data: StatusData[]) => void> = [];
    const mapFunc = jest.fn(() => new Promise<StatusData[]>(resolve => pending.push(resolve)));
    const processor = jest.fn((data: StatusData[]) => data);
    const hook = renderHook(() => useData<'status'>({ session, unit, metadata: unit.metadata, mapFunc, width: 100, processor }));
    return { ...hook, session, unit, pending, mapFunc, processor };
};

describe('useData', () => {
    it('preserves master mapper arguments, width-only acceptance and phase policy for native DB', async () => {
        const { session, unit, pending, mapFunc, result, rerender } = renderPendingThread({}, {
            metadata: { ...createUnit().metadata, metaType: 'PYTORCH_API' },
        });
        expect(mapFunc.mock.calls[0]).toHaveLength(4);
        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        await act(async () => { pending[1]([{ name: 'new' } as StatusData]); });
        unit.phase = 'loading';
        await act(async () => { pending[0]([{ name: 'master old' } as StatusData]); });
        expect(result.current[0].name).toBe('master old');
        expect(unit.phase).toBe('download');
    });
    it('keeps the existing mutable Thread response policy outside stable offline playback', async () => {
        const { session, pending, result } = renderPendingThread({ phase: 'recording' });
        const data: StatusData[] = [{ name: 'live data', type: 'operator', startTime: 10, duration: 20 }];
        session.domainRange = { domainStart: 20, domainEnd: 80 };
        await act(async () => { pending[0](data); });
        expect(result.current).toEqual(data);
    });

    it('does not refetch other charts merely because their parent recreates a mapper', async () => {
        const session = createSession();
        const unit = createUnit();
        unit.name = 'Label';
        const data: StatusData[] = [{ name: 'loaded once', type: 'operator', startTime: 10, duration: 20 }];
        const load = jest.fn().mockResolvedValue(data);
        const { result, rerender } = renderHook(({ width }) => useData<'status'>({
            session, unit, metadata: unit.metadata, width, mapFunc: async () => load(),
        }), { initialProps: { width: 100 } });
        await act(async () => { await Promise.resolve(); });
        rerender({ width: 100 });
        await act(async () => { await Promise.resolve(); });
        expect(load).toHaveBeenCalledTimes(1);
        expect(result.current).toEqual(data);
        rerender({ width: 200 });
        await act(async () => { await Promise.resolve(); });
        expect(load).toHaveBeenCalledTimes(2);
    });

    it('discards a Thread response after the domain changes even before the next effect runs', async () => {
        const { session, pending, processor, result } = renderPendingThread();
        session.domainRange = { domainStart: 20, domainEnd: 80 };
        await act(async () => { pending[0]([{ name: 'old domain' } as StatusData]); });
        expect(result.current).toEqual([]);
        expect(processor).not.toHaveBeenCalled();
    });

    it('does not overwrite a newer same-width pan/zoom response with an older one', async () => {
        const { session, unit, pending, processor, result, rerender } = renderPendingThread();
        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        const newer: StatusData[] = [{ startTime: 30, duration: 5, name: 'newer', type: 'operator' }];
        await act(async () => { pending[1](newer); });
        expect(result.current).toEqual(newer);
        unit.phase = 'loading';
        await act(async () => { pending[0]([{ startTime: 1, duration: 1 }] as StatusData[]); });
        expect(result.current).toEqual(newer);
        expect(processor).toHaveBeenCalledTimes(1);
        expect(unit.phase).toBe('loading');
    });

    it('does not process an unmounted chart response or mark its lane downloaded', async () => {
        const { unit, pending, processor, unmount } = renderPendingThread({}, { phase: 'loading' });
        unmount();
        await act(async () => { pending[0]([]); });
        expect(processor).not.toHaveBeenCalled();
        expect(unit.phase).toBe('loading');
    });

    it('rejects a pre-invalidation response even before the next effect has run', async () => {
        const { session, pending, processor, rerender } = renderPendingThread({ simpleCache: new SimpleCache() });
        session.simpleCache.clear();
        await act(async () => { pending[0]([]); });
        expect(processor).not.toHaveBeenCalled();
        rerender();
        const fresh = [{ startTime: 40, duration: 5 }] as StatusData[];
        await act(async () => { pending[1](fresh); });
        expect(processor).toHaveBeenCalledWith(fresh, 100, 0, 100);
    });

    it('loads a replacement source with the same viewport dimensions', async () => {
        const session = createSession();
        const first = createUnit();
        const second = createUnit();
        second.metadata = { ...second.metadata, cardId: 'other-rank' };
        const mapFunc = jest.fn().mockResolvedValue([]) as MapFunc<'status'>;
        const { rerender } = renderHook(({ unit }) => useData({ session, unit, metadata: unit.metadata, mapFunc, width: 100 }), {
            initialProps: { unit: first },
        });
        await act(async () => { await Promise.resolve(); });
        rerender({ unit: second });
        await act(async () => { await Promise.resolve(); });
        expect(mapFunc).toHaveBeenCalledTimes(2);
        expect((mapFunc as jest.Mock).mock.calls[1][2]).toBe(second);
    });

    it('redraws full-trace summary data without refetching it after a domain change', async () => {
        const session = createSession();
        const mapFunc = jest.fn().mockResolvedValue([]) as MapFunc<'status'>;
        const processor = jest.fn((data: StatusData[]): StatusData[] => data);
        const unit = createUnit();
        const { rerender } = renderHook(() => useData({
            session,
            mapFunc,
            unit,
            metadata: unit.metadata,
            width: 100,
            processor,
            refetchOnDomainChange: false,
        }));

        await act(async () => {
            await Promise.resolve();
        });
        expect(mapFunc).toHaveBeenCalledTimes(1);
        expect(processor).toHaveBeenCalledWith([], 100, 0, 100);

        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        await act(async () => {
            await Promise.resolve();
        });

        expect(mapFunc).toHaveBeenCalledTimes(1);
    });

    it('keeps domain refetching enabled by default for viewport-scoped charts', async () => {
        const session = createSession();
        const mapFunc = jest.fn().mockResolvedValue([]) as MapFunc<'status'>;
        const unit = createUnit();
        const { rerender } = renderHook(() => useData({
            session,
            mapFunc,
            unit,
            metadata: unit.metadata,
            width: 100,
        }));

        await act(async () => {
            await Promise.resolve();
        });
        expect(mapFunc).toHaveBeenCalledTimes(1);

        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        await act(async () => {
            await Promise.resolve();
        });

        expect(mapFunc).toHaveBeenCalledTimes(2);
    });
});

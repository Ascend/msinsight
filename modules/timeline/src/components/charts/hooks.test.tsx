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

import { act, renderHook } from '@testing-library/react';
import type { MapFunc, StackStatusData, StatusData } from '../../entity/chart';
import { unit, type InsightUnit } from '../../entity/insight';
import { Session } from '../../entity/session';
import type { ThreadMetaData } from '../../entity/data';
import { useData, useDataState } from './hooks';

describe('useData', () => {
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
        name: 'Label',
        metadata: { cardId: 'rank0', metaType: 'DPU' },
        phase: 'download',
    } as unknown as InsightUnit);

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

    it('keeps a full-trace request valid and its completed data ready across viewport changes', async () => {
        const session = createSession();
        const lane = createUnit();
        const data = [{ startTime: 0, duration: 100, name: 'summary', type: 'summary' }];
        let resolveData: (value: StatusData[]) => void = () => {};
        const pending = new Promise<StatusData[]>(resolve => { resolveData = resolve; });
        const mapFunc = jest.fn().mockReturnValue(pending);
        const processor = jest.fn((value: StatusData[]): StatusData[] => value);
        const { result, rerender, unmount } = renderHook(() => useDataState<'status'>({
            session, mapFunc, unit: lane, metadata: lane.metadata, width: 100, processor, refetchOnDomainChange: false,
        }));
        expect(result.current.ready).toBe(false);

        session.domainRange = { domainStart: 20, domainEnd: 80 };
        rerender();
        expect(mapFunc).toHaveBeenCalledTimes(1);
        expect(result.current.ready).toBe(false);

        await act(async () => resolveData(data));
        expect(processor).toHaveBeenCalledWith(data, 100, 0, 100);
        expect(result.current).toMatchObject({ data, ready: true });

        session.domainRange = { domainStart: 30, domainEnd: 60 };
        rerender();
        expect(mapFunc).toHaveBeenCalledTimes(1);
        expect(processor).toHaveBeenCalledTimes(1);
        expect(result.current).toMatchObject({ data, ready: true });
        unmount();
    });
});

describe('useDataState readiness', () => {
    const deferred = (): { promise: Promise<StackStatusData[][]>; resolve: (data: StackStatusData[][]) => void } => {
        let finish: (data: StackStatusData[][]) => void = () => {};
        const promise = new Promise<StackStatusData[][]>(resolve => { finish = resolve; });
        return { promise, resolve: finish };
    };

    test('does not expose stale chart data as ready or let a late viewport response replace the current data', async () => {
        const session = new Session({ isNsMode: true });
        const Thread = unit<ThreadMetaData>({ name: 'Thread' });
        const metadata = { cardId: '0', processId: 'p1' } as ThreadMetaData;
        const lane = new Thread(metadata);
        session.phase = 'download';
        session.domainRange = { domainStart: 0, domainEnd: 2000 };
        const old = deferred();
        const newer = deferred();
        const mapFunc = jest.fn().mockReturnValueOnce(old.promise).mockReturnValueOnce(newer.promise);
        const { result, rerender, unmount } = renderHook(() => useDataState<'stackStatus'>({ session, unit: lane, metadata, width: 800, mapFunc }));
        expect(result.current.ready).toBe(false);
        act(() => { session.domainRange = { domainStart: 1000, domainEnd: 3000 }; });
        rerender();
        expect(mapFunc).toHaveBeenCalledTimes(2);
        const newData = [[{ id: 'new', startTime: 1500 } as StackStatusData]];
        await act(async () => newer.resolve(newData));
        expect(result.current).toMatchObject({ data: newData, ready: true });
        await act(async () => old.resolve([[{ id: 'old' } as StackStatusData]]));
        expect(result.current).toMatchObject({ data: newData, ready: true });
        unmount();
        session.cancelZoomingHistory();
    });

    test('keeps existing chart data unavailable to selection while a new request is pending', async () => {
        const session = new Session({ isNsMode: true });
        const Thread = unit<ThreadMetaData>({ name: 'Thread' });
        const metadata = { cardId: '0', processId: 'p1' } as ThreadMetaData;
        const lane = new Thread(metadata);
        session.phase = 'download';
        const next = deferred();
        const mapFunc = jest.fn().mockResolvedValueOnce([]).mockReturnValueOnce(next.promise);
        const { result, rerender, unmount } = renderHook(({ width }) => useDataState<'stackStatus'>({ session, unit: lane, metadata, width, mapFunc }), { initialProps: { width: 800 } });
        await act(async () => { await Promise.resolve(); });
        expect(result.current.ready).toBe(true);
        rerender({ width: 900 });
        expect(result.current.ready).toBe(false);
        await act(async () => next.resolve([]));
        expect(result.current.ready).toBe(true);
        unmount();
        session.cancelZoomingHistory();
    });
});

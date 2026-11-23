/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { act, renderHook } from '@testing-library/react';
import type { Session } from '../../entity/session';
import type { InsightUnit } from '../../entity/insight';
import type { PmuCounterResponse } from '../../entity/pmuCounter';
import { PmuCounterCache } from '../../cache/pmuCounterCache';
import { usePmuCounterData } from './usePmuCounterData';
import { pmuMetadata, pmuResponse, deferred } from './__test__/pmuTestUtils';

function fixture(): { session: Session; unit: InsightUnit } {
    return {
        session: {
            domainRange: { domainStart: 0, domainEnd: 100 },
            pmuCounterCache: new PmuCounterCache(),
            pmuCounterGeneration: 0,
            units: [],
            unitsConfig: { offsetConfig: { timestampOffset: { Host__host: 0 } } },
        } as unknown as Session,
        unit: { isUnitVisible: true, isDisplay: true, phase: 'download' } as InsightUnit,
    };
}

describe('PMU lazy request lifecycle', () => {
    beforeEach(() => { window.request = jest.fn(); jest.useFakeTimers(); });
    afterEach(() => { jest.useRealTimers(); });

    const flushRequest = async (): Promise<void> => { await act(async () => { jest.advanceTimersByTime(80); }); };

    it('keeps the completed intervals while a new viewport is debounced and fetched', async () => {
        const body = pmuResponse();
        const next = deferred<PmuCounterResponse>();
        (window.request as jest.Mock).mockResolvedValueOnce(body).mockReturnValueOnce(next.promise);
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await flushRequest();
        session.domainRange = { domainStart: 5, domainEnd: 95 };
        hook.rerender();
        expect(hook.result.current.data).toBe(body);
        expect(hook.result.current.loading).toBe(true);
        expect(window.request).toHaveBeenCalledTimes(1);
        await flushRequest();
        expect(hook.result.current.data).toBe(body);
        const updated = pmuResponse({ segments: [{ startTime: 10, endTime: 20, value: 9 }] });
        await act(async () => { next.resolve(updated); });
        expect(hook.result.current.data).toBe(updated);
        expect(hook.result.current.loading).toBe(false);
    });

    it('restores a cached viewport immediately without an empty or loading render', async () => {
        const first = pmuResponse();
        const second = pmuResponse({ segments: [{ startTime: 100, endTime: 200, value: 9 }] });
        (window.request as jest.Mock).mockResolvedValueOnce(first).mockResolvedValueOnce(second);
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const renders: Array<{ data: PmuCounterResponse; loading: boolean }> = [];
        const hook = renderHook(() => {
            const result = usePmuCounterData(session, metadata, unit, 600);
            renders.push(result);
            return result;
        });
        await flushRequest();
        session.domainRange = { domainStart: 100, domainEnd: 200 };
        hook.rerender();
        await flushRequest();
        const beforeRestore = renders.length;
        session.domainRange = { domainStart: 0, domainEnd: 100 };
        hook.rerender();
        expect(hook.result.current.data).toBe(first);
        expect(hook.result.current.loading).toBe(false);
        await act(async () => { await Promise.resolve(); });
        expect(renders.slice(beforeRestore).every(result => result.data === first && !result.loading)).toBe(true);
        expect(window.request).toHaveBeenCalledTimes(2);
        hook.unmount();
        const remount = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        expect(remount.result.current.data).toBe(first);
        expect(remount.result.current.loading).toBe(false);
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(2);
    });

    it.each(['database', 'counter', 'connection', 'generation'] as const)('clears intervals immediately when the %s identity changes', async kind => {
        const next = deferred<PmuCounterResponse>();
        (window.request as jest.Mock).mockResolvedValueOnce(pmuResponse()).mockReturnValueOnce(next.promise);
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await flushRequest();
        if (kind === 'database') { metadata.dbPath = '/other/db'; }
        if (kind === 'counter') { metadata.pmu.counterId = '2'; }
        if (kind === 'connection') { metadata.dataSource = { ...metadata.dataSource, port: 9001 }; }
        if (kind === 'generation') { session.pmuCounterCache.clear(); session.pmuCounterGeneration++; }
        hook.rerender();
        expect(hook.result.current.data.segments).toEqual([]);
        expect(hook.result.current.loading).toBe(true);
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(2);
        const updated = pmuResponse({ counterId: metadata.pmu.counterId });
        await act(async () => { next.resolve(updated); });
        expect(hook.result.current.data).toBe(updated);
    });

    it('coalesces rapidly changing ranges into a single final-window request', async () => {
        (window.request as jest.Mock).mockResolvedValue(pmuResponse());
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        for (let start = 10; start <= 50; start += 10) {
            act(() => { jest.advanceTimersByTime(20); });
            session.domainRange = { domainStart: start, domainEnd: start + 100 };
            hook.rerender();
        }
        expect(window.request).not.toHaveBeenCalled();
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(window.request).toHaveBeenCalledWith(metadata.dataSource,
            expect.objectContaining({ params: expect.objectContaining({ startTime: 50, endTime: 150 }) }), expect.anything());
    });

    it('contains an unsafe time range within the lane and does not issue a request', async () => {
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        session.domainRange = { domainStart: 0, domainEnd: Number.NaN };
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await act(async () => { await Promise.resolve(); });
        expect(window.request).not.toHaveBeenCalled();
        expect(hook.result.current.data.status).toBe('invalid-data');
    });

    it('does not request invisible leaves and requests when visible', async () => {
        (window.request as jest.Mock).mockResolvedValue(pmuResponse());
        const { session, unit } = fixture();
        unit.isUnitVisible = false;
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await act(async () => { await Promise.resolve(); });
        expect(window.request).not.toHaveBeenCalled();
        unit.isUnitVisible = true;
        hook.rerender();
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(1);
        expect(hook.result.current.data.status).toBe('ok');
        unit.isDisplay = false;
        session.domainRange = { domainStart: 10, domainEnd: 110 };
        hook.rerender();
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(1);
    });

    it('keeps the new window when the old request resolves last', async () => {
        const old = deferred<PmuCounterResponse>();
        const next = deferred<PmuCounterResponse>();
        (window.request as jest.Mock).mockReturnValueOnce(old.promise).mockReturnValueOnce(next.promise);
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await flushRequest();
        session.domainRange = { domainStart: 100, domainEnd: 200 };
        hook.rerender();
        await flushRequest();
        const body = pmuResponse({ segments: [{ startTime: 100, endTime: 200, value: 42 }] });
        await act(async () => { next.resolve(body); });
        expect(hook.result.current.data).toBe(body);
        await act(async () => { old.resolve(pmuResponse()); });
        expect(hook.result.current.data).toBe(body);
    });

    it('refetches after a generation or Host offset change and preserves raw data coordinates', async () => {
        (window.request as jest.Mock).mockResolvedValue(pmuResponse());
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await flushRequest();
        session.unitsConfig.offsetConfig.timestampOffset.Host__host = 20;
        hook.rerender();
        await flushRequest();
        expect(window.request).toHaveBeenLastCalledWith(metadata.dataSource, expect.objectContaining({ params: expect.objectContaining({ startTime: 20, endTime: 120 }) }), expect.anything());
        expect(hook.result.current.offset).toBe(20);
        expect(hook.result.current.data.segments[0].startTime).toBe(10);
        session.pmuCounterCache.clear();
        session.pmuCounterGeneration++;
        hook.rerender();
        await flushRequest();
        expect(window.request).toHaveBeenCalledTimes(3);
    });

    it('does not issue requests for known empty counters and supports retry after failure', async () => {
        (window.request as jest.Mock).mockRejectedValueOnce(new Error('offline')).mockResolvedValue(pmuResponse());
        const { session, unit } = fixture();
        const metadata = pmuMetadata();
        metadata.pmu.hasSamples = false;
        const hook = renderHook(() => usePmuCounterData(session, metadata, unit, 600));
        await act(async () => { await Promise.resolve(); });
        expect(window.request).not.toHaveBeenCalled();
        metadata.pmu.hasSamples = true;
        hook.rerender();
        await flushRequest();
        expect(hook.result.current.error).toBe(true);
        await act(async () => { hook.result.current.retry(); });
        await flushRequest();
        expect(hook.result.current.error).toBe(false);
        expect(hook.result.current.data.status).toBe('ok');
    });
});

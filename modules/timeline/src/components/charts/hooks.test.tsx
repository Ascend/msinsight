/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { act, renderHook } from '@testing-library/react';
import { Session } from '../../entity/session';
import { unit } from '../../entity/insight';
import type { ThreadMetaData } from '../../entity/data';
import type { StackStatusData } from '../../entity/chart';
import { useDataState } from './hooks';

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
});

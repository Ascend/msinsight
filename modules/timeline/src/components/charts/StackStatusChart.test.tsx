/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { act, render } from '@testing-library/react';
import { Session } from '../../entity/session';
import { unit, UnitHeight } from '../../entity/insight';
import type { ThreadMetaData } from '../../entity/data';
import type { StackStatusData } from '../../entity/chart';
import { selectSlice } from '../../agent/selectSlice';
import { StackStatusChart } from './StackStatusChart';

jest.mock('d3', () => ({
    scaleLinear: () => {
        const scale = Object.assign((value: number) => value, { range: () => scale, domain: () => scale, clamp: () => scale });
        return scale;
    },
}));
jest.mock('./hooks', () => ({
    ...jest.requireActual('./hooks'),
    useBatchedRender: () => {},
    useHoverPos: () => undefined,
    useClick: () => {},
}));
jest.mock('./TooltipComp', () => ({ TooltipComponent: () => null }));

test('registers the mounted expanded chart and selects through its click callback, then removes collapsed/unmounted sources', async () => {
    jest.spyOn(HTMLElement.prototype, 'getBoundingClientRect').mockReturnValue({ left: 200, right: 1000, top: 100, bottom: 180, width: 800, height: 80 } as DOMRect);
    const session = new Session({ isNsMode: true });
    const Thread = unit<ThreadMetaData>({ name: 'Thread' });
    const metadata = { cardId: '0', processId: 'p1', threadId: 't1', threadName: 'stream', metaType: 'TEXT' } as ThreadMetaData;
    const lane = new Thread(metadata);
    const data: StackStatusData = { id: 'slice-1', startTime: 1000, originalStartTime: 1200, duration: 50, name: 'MatMul', type: 'MatMul', color: 'deepBlue', depth: 0, cname: '' };
    session.phase = 'download';
    session.domainRange = { domainStart: 0, domainEnd: 2000 };
    const viewport = { ...session.domainRange };
    const props = {
        session,
        unit: lane,
        metadata,
        margin: 0,
        width: 800,
        height: 80,
        rowHeight: UnitHeight.STANDARD,
        title: 'Thread',
        mapFunc: jest.fn(async () => [[data]]),
        onClick: jest.fn(),
        isCollapse: false,
    };
    const view = render(<StackStatusChart {...props}/>);
    await act(async () => { await Promise.resolve(); });
    const context = { requestId: 'test', signal: new AbortController().signal, deadline: Date.now() + 5000 };
    await act(async () => {
        await expect(selectSlice({ name: 'MatMul' }, context, session, () => true)).resolves.toMatchObject({ status: 'selected' });
    });
    expect(session.selectedData).toMatchObject({ id: 'slice-1', timestamp: 1200, threadId: 't1', showSelectedData: true });
    expect(session.selectedUnits).toEqual([lane]);
    expect(props.onClick).toHaveBeenCalledWith(data, session, metadata);
    expect(session.domainRange).toEqual(viewport);
    view.rerender(<StackStatusChart {...props} isCollapse={true}/>);
    await expect(selectSlice({ name: 'MatMul' }, context, session, () => true)).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
    view.rerender(<StackStatusChart {...props}/>);
    view.unmount();
    await expect(selectSlice({ name: 'MatMul' }, context, session, () => true)).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
    jest.restoreAllMocks();
});

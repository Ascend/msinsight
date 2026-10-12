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
import { act, cleanup, render } from '@testing-library/react';
import type { StackStatusData } from '../../entity/chart';
import type { ThreadMetaData } from '../../entity/data';
import { unit, UnitHeight } from '../../entity/insight';
import { Session } from '../../entity/session';
import { selectSlice } from '../../agent/selectSlice';
import { StackStatusChart } from './StackStatusChart';
import { useClick, useDataState, useRangeAndDomain } from './hooks';
import type { OffsetSide } from '../../insight/units/offset';

// Use the distributed CommonJS bundle so pnpm's nested ESM package path works in Jest.
jest.mock('d3', () => jest.requireActual(require.resolve('d3').replace(/src[\\/]index.js$/, 'dist/d3.min.js')));
jest.mock('./hooks', () => ({
    useBatchedRender: jest.fn(),
    useClick: jest.fn(),
    useDataState: jest.fn(),
    useHoverPos: jest.fn(),
    useRangeAndDomain: jest.fn(),
}));
jest.mock('./TooltipComp', () => ({ TooltipComponent: (): null => null }));

afterEach(() => {
    cleanup();
    jest.resetAllMocks();
    jest.restoreAllMocks();
});

const clickSlice = (metadata: Partial<ThreadMetaData>, returnedThreadId?: string, unitMetadata = metadata): Session => {
    const slice: StackStatusData = {
        id: 'slice',
        startTime: 10,
        originalStartTime: 110,
        duration: 20,
        depth: 0,
        name: 'python call',
        type: 'python call',
        color: 'deepBlue',
        cname: '',
        threadId: returnedThreadId,
        cardId: 'rank0',
        dbPath: 'trace.db',
    };
    const Stack = unit<ThreadMetaData>({ name: 'Python Stack' });
    const lane = new Stack(unitMetadata as ThreadMetaData);
    const selectedSession = new Session({ isNsMode: true });
    selectedSession.endTimeAll = 100;
    selectedSession.domainRange = { domainStart: 0, domainEnd: 100 };
    selectedSession.selectedRangeData = [slice];
    (useDataState as jest.Mock).mockReturnValue({ data: [[slice]], ready: true });
    render(<StackStatusChart
        session={selectedSession} unit={lane} metadata={metadata} margin={0} width={100} height={UnitHeight.STANDARD}
        rowHeight={UnitHeight.STANDARD} isCollapse={false} title="Python Stack" mapFunc={jest.fn()}
    />);
    const { handleMouseUp } = (useClick as jest.Mock).mock.calls[0][0];
    act(() => handleMouseUp({ offsetX: 20, offsetY: 5 } as MouseEvent));
    expect(selectedSession.selectedDataUnit).toBe(lane);
    expect(selectedSession.selectedRangeData).toBeUndefined();
    return selectedSession;
};

describe('StackStatusChart slice selection', () => {
    beforeEach(() => {
        (useRangeAndDomain as jest.Mock).mockReturnValue([[0, 100], [0, 100]]);
    });

    it.each<{ metaType: string; offsetSide?: OffsetSide; expectedSide: OffsetSide }>([
        { metaType: 'TEXT', offsetSide: 'device', expectedSide: 'device' },
        { metaType: 'TEXT', offsetSide: 'host', expectedSide: 'host' },
        { metaType: 'PYTORCH_API_PYTHON_STACK', offsetSide: 'host', expectedSide: 'host' },
        { metaType: 'CANN_API', expectedSide: 'host' },
        { metaType: 'Ascend Hardware', expectedSide: 'device' },
    ])('keeps the $expectedSide offset category for $metaType selection', ({ metaType, offsetSide, expectedSide }) => {
        const selectedSession = clickSlice({ threadId: '2', processId: '200', metaType, offsetSide });

        expect(selectedSession.selectedData?.offsetSide).toBe(expectedSide);
    });

    it('takes the offset category from the source unit when chart metadata differs', () => {
        const metadata: Partial<ThreadMetaData> = { threadId: '2', processId: '200', metaType: 'TEXT', offsetSide: 'host' };
        const selectedSession = clickSlice(metadata, '2', { ...metadata, offsetSide: 'device' });

        expect(selectedSession.selectedData?.offsetSide).toBe('device');
    });

    it.each([
        ['python_stack:text:100', '100'],
        ['python_stack:100', 'pytorch'],
    ])('keeps virtual lane %s when traces return underlying thread %s', (threadId, returnedThreadId) => {
        const selectedSession = clickSlice({
            threadId, processId: '200', metaType: 'PYTORCH_API_PYTHON_STACK',
        }, returnedThreadId);

        expect(selectedSession.selectedData).toEqual(expect.objectContaining({
            threadId,
            processId: '200',
            metaType: 'PYTORCH_API_PYTHON_STACK',
            id: 'slice',
            startTime: 10,
            duration: 20,
            depth: 0,
        }));
    });

    it('preserves the actual source thread when clicking a merged stream', () => {
        const selectedSession = clickSlice({
            threadId: '', threadIdList: ['2', '8'], processId: '200', metaType: 'Ascend Hardware',
        }, '8');

        expect(selectedSession.selectedData?.threadId).toBe('8');
    });

    it('keeps ordinary lane identity when traces omit the thread', () => {
        const selectedSession = clickSlice({ threadId: '2', processId: '200', metaType: 'Ascend Hardware' });

        expect(selectedSession.selectedData?.threadId).toBe('2');
    });
});

describe('StackStatusChart Agent slice registration', () => {
    beforeEach(() => {
        const actualHooks = jest.requireActual<typeof import('./hooks')>('./hooks');
        (useDataState as jest.Mock).mockImplementation(actualHooks.useDataState);
        (useRangeAndDomain as jest.Mock).mockImplementation(actualHooks.useRangeAndDomain);
    });

    test('registers the mounted expanded chart and selects through its click callback, then removes collapsed/unmounted sources', async () => {
        jest.spyOn(HTMLElement.prototype, 'getBoundingClientRect').mockReturnValue({ left: 200, right: 1000, top: 100, bottom: 180, width: 800, height: 80 } as DOMRect);
        const session = new Session({ isNsMode: true });
        const Thread = unit<ThreadMetaData>({ name: 'Thread' });
        const metadata = { cardId: '0', processId: 'p1', threadId: 't1', threadName: 'stream', metaType: 'TEXT' } as ThreadMetaData;
        const lane = new Thread(metadata);
        const data: StackStatusData = { id: 'slice-1', startTime: 1000, originalStartTime: 1200, duration: 50, name: 'MatMul', type: 'MatMul', color: 'deepBlue', depth: 0, cname: '' };
        session.phase = 'download';
        session.endTimeAll = 2000;
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
        expect(session.selectedDataUnit).toBe(lane);
        expect(session.selectedData?.offsetSide).toBe('host');
        expect(session.selectedUnits).toEqual([lane]);
        expect(props.onClick).toHaveBeenCalledWith(data, session, metadata);
        expect(session.domainRange).toEqual(viewport);
        view.rerender(<StackStatusChart {...props} isCollapse={true}/>);
        await expect(selectSlice({ name: 'MatMul' }, context, session, () => true)).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
        view.rerender(<StackStatusChart {...props}/>);
        view.unmount();
        await expect(selectSlice({ name: 'MatMul' }, context, session, () => true)).rejects.toMatchObject({ code: 'COMMAND_UNAVAILABLE' });
    });
});

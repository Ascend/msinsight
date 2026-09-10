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
import { act, render } from '@testing-library/react';
import type { StackStatusData } from '../../entity/chart';
import type { ThreadMetaData } from '../../entity/data';
import { UnitHeight, type InsightUnit } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { StackStatusChart } from './StackStatusChart';
import { useClick, useData } from './hooks';

// Use the distributed CommonJS bundle so pnpm's nested ESM package path works in Jest.
jest.mock('d3', () => jest.requireActual(require.resolve('d3').replace(/src[\\/]index.js$/, 'dist/d3.min.js')));
jest.mock('./hooks', () => ({
    useBatchedRender: jest.fn(),
    useClick: jest.fn(),
    useData: jest.fn(),
    useHoverPos: jest.fn(),
    useRangeAndDomain: (): number[][] => [[0, 100], [0, 100]],
}));
jest.mock('./TooltipComp', () => ({ TooltipComponent: (): null => null }));

const clickSlice = (metadata: Partial<ThreadMetaData>, returnedThreadId?: string): Session => {
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
    const unit = { metadata, isTraceLoading: false } as InsightUnit;
    const selectedSession = { endTimeAll: 100, selectedRangeData: [slice] } as unknown as Session;
    (useData as jest.Mock).mockReturnValue([[slice]]);
    render(<StackStatusChart
        session={selectedSession} unit={unit} metadata={metadata} margin={0} width={100} height={UnitHeight.STANDARD}
        rowHeight={UnitHeight.STANDARD} isCollapse={false} title="Python Stack" mapFunc={jest.fn()}
    />);
    const { handleMouseUp } = (useClick as jest.Mock).mock.calls[0][0];
    act(() => handleMouseUp({ offsetX: 20, offsetY: 5 } as MouseEvent));
    expect(selectedSession.selectedDataUnit).toBe(unit);
    expect(selectedSession.selectedRangeData).toBeUndefined();
    return selectedSession;
};

describe('StackStatusChart slice selection', () => {
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

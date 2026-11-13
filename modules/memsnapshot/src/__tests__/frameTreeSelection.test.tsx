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
import MemoryStack from '../components/MemoryStack';
import { createSession } from '../application/session';
import { getSnapshotEvent } from '../api/requests';

jest.mock('../api/requests', () => ({ getSnapshotEvent: jest.fn() }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('@insight/lib/components', () => ({
    CollapsiblePanel: ({ children }: { children: React.ReactNode }) => children,
    Select: () => null,
}));
jest.mock('../common/components/Common', () => ({ Label: () => null }));
jest.mock('../common/components/MemoryDataZoom', () => () => null);
jest.mock('../components/MemoryStateDiagram', () => ({ MemoryStateDiagram: () => null }));
jest.mock('../components/PotentialLeakStats', () => () => null);
jest.mock('../components/MemSnapshotSliceOverview', () => () => null);
jest.mock('../loaders/data', () => ({
    getBarNewData: jest.fn(),
    getBlockTableData: jest.fn(),
    getEventTableData: jest.fn(),
    getPotentialLeakStats: jest.fn(),
    preloadSnapshotSliceOverviews: jest.fn(),
}));
jest.mock('../common/leaksWorker/blockWorker/worker', () => ({
    getBlockRenderer: () => ({ removeMemoryBlockCaches: jest.fn(), setBlockGraphGlobalMaxSize: jest.fn() }),
}));
jest.mock('../rendering/stateWorker/worker', () => ({ getStateRenderer: () => ({ selectItem: jest.fn() }) }));
jest.mock('../common/shared/hooks/useLifecycleRange', () => ({
    useLifecycleRange: () => ({
        selectedRange: undefined,
        setSelectedRange: jest.fn(),
        selectedRangeRef: { current: undefined },
        zoomRangeRef: { current: { min: 0, max: 100 } },
        debouncedRangeRequestRef: { current: null },
        debouncedCommitRangeRef: { current: null },
        rangeRequestSeqRef: { current: 0 },
        commitRangeSideEffects: jest.fn(),
        requestRangeData: jest.fn(),
        selectedZoomChange: jest.fn(),
        syncDataZoomRange: jest.fn(),
    }),
}));
jest.mock('../common/components/leaks/MemoryBlockDiagram', () => ({
    MemoryBlockDiagram: ({ onSelectTimestamp }: { onSelectTimestamp: (value: number) => void }) =>
        <button onClick={(): void => onSelectTimestamp(135.8)}>Select event</button>,
}));
jest.mock('../components/FrameTreePanel', () => ({
    FrameTreePanel: (props: unknown) => <div data-testid="selection-output">{JSON.stringify(props)}</div>,
}));

describe('timeline selection context isolation', () => {
    afterEach(() => jest.useRealTimers());
    it.each([
        ['device', 'success'], ['device', 'failure'], ['slice', 'success'],
        ['slice', 'failure'], ['dataset', 'success'], ['dataset', 'failure'],
    ])('discards old %s context %s after selecting a new event', async (change, outcome) => {
        jest.useFakeTimers();
        let resolveOld: (value: unknown) => void = () => undefined;
        let rejectOld: (error: Error) => void = () => undefined;
        const old = new Promise((resolve, reject) => { resolveOld = resolve; rejectOld = reject; });
        (getSnapshotEvent as jest.Mock).mockReturnValueOnce(old)
            .mockResolvedValue({ total: 1, events: [{ id: 132 }] });
        const session = createSession();
        runInAction(() => {
            session.deviceIds = { 0: ['BLOCK'], 1: ['BLOCK'] };
            session.deviceId = '0';
            session.eventType = 'BLOCK';
            session.fileHash = 'dataset-a';
            session.selectedSliceIndex = 0;
        });
        const view = render(<MemoryStack session={session} />);
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        expect(getSnapshotEvent).toHaveBeenCalledTimes(1);
        act(() => runInAction(() => {
            if (change === 'device') session.deviceId = '1';
            else if (change === 'slice') session.selectedSliceIndex = 1;
            else session.fileHash = 'dataset-b';
        }));
        expect(JSON.parse(view.getByTestId('selection-output').textContent ?? '{}').context).toBeNull();
        fireEvent.click(view.getByRole('button', { name: 'Select event' }));
        await act(async () => { jest.advanceTimersByTime(150); });
        const current = view.getByTestId('selection-output').textContent;
        expect(JSON.parse(current ?? '{}').context.eventId).toBe(132);
        await act(async () => {
            if (outcome === 'success') resolveOld({ total: 1, events: [{ id: 130 }] });
            else rejectOld(new Error('stale error'));
        });
        expect(view.getByTestId('selection-output').textContent).toBe(current);
        view.unmount();
    });
});

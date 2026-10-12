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
import { act, render, waitFor } from '@testing-library/react';
import { MemoryStateDiagram } from './MemoryStateDiagram';
import { getMemoryStateData, getSnapshotEvent } from '@/utils/RequestUtils';
import { workerSetMemoryStateData } from '@/leaksWorker/stateWorker/worker';
import { clearMemoryPoolSummary, updateMemoryPoolSummary } from '@/agent/graphController';

jest.mock('@/agent/graphController', () => ({
    clearMemoryPoolSummary: jest.fn(),
    updateMemoryPoolSummary: jest.fn(),
}));

jest.mock('@/utils/RequestUtils', () => ({
    getSnapshotEvent: jest.fn(),
    getMemoryStateData: jest.fn().mockResolvedValue({ segments: [] }),
}), { virtual: true });
jest.mock('@/utils/utils', () => ({ formatBytes: String }), { virtual: true });
jest.mock('@/leaksWorker/stateWorker/worker', () => ({
    workerInitCanvas: jest.fn(),
    workerResizeCanvas: jest.fn(),
    workerTransform: jest.fn(),
    workerHoverItem: jest.fn(),
    workerClickItem: jest.fn(),
    workerSetMemoryStateData: jest.fn(),
    workerSelectItem: jest.fn(),
}), { virtual: true });
jest.mock('@/leaksWorker/blockWorker/worker', () => ({ workerSelectItem: jest.fn() }), { virtual: true });
jest.mock('@insight/lib', () => ({
    Progress: () => null,
    ResizeTable: jest.requireActual('react').forwardRef(() => null),
}), { virtual: true });
jest.mock('@insight/lib/components', () => ({ Tooltip: () => null }), { virtual: true });
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('antd', () => ({ message: { warning: jest.fn() } }));
jest.mock('@ant-design/icons', () => ({
    BorderOutlined: () => null,
    LineChartOutlined: () => null,
    FlagOutlined: () => null,
    ArrowDownOutlined: () => null,
    ArrowUpOutlined: () => null,
    CloseCircleFilled: () => null,
    ColumnWidthOutlined: () => null,
    OneToOneOutlined: () => null,
    SearchOutlined: () => null,
}));
jest.mock('./tools', () => ({
    graphToolbarTooltipClassName: '',
    GraphKeycap: () => null,
    GraphMouseIcon: () => null,
    GraphShortcutActions: () => null,
    GraphShortcutRow: () => null,
    GraphShortcutTip: () => null,
    GraphShortcutTitle: () => null,
    GraphToolbar: () => null,
    GraphToolbarTooltipStyle: () => null,
    GraphWheelCombo: () => null,
    GraphWheelIcon: () => null,
    Loading: () => null,
    StateHoverItem: () => null,
}));

describe('snapshot event pagination during project switching', () => {
    it.each(['reset', 'unmount'])('stops requesting pages after %s', async transition => {
        let resolvePage: (value: any) => void = () => undefined;
        const request = getSnapshotEvent as jest.Mock;
        request.mockReset();
        request.mockReturnValueOnce(new Promise(resolve => { resolvePage = resolve; }));
        request.mockResolvedValue({ events: [], total: 0 });
        const session = createSession();
        const view = render(<MemoryStateDiagram session={session} />);
        expect(request).toHaveBeenCalledTimes(1);
        if (transition === 'unmount') {
            view.unmount();
        } else {
            // Reset is synchronous; an old response may arrive before React effects rerun.
            session.deviceId = '';
            session.module = 'leaks';
        }
        await act(async () => {
            resolvePage({ events: [{ id: 1, action: 'alloc' }], total: 2000 });
        });
        expect(request).toHaveBeenCalledTimes(1);
    });
});

const createSession = (sliceIndex = 0): any => ({
    module: 'memsnapshot',
    deviceId: '0',
    deviceIds: { 0: ['BLOCK'] },
    fileHash: 'snapshot-a',
    dbPath: 'snapshot-a.db',
    eventType: 'BLOCK',
    selectedSliceIndex: sliceIndex,
    loadedMemoryBlockContextKey: JSON.stringify(['snapshot-a', 'memsnapshot', '0', 'BLOCK', sliceIndex]),
    snapshotSlices: { 0: { readySlices: [0, 1], slices: [
        { index: 0, startEventId: 0, endEventId: 99, ready: true },
        { index: 1, startEventId: 100, endEventId: 199, ready: true },
        { index: 2, startEventId: 200, endEventId: 299, ready: false },
    ] } },
    pendingEventLocate: null,
    selectionVersion: 0,
    leaksWorkerInfo: { clickItem: null },
    stateWorkerInfo: {
        eventId: -1,
        renderOptions: {
            viewport: { width: 0, height: 0 },
            transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
            zoom: { x: 1, y: 1, offset: 0 },
        },
    },
});

const eventRow = (id = 1): any => ({ id, action: 'alloc', address: '0x1', stream: 0, size: 16 });
const deferredState = (): { promise: Promise<any>; resolve: (data: any) => void; reject: (error: Error) => void } => {
    let resolve!: (data: any) => void;
    let reject!: (error: Error) => void;
    const promise = new Promise((accept, fail) => { resolve = accept; reject = fail; });
    return { promise, resolve, reject };
};

describe('state detail and pool summary integration', () => {
    beforeEach(() => {
        (getSnapshotEvent as jest.Mock).mockReset().mockResolvedValue({ events: [eventRow()], total: 1 });
        (getMemoryStateData as jest.Mock).mockReset().mockResolvedValue({ segments: [] });
        (workerSetMemoryStateData as jest.Mock).mockClear();
        (updateMemoryPoolSummary as jest.Mock).mockClear();
        (clearMemoryPoolSummary as jest.Mock).mockClear();
    });

    test.each([0, 1])('requests the selected slice %s and publishes only its current summary', async sliceIndex => {
        const session = createSession(sliceIndex);
        const id = sliceIndex === 0 ? 1 : 101;
        (getSnapshotEvent as jest.Mock).mockResolvedValue({ events: [eventRow(id)], total: 1 });
        render(<MemoryStateDiagram session={session} />);
        await waitFor(() => expect(getMemoryStateData).toHaveBeenCalledWith({
            eventId: id, deviceId: '0', sliceIndex,
        }));
        await waitFor(() => expect(updateMemoryPoolSummary).toHaveBeenCalledWith(id, []));
        expect(session.stateWorkerInfo.eventId).toBe(id);
    });

    test.each(['project', 'path', 'identity', 'device', 'slice', 'graph', 'unmount'])('%s changes suppress a late state response', async change => {
        const session = createSession();
        const pending = deferredState();
        (getMemoryStateData as jest.Mock).mockReturnValue(pending.promise);
        const view = render(<MemoryStateDiagram session={session} />);
        await waitFor(() => expect(getMemoryStateData).toHaveBeenCalledTimes(1));
        (workerSetMemoryStateData as jest.Mock).mockClear();
        (updateMemoryPoolSummary as jest.Mock).mockClear();
        (clearMemoryPoolSummary as jest.Mock).mockClear();
        session.stateWorkerInfo.eventId = 500;
        if (change === 'project') session.fileHash = 'snapshot-b';
        if (change === 'path') session.dbPath = 'snapshot-b.db';
        if (change === 'identity') session.deviceIds = { 0: ['BLOCK'] };
        if (change === 'device') session.deviceId = '';
        if (change === 'slice') session.selectedSliceIndex = 1;
        if (change === 'graph') session.loadedMemoryBlockContextKey = '';
        if (change === 'unmount') view.unmount();
        await act(async () => { pending.resolve({ segments: [] }); });
        expect(workerSetMemoryStateData).not.toHaveBeenCalled();
        expect(updateMemoryPoolSummary).not.toHaveBeenCalled();
        expect(session.stateWorkerInfo.eventId).toBe(500);
    });

    test('a late state rejection cannot clear another project summary or selection', async () => {
        const session = createSession();
        const pending = deferredState();
        (getMemoryStateData as jest.Mock).mockReturnValue(pending.promise);
        render(<MemoryStateDiagram session={session} />);
        await waitFor(() => expect(getMemoryStateData).toHaveBeenCalledTimes(1));
        session.fileHash = 'snapshot-b';
        session.stateWorkerInfo.eventId = 500;
        (clearMemoryPoolSummary as jest.Mock).mockClear();
        await act(async () => { pending.reject(new Error('old state failed')); });
        expect(clearMemoryPoolSummary).not.toHaveBeenCalled();
        expect(session.stateWorkerInfo.eventId).toBe(500);
    });

    test('an unready slice does not issue event or state requests', () => {
        const session = createSession(2);
        render(<MemoryStateDiagram session={session} />);
        expect(getSnapshotEvent).not.toHaveBeenCalled();
        expect(getMemoryStateData).not.toHaveBeenCalled();
        expect(updateMemoryPoolSummary).not.toHaveBeenCalled();
    });
});

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
import * as requests from '../RequestUtils';
import * as memscopeRequests from '../../features/memscope/api/requests';
import * as memsnapshotRequests from '../../features/memsnapshot/api/requests';

const blockParams = {
    deviceId: '0',
    eventType: 'BLOCK',
    isTable: false,
    relativeTime: true,
    startTimestamp: 0,
    endTimestamp: 100,
    currentPage: 2,
    pageSize: 50,
    sliceIndex: 0,
    filters: { owner: 'tensor' },
    rangeFilters: { size: [0, 1024] },
    onlyUnreleasedInRange: true,
};
const allocationParams = {
    deviceId: '0',
    eventType: 'BLOCK',
    startTimestamp: 0,
    endTimestamp: 100,
    currentPage: 2,
    pageSize: 50,
    sliceIndex: 0,
};
const eventParams = {
    deviceId: '0',
    startEventIdx: 0,
    endEventIdx: 100,
    sliceIndex: 0,
    currentPage: 2,
    pageSize: 50,
    orderBy: 'id',
    desc: false,
    filters: { action: 'alloc' },
};
const traceParams = {
    deviceId: '0', threadId: 0, relativeTime: true, startTimestamp: 0, endTimestamp: 100, allowTrim: false,
};
const detailParams = {
    id: -1, type: 'segment', deviceId: '0', eventId: 0, segmentAddress: '0x1000', stream: 0, sliceIndex: 0,
};
const stateParams = { deviceId: '0', eventId: 0, sliceIndex: 0 };
const statsParams = { deviceId: '0', startTimestamp: 0, endTimestamp: 100, sliceIndex: 0 };

type RequestCase = [name: string, command: string, params: Record<string, unknown>, invoke: () => Promise<unknown>];
const cases: RequestCase[] = [
    ['getBlocksGraphData', 'Memory/leaks/blocks', blockParams, () => requests.getBlocksGraphData(blockParams)],
    ['getLeaksAllocationsData', 'Memory/leaks/allocations', allocationParams, () => requests.getLeaksAllocationsData(allocationParams)],
    ['getMemoryDetailData', 'Memory/leaks/details',
        { deviceId: '0', timestamp: 0, eventType: 'BLOCK', relativeTime: true }, () => requests.getMemoryDetailData('0', 0, 'BLOCK')],
    ['getFuncData', 'Memory/leaks/traces', traceParams, () => requests.getFuncData(traceParams)],
    ['getBlockDetails', 'Memory/leaks/blocks', { ...blockParams, isTable: true }, () => requests.getBlockDetails({ ...blockParams, isTable: true })],
    ['getEventDetails', 'Memory/leaks/events', eventParams, () => requests.getEventDetails(eventParams)],
    ['getSnapshotBlocks', 'Memory/snapshot/blocks', blockParams, () => requests.getSnapshotBlocks(blockParams)],
    ['getSnapshotAllocations', 'Memory/snapshot/allocations', allocationParams, () => requests.getSnapshotAllocations(allocationParams)],
    ['getSnapshotAllocationLines', 'Memory/snapshot/allocationLines', allocationParams, () => requests.getSnapshotAllocationLines(allocationParams)],
    ['getSnapshotBlockTable', 'Memory/snapshot/blocks', { ...blockParams, isTable: true }, () => requests.getSnapshotBlockTable({ ...blockParams, isTable: true })],
    ['getSnapshotLeakStats', 'Memory/snapshot/leakStats', statsParams, () => requests.getSnapshotLeakStats(statsParams)],
    ['getSnapshotEvent', 'Memory/snapshot/events', eventParams, () => requests.getSnapshotEvent(eventParams)],
    ['getMemoryStateData', 'Memory/snapshot/state', stateParams, () => requests.getMemoryStateData(stateParams)],
    ['getSnapshotDetail', 'Memory/snapshot/detail', detailParams, () => requests.getSnapshotDetail(detailParams)],
];

describe('memory request transport contract', () => {
    const originalRequest = window.request;
    const transport = jest.fn();

    beforeEach(() => {
        transport.mockReset();
        window.request = transport;
    });

    afterEach(() => {
        window.request = originalRequest;
    });

    it('keeps legacy imports bound to the feature implementations', () => {
        const featureRequests = { ...memscopeRequests, ...memsnapshotRequests };
        expect(Object.keys(featureRequests).sort()).toEqual(cases.map(([name]) => name).sort());
        Object.entries(featureRequests).forEach(([name, request]) => {
            expect(requests[name as keyof typeof featureRequests]).toBe(request);
        });
    });

    it.each(cases)('%s preserves the command, parameters and response', async (_name, command, params, invoke) => {
        const response = { data: [{ id: 0 }], total: 1 };
        transport.mockResolvedValueOnce(response);
        await expect(invoke()).resolves.toBe(response);
        expect(transport).toHaveBeenCalledTimes(1);
        expect(transport).toHaveBeenCalledWith({ command, params });
        expect(transport.mock.calls[0][0].params).not.toBe(params);
    });

    it.each(cases)('%s preserves rejection without retrying', async (_name, _command, _params, invoke) => {
        const error = new Error('request cancelled');
        transport.mockRejectedValueOnce(error);
        await expect(invoke()).rejects.toBe(error);
        expect(transport).toHaveBeenCalledTimes(1);
    });

    it('keeps omitted snapshot options absent', async () => {
        await requests.getSnapshotDetail({ id: 0, type: 'block', deviceId: '0' });
        expect(transport).toHaveBeenCalledWith({
            command: 'Memory/snapshot/detail', params: { id: 0, type: 'block', deviceId: '0' },
        });
        expect(Object.keys(transport.mock.calls[0][0].params).sort()).toEqual(['deviceId', 'id', 'type']);
    });
});

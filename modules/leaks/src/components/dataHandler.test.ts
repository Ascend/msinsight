/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { Session } from '../entity/session';
import { getBlockTableData, getEventTableData } from './dataHandler';

jest.mock('@/leaksWorker/blockWorker/worker', () => ({
    workerSetMemoryBlockData: jest.fn(),
    workerTransform: jest.fn(),
}));

const requestMock = jest.fn();
window.request = requestMock;

describe('MemScope table data handlers', () => {
    beforeEach(() => {
        requestMock.mockReset();
        requestMock.mockResolvedValue({ headers: [], blocks: [], events: [], total: 0 });
    });

    test('builds the Snapshot block request from the current query state', async () => {
        const session = baseSession();
        session.module = 'memsnapshot';
        session.blocksCurrentPage = 2;
        session.blocksPageSize = 30;
        session.blocksOrder = true;
        session.blocksOrderBy = 'size';
        session.blocksFilters = { name: 'tensor' };
        session.blocksRangeFilters = { size: [10, 20] };

        await getBlockTableData(session);

        expect(requestMock).toHaveBeenCalledWith({
            command: 'Memory/snapshot/blocks',
            params: expect.objectContaining({
                currentPage: 2,
                pageSize: 30,
                desc: true,
                orderBy: 'size',
                filters: { name: 'tensor' },
                rangeFilters: { size: [10, 20] },
                isTable: true,
            }),
        });
    });

    test('uses the Leaks event endpoint and preserves ascending sort', async () => {
        const session = baseSession();
        session.module = 'leaks';
        session.eventsCurrentPage = 3;
        session.eventsPageSize = 20;
        session.eventsOrder = false;
        session.eventsOrderBy = 'timestamp';
        session.eventsFilters = { event: 'alloc' };

        await getEventTableData(session);

        expect(requestMock).toHaveBeenCalledWith({
            command: 'Memory/leaks/events',
            params: expect.objectContaining({
                currentPage: 3,
                pageSize: 20,
                desc: false,
                orderBy: 'timestamp',
                filters: { event: 'alloc' },
                isTable: true,
            }),
        });
    });
});

const baseSession = (): Session => {
    const session = new Session();
    session.deviceId = 'device-0';
    session.eventType = 'acl';
    session.minTime = 10;
    session.maxTime = 100;
    return session;
};

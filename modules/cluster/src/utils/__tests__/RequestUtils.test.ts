/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { queryCommunication, queryCommunicationOperatorLists } from '../RequestUtils';

const createRank = (rankId: number): any => ({
    rankId: `${rankId}`,
    dbPath: `rank-${rankId}.db`,
    lists: { compare: [], baseline: [], diff: [] },
});

describe('queryCommunicationOperatorLists', () => {
    beforeEach(() => {
        window.requestData = jest.fn();
    });

    it('merges multiple pages in rank order', async () => {
        const firstPage = Array.from({ length: 1000 }, (_, index) => createRank(index));
        const requestData = window.requestData as jest.Mock;
        requestData
            .mockResolvedValueOnce({ minTime: 1, maxTime: 9, total: 1001, data: firstPage })
            .mockResolvedValueOnce({ minTime: 1, maxTime: 9, total: 1001, data: [createRank(1000)] });

        const result = await queryCommunicationOperatorLists({
            iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
        });

        expect(result?.data).toHaveLength(1001);
        expect(result?.data[1000].rankId).toBe('1000');
        expect(requestData).toHaveBeenNthCalledWith(2, 'communication/operatorLists', expect.objectContaining({
            currentPage: 2,
            pageSize: 1000,
        }));
    });

    it('uses a legacy response without requesting another page', async () => {
        const requestData = window.requestData as jest.Mock;
        const legacy = { minTime: 1, maxTime: 9, data: [createRank(0)] };
        requestData.mockResolvedValue(legacy);

        await expect(queryCommunicationOperatorLists({
            iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
        })).resolves.toBe(legacy);
        expect(requestData).toHaveBeenCalledTimes(1);
    });

    it('accepts an empty paginated result', async () => {
        (window.requestData as jest.Mock).mockResolvedValue({ minTime: 0, maxTime: 0, total: 0, data: [] });

        const result = await queryCommunicationOperatorLists({
            iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
        });

        expect(result).toEqual({ minTime: 0, maxTime: 0, data: [] });
    });

    it('rejects changed pagination metadata', async () => {
        const firstPage = Array.from({ length: 1000 }, (_, index) => createRank(index));
        const requestData = window.requestData as jest.Mock;
        requestData
            .mockResolvedValueOnce({ minTime: 1, maxTime: 9, total: 1001, data: firstPage })
            .mockResolvedValueOnce({ minTime: 2, maxTime: 9, total: 1001, data: [createRank(1000)] });

        await expect(queryCommunicationOperatorLists({
            iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
        })).rejects.toThrow('metadata changed');
    });

    it('stops when the request is superseded', async () => {
        let latest = true;
        (window.requestData as jest.Mock).mockImplementation(async () => {
            latest = false;
            return { minTime: 1, maxTime: 9, total: 1200, data: Array.from({ length: 1000 }, (_, index) => createRank(index)) };
        });

        const result = await queryCommunicationOperatorLists({
            iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
        }, () => latest);

        expect(result).toBeUndefined();
        expect(window.requestData).toHaveBeenCalledTimes(1);
    });
});

const createDurationRank = (rankId: number): any => ({
    index: rankId,
    rankId: `${rankId}`,
    dbPath: `rank-${rankId}.db`,
    compareData: { compare: {}, baseline: {}, diff: {} },
});

const durationParams = {
    iterationId: '1', operatorName: 'AllReduce', pgName: 'pg', groupIdHash: 'group', baselineGroupIdHash: '',
};

const advice = [{ type: 'SDMA', max: 2, min: 1, avg: 1.5, diff: 1, time: 10 }];

describe('queryCommunication', () => {
    beforeEach(() => {
        window.requestData = jest.fn();
    });

    it('merges multiple duration pages in rank order', async () => {
        const firstPage = Array.from({ length: 1000 }, (_, index) => createDurationRank(index));
        const requestData = window.requestData as jest.Mock;
        requestData
            .mockResolvedValueOnce({ total: 1001, items: firstPage, advice })
            .mockResolvedValueOnce({ total: 1001, items: [createDurationRank(1000)], advice });

        const result = await queryCommunication(durationParams);

        expect(result?.items).toHaveLength(1001);
        expect(result?.items[1000].rankId).toBe('1000');
        expect(result?.advice).toEqual(advice);
        expect(requestData).toHaveBeenNthCalledWith(2, 'communication/duration/list', expect.objectContaining({
            currentPage: 2,
            pageSize: 1000,
        }));
    });

    it('uses a legacy duration response without requesting another page', async () => {
        const legacy = { items: [createDurationRank(0)], advice };
        (window.requestData as jest.Mock).mockResolvedValue(legacy);

        await expect(queryCommunication(durationParams)).resolves.toBe(legacy);
        expect(window.requestData).toHaveBeenCalledTimes(1);
    });

    it('accepts an empty paginated duration result', async () => {
        (window.requestData as jest.Mock).mockResolvedValue({ total: 0, items: [], advice: [] });

        await expect(queryCommunication(durationParams)).resolves.toEqual({ items: [], advice: [] });
    });

    it('rejects changed duration pagination metadata', async () => {
        const firstPage = Array.from({ length: 1000 }, (_, index) => createDurationRank(index));
        const requestData = window.requestData as jest.Mock;
        requestData
            .mockResolvedValueOnce({ total: 1001, items: firstPage, advice })
            .mockResolvedValueOnce({ total: 1001, items: [createDurationRank(1000)], advice: [] });

        await expect(queryCommunication(durationParams)).rejects.toThrow('metadata changed');
    });

    it('stops duration pagination when the request is superseded', async () => {
        let latest = true;
        (window.requestData as jest.Mock).mockImplementation(async () => {
            latest = false;
            return {
                total: 1200,
                items: Array.from({ length: 1000 }, (_, index) => createDurationRank(index)),
                advice,
            };
        });

        const result = await queryCommunication(durationParams, () => latest);

        expect(result).toBeUndefined();
        expect(window.requestData).toHaveBeenCalledTimes(1);
    });
});

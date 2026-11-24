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
import { CommunicationApi, groupsFromResponse, defaultOperator } from '../api';
import { TOTAL_OPERATOR } from '../domain/types';
const scope = { clusterPath: '/cluster', iterationId: '2', group: { key: 'hash:', stage: '(0, 1)', groupIdHash: 'hash', pgName: 'dp', label: 'dp' } };
const row = (rankId: number): object => ({ rankId, dbPath: `${rankId}.db`, lists: { compare: [{ operatorName: 'allReduce', startTime: 1000000, duration: 2000000 }], baseline: [] } });
it('preserves string iteration IDs and accepts only safe integers as numeric IDs', async () => {
    const request = jest.fn()
        .mockResolvedValueOnce({ iterationOrRankId: { compare: ['013', '9007199254740993', 0] } })
        .mockResolvedValueOnce({ iterationOrRankId: { compare: [Number.MAX_SAFE_INTEGER + 1] } });
    const api = new CommunicationApi(request);
    await expect(api.iterations('/cluster')).resolves.toEqual(['013', '9007199254740993', '0']);
    await expect(api.iterations('/cluster')).rejects.toThrow();
});
it('uses strategy and rank ordering and keeps the wire stage unchanged', () => {
    const groups = groupsFromResponse({
        data: [
            { group: '(2, 1)', parallelStrategy: '', groupIdHash: { compare: 'a' }, groupId: { compare: 'group_name_1' } },
            { group: '(3, 2, 1)', parallelStrategy: 'dp', groupIdHash: { compare: 'b' }, groupId: { compare: 'group_name_3', baseline: 'other' } },
            { group: '(0, 1)', parallelStrategy: 'dp', groupIdHash: { compare: 'c' } },
        ],
    });
    expect(groups.map(group => group.key)).toEqual(['b:', 'c:', 'a:']);
    expect(groups.map(group => group.label)).toEqual(['group_name_3 · dp', 'dp', 'group_name_1']);
    expect(groups[0]).toMatchObject({ stage: '(3, 2, 1)', groupIdHash: 'b' });
    expect(defaultOperator(['op', TOTAL_OPERATOR])).toBe(TOTAL_OPERATOR);
    expect(defaultOperator(['op'])).toBe('op');
});
it('loads complete old pages with noncompare parameters and preserves ns times', async () => {
    const request = jest.fn().mockResolvedValueOnce({ minTime: 0, maxTime: 3000000, total: 1001, data: Array.from({ length: 1000 }, (_, id) => row(id)) })
        .mockResolvedValueOnce({ minTime: 0, maxTime: 3000000, total: 1001, data: [row(1000)] });
    const data = await new CommunicationApi(request).timeline(scope, 'allReduce', 'tail', undefined);
    expect(data.rows).toHaveLength(1001); expect(data.rows[0].operators[0].duration).toBe(2000000);
    expect(request.mock.calls[1][1]).toMatchObject({ currentPage: 2, pageSize: 1000, isCompare: false, targetOperatorName: 'tail', groupIdHash: 'hash', pgName: 'dp', baselineGroupIdHash: '' });
});
it('rejects changing page metadata instead of showing truncated or mixed data', async () => {
    const request = jest.fn().mockResolvedValueOnce({ minTime: 0, maxTime: 3, total: 1001, data: Array.from({ length: 1000 }, (_, id) => row(id)) })
        .mockResolvedValueOnce({ minTime: 0, maxTime: 4, total: 1001, data: [row(1000)] });
    await expect(new CommunicationApi(request).timeline(scope, TOTAL_OPERATOR, '')).rejects.toThrow('metadata');
});
it('does not fetch a second page after cancellation', async () => {
    const controller = new AbortController(); const request = jest.fn(async () => {
        controller.abort(); return { minTime: 0, maxTime: 3, total: 1001, data: Array.from({ length: 1000 }, (_, id) => row(id)) };
    });
    await expect(new CommunicationApi(request).timeline(scope, TOTAL_OPERATOR, '', controller.signal)).rejects.toHaveProperty('name', 'AbortError');
    expect(request).toHaveBeenCalledTimes(1);
});
it('reads unpaged responses without requiring an invented total', async () => {
    const request = jest.fn().mockResolvedValue({ minTime: 0, maxTime: 3, data: [row(0)] });
    expect((await new CommunicationApi(request).timeline(scope, TOTAL_OPERATOR, '')).rows).toHaveLength(1);
});
it('parses packet JSON with the old MB/ms formula and string rank parameter', async () => {
    const request = jest.fn().mockResolvedValue({ distributionData: '{"2":[3,4]}' });
    expect(await new CommunicationApi(request).packets(scope, '4', 'RDMA')).toEqual([{ size: 2, count: 3, bandwidth: 1.5 }]);
    expect(request.mock.calls[0][1]).toEqual({ clusterPath: '/cluster', iterationId: '2', stage: '(0, 1)', pgName: 'dp', groupIdHash: 'hash', rankId: '4', transportType: 'RDMA', operatorName: TOTAL_OPERATOR });
});
it.each(['0', '9007199254740993'])('preserves rank ID %s as a string in both legacy bandwidth requests', async rankId => {
    const request = jest.fn().mockResolvedValueOnce({ items: [] }).mockResolvedValueOnce({ distributionData: '' });
    const api = new CommunicationApi(request);
    await api.transports(scope, rankId);
    await api.packets(scope, rankId, 'HCCS');
    expect(request.mock.calls.map(call => call[1].rankId)).toEqual([rankId, rankId]);
});
it.each(['null', '{}', null, undefined])('handles empty packet distribution %s', async distributionData => {
    expect(await new CommunicationApi(jest.fn().mockResolvedValue({ distributionData })).packets(scope, '0', 'HCCS')).toEqual([]);
});
it('uses the four documented commands with string identities and separate legacy parameters', async () => {
    const request = jest.fn().mockResolvedValue({ items: [], total: 0, currentPage: 1, pageSize: 20 }); const api = new CommunicationApi(request);
    const selected = { ...scope, iterationId: 'step013', group: { ...scope.group, groupIdHash: '3985310405473756128' } };
    const page = { currentPage: 1, pageSize: 20, orderBy: 'rankId' as const, order: 'ascend' as const };
    await api.groupRange('/c', '013'); await api.groupBandwidth('/c', '013', 'SDMA'); await api.fluctuation(selected); await api.ranks(selected, page);
    expect(request.mock.calls.map(call => call[0])).toEqual(['communication/group/rangeRatio', 'communication/group/bandwidth', 'communication/operator/fluctuation', 'communication/rank/statistics']);
    expect(request.mock.calls.map(call => call[1])).toEqual([
        { clusterPath: '/c', iterationId: '013' },
        { clusterPath: '/c', iterationId: '013', transportType: 'SDMA' },
        { clusterPath: '/cluster', iterationId: 'step013', groupName: '3985310405473756128' },
        { clusterPath: '/cluster', iterationId: 'step013', groupName: '3985310405473756128', ...page },
    ]);
});
it('never substitutes fake values when a new DTO is incompatible', async () => {
    await expect(new CommunicationApi(jest.fn().mockResolvedValue({ data: [] })).groupRange('/c', '13')).rejects.toThrow();
});

it('preserves group identity, backend ordering, null and zero without assigning units or thresholds', async () => {
    const items = [
        { groupName: '3985310405473756128', groupId: 'group_name_19', rangeRatio: 4.319597994375731, bandwidth: 20.905927651991874 },
        { groupName: '2', groupId: 'group_name_2', rangeRatio: null, bandwidth: null },
        { groupName: '3', groupId: 'group_name_3', rangeRatio: 1, bandwidth: 0 },
    ];
    const request = jest.fn().mockResolvedValue({ items }); const api = new CommunicationApi(request);
    expect(await api.groupRange('/c', '13')).toEqual({ items: items.map(item => ({ id: item.groupName, name: item.groupId, value: item.rangeRatio })) });
    expect(await api.groupBandwidth('/c', '13', 'HCCS')).toEqual({ items: items.map(item => ({ id: item.groupName, name: item.groupId, value: item.bandwidth })) });
});

it('preserves operator names and zero range time', async () => {
    const request = jest.fn().mockResolvedValue({ items: [{ opName: 'hcom_batchSendRecv__128_3_1', rangeTime: 0 }] });
    expect(await new CommunicationApi(request).fluctuation(scope)).toEqual({ items: [{ id: 'hcom_batchSendRecv__128_3_1', name: 'hcom_batchSendRecv__128_3_1', value: 0 }] });
});

it('loads only the requested server page with bandwidth sorting and preserves missing and unknown types', async () => {
    const request = jest.fn().mockResolvedValue({ total: 10002, currentPage: 2, pageSize: 1, items: [{ rankId: 10001, bandwidths: [{ bandType: 'NEW_TYPE', bandwidth: 12.34567 }, { bandType: 'SDMA', bandwidth: 0 }] }] });
    const query = { currentPage: 2, pageSize: 1, orderBy: 'bandwidth' as const, bandType: 'SDMA', order: 'descend' as const };
    const result = await new CommunicationApi(request).ranks(scope, query);
    expect(result).toEqual({ total: 10002, currentPage: 2, pageSize: 1, items: [{ rankId: '10001', bandwidth: { NEW_TYPE: 12.34567, SDMA: 0 } }] });
    expect(result.items[0].bandwidth.RDMA).toBeUndefined();
    expect(request).toHaveBeenCalledTimes(1);
    expect(request).toHaveBeenCalledWith('communication/rank/statistics', { clusterPath: '/cluster', iterationId: '2', groupName: 'hash', ...query }, undefined);
});

it('retains the total for an empty page beyond the last page', async () => {
    const response = { items: [], total: 2, currentPage: 3, pageSize: 1 };
    const api = new CommunicationApi(jest.fn().mockResolvedValue(response));
    expect(await api.ranks(scope, { currentPage: 3, pageSize: 1, orderBy: 'rankId', order: 'ascend' })).toEqual(response);
});

it.each([undefined, '0', NaN])('rejects missing or invalid metrics (%s) instead of treating them as null/zero', async value => {
    const request = jest.fn().mockResolvedValue({ items: [{ groupName: 'hash', groupId: 'display', rangeRatio: value }] });
    await expect(new CommunicationApi(request).groupRange('/c', '13')).rejects.toThrow();
});

it.each([1101, 3001, 3108, 3109, 3110])('preserves error code %s from the shared request layer', async code => {
    const error = Object.assign(new Error('backend failure'), { code });
    const api = new CommunicationApi(jest.fn().mockRejectedValue(error));
    await expect(api.groupRange('/c', '13')).rejects.toBe(error);
});

it('distinguishes an absent transport from an empty packet distribution', async () => {
    expect(await new CommunicationApi(jest.fn().mockResolvedValue({ distributionData: '' })).packets(scope, '0', 'HCCS')).toBeNull();
});

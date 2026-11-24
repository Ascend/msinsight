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
import type { CommunicationRequest } from '../api';
import { throwIfAborted } from '@insight/lib/connection';
import type { HostContext } from '../domain/types';
import { TOTAL_OPERATOR } from '../domain/types';
export const context: Partial<HostContext> = {
    project: 'Development preview',
    clusterPath: '/fixture/cluster',
    language: 'zh',
    dark: false,
    unitCount: 8,
    clusters: [{ name: '训练集群01', path: '/fixture/cluster', parsed: true, durationParsed: true }],
};
const operators = [TOTAL_OPERATOR, 'allReduce_0', 'reduceScatter_1', 'allGather_2'];
const rankCount = 32;
const groupBars = (iterationId: unknown, metric: 'rangeRatio' | 'bandwidth'): object => ({
    items: Array.from({ length: 20 }, (_, id) => ({ groupName: `${iterationId}-${id}`, groupId: `group_name_${id}`, [metric]: id === 19 ? null : 7 / (id + 1) })),
});
const rankPage = (params: Record<string, unknown>): object => {
    const currentPage = Number(params.currentPage ?? 1); const pageSize = Number(params.pageSize ?? 20);
    const rows = Array.from({ length: rankCount }, (_, rankId) => ({
        rankId,
        bandwidths: [{ bandType: 'RDMA', bandwidth: 18.5 + rankId % 4 }, { bandType: 'SDMA', bandwidth: 21 }, { bandType: 'HCCS', bandwidth: 15 }, { bandType: 'SIO', bandwidth: 0 }],
    }));
    rows.sort((a, b) => {
        const direction = params.order === 'descend' ? -1 : 1;
        if (params.orderBy !== 'bandwidth') { return direction * (a.rankId - b.rankId); }
        const left = a.bandwidths.find(item => item.bandType === params.bandType)?.bandwidth;
        const right = b.bandwidths.find(item => item.bandType === params.bandType)?.bandwidth;
        if (left === undefined || right === undefined) { return left === right ? a.rankId - b.rankId : left === undefined ? 1 : -1; }
        return direction * (left - right) || a.rankId - b.rankId;
    });
    return { total: rows.length, currentPage, pageSize, items: rows.slice((currentPage - 1) * pageSize, currentPage * pageSize) };
};
/** Development-only deterministic wire fixtures. Unknown commands fail instead of fabricating success. */
export const request: CommunicationRequest = async (command, params, signal) => {
    await new Promise(resolve => setTimeout(resolve, 80)); throwIfAborted(signal);
    switch (command) {
        case 'communication/duration/iterations': return { iterationOrRankId: { compare: ['1', '2'], baseline: [] } };
        case 'communication/matrix/group': return { data: ['dp', 'tp'].map((parallelStrategy, index) => ({
            group: `(${Array.from({ length: rankCount }, (_, n) => n).join(', ')})`, parallelStrategy,
            groupId: { compare: `group_name_${index}`, baseline: '' }, groupIdHash: { compare: `${params.iterationId}-${index}`, baseline: '' },
        })) };
        case 'communication/duration/operatorNames':
        case 'communication/matrix/sortOpNames': return { operatorName: operators };
        case 'communication/operatorLists': return { minTime: 3063000000, maxTime: 3095000000, data: Array.from({ length: rankCount }, (_, rankId) => ({ rankId, dbPath: `/fixture/rank-${rankId}.db`, lists: { baseline: [], compare: operators.slice(1).filter(name => params.operatorName === TOTAL_OPERATOR || name === params.operatorName).map((operatorName, index) => ({ operatorName, startTime: 3065000000 + index * 9000000 + (params.targetOperatorName ? 0 : rankId % 4 * 600000), duration: 7000000 - rankId % 4 * 400000 })) } })) };
        case 'communication/matrix/bandwidthInfo': return { matrixList: Array.from({ length: rankCount * rankCount }, (_, i) => ({ srcRank: i % rankCount, dstRank: Math.floor(i / rankCount), matrixData: { compare: { opName: String(params.operatorName), bandwidth: 10 + i % 7, transitSize: 7, transitTime: 0.5, transportType: i % 2 ? 'HCCS' : 'RDMA' } } })) };
        case 'communication/duration/slow-rank/list': return { hasAdvice: true, fastRankId: '4', fastTotalElapseTime: 187834.54, data: [1, 5, 0].map(rankId => ({ rankId, totalElapseTime: 97490.628, totalDiffTime: 90343.912, opList: [{ name: 'allReduce_0', startTime: 3065000 + rankId % 4 * 600, elapseTime: 7000, diffTime: 1000, maxTime: 8000, maxStartTime: 3065000 }] })) };
        case 'communication/advisor': return { items: [] };
        case 'communication/bandwidth': return { items: ['SDMA', 'HCCS', 'SIO'].map(transportType => ({ transportType, transitSize: 4404.0859, transitTime: 39.4747, bandwidth: 111.5672, largePacketRatio: 0 })) };
        case 'communication/distribution': return { distributionData: ['HCCS', 'SIO'].includes(String(params.transportType)) ? JSON.stringify({ 0: [0, 0], 0.001: [34, 0.01], 0.0017: [8, 0.001], 0.0022: [3, 0.0003], 6.2915: [693, 39.4] }) : '' };
        case 'communication/group/rangeRatio': return groupBars(params.iterationId, 'rangeRatio');
        case 'communication/group/bandwidth': return groupBars(params.iterationId, 'bandwidth');
        case 'communication/operator/fluctuation': return { items: operators.slice(1).map((opName, id) => ({ opName, rangeTime: id })) };
        case 'communication/rank/statistics': return rankPage(params);
        case 'unit/kernelDetail': return { pid: '100', startTime: 3065000000, rankId: params.rankId, duration: 7000000 };
        default: throw new Error(`No development fixture: ${command}`);
    }
};

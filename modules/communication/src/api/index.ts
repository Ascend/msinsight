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
import type { Advice, BarResult, RankQuery, RankResult, Group, MatrixEdge, Packet, RankSelection, Scope, SlowRanks, Timeline, Transport } from '../domain/types';
import { TOTAL_OPERATOR } from '../domain/types';
import { throwIfAborted } from '@insight/lib/connection';
import { readArray, readFiniteNumber, readNullableNumber, readRecord, readSafeInteger, readString } from '@insight/lib/utils/validation';

export type CommunicationRequest = (command: string, params: Record<string, unknown>, signal?: AbortSignal, module?: string) => Promise<unknown>;

// Legacy iteration and rank IDs may be strings or safe integers. Preserve string IDs verbatim.
function readId(value: unknown): string {
    return typeof value === 'number' && Number.isSafeInteger(value) ? String(value) : readString(value);
}

export const scopeParams = (scope: Scope): Record<string, unknown> => ({
    clusterPath: scope.clusterPath,
    iterationId: scope.iterationId,
    stage: scope.group.stage,
    pgName: scope.group.pgName,
    groupIdHash: scope.group.groupIdHash,
});
export const durationParams = (scope: Scope, operatorName = TOTAL_OPERATOR): Record<string, unknown> => ({
    ...scopeParams(scope), operatorName, isCompare: false, baselineIterationId: '', baselineGroupIdHash: '',
});

/** Preserve old ordering: strategy first, then descending rank count and numeric rank order. */
export function groupsFromResponse(raw: unknown): Group[] {
    const rows = readArray(readRecord(raw).data);
    const groups = rows.map(rawRow => {
        const row = readRecord(rawRow);
        const stage = readString(row.group);
        const pgName = readString(row.parallelStrategy);
        const hash = readString(readRecord(row.groupIdHash).compare);
        const groupId = row.groupId == null ? '' : readString(readRecord(row.groupId).compare ?? '');
        return {
            key: `${hash}:`,
            stage,
            pgName,
            groupIdHash: hash,
            label: [groupId, pgName].filter(Boolean).join(' · ') || '—',
        };
    });
    return groups.sort((a, b) => {
        if (Boolean(a.pgName) !== Boolean(b.pgName)) { return a.pgName ? -1 : 1; }
        if (a.pgName !== b.pgName) { return a.pgName < b.pgName ? -1 : 1; }
        const aRanks = (a.stage.match(/\d+/g) ?? []).map(Number).sort((x, y) => x - y);
        const bRanks = (b.stage.match(/\d+/g) ?? []).map(Number).sort((x, y) => x - y);
        if (aRanks.length !== bRanks.length) { return bRanks.length - aRanks.length; }
        for (let i = 0; i < aRanks.length; i++) { if (aRanks[i] !== bRanks[i]) { return aRanks[i] - bRanks[i]; } }
        return 0;
    });
}

export function defaultOperator(options: string[]): string { return options.includes(TOTAL_OPERATOR) ? TOTAL_OPERATOR : options[0] ?? ''; }

/** Original APIs optionally paginate. Validate each page before merging; abort between pages. */
async function allPages(request: CommunicationRequest, command: string, params: Record<string, unknown>, field: string, metadata: string[], signal?: AbortSignal): Promise<Record<string, unknown>> {
    const rows: unknown[] = [];
    let first: Record<string, unknown> | undefined;
    for (let currentPage = 1; ; currentPage++) {
        throwIfAborted(signal);
        const page = readRecord(await request(command, { ...params, currentPage, pageSize: 1000 }, signal));
        throwIfAborted(signal);
        const data = readArray(page[field]);
        if (first === undefined) {
            first = page;
            if (page.total === undefined) { return page; }
            if (!Number.isSafeInteger(page.total) || (page.total as number) < 0) { throw new Error('Invalid pagination total'); }
        }
        const total = first.total as number;
        if (page.total !== total || data.length !== Math.min(1000, Math.max(0, total - rows.length)) ||
            metadata.some(key => JSON.stringify(first?.[key]) !== JSON.stringify(page[key]))) {
            throw new Error('Pagination metadata changed while loading');
        }
        rows.push(...data);
        if (rows.length >= total) { return { ...first, [field]: rows }; }
    }
}

/** DB 旧接口的 groupIdHash 与新接口的 groupName 均来自原始 group_name，始终保持字符串。 */
const statisticsParams = (scope: Scope): Record<string, unknown> => ({
    clusterPath: scope.clusterPath, iterationId: scope.iterationId, groupName: scope.group.groupIdHash,
});

function groupBars(raw: unknown, metric: 'rangeRatio' | 'bandwidth'): BarResult {
    return {
        items: readArray(readRecord(raw).items).map(rawItem => {
            const item = readRecord(rawItem);
            return { id: readString(item.groupName), name: readString(item.groupId), value: item[metric] === null ? null : readFiniteNumber(item[metric]) };
        }),
    };
}

/** 通信页面接口：统一组装请求参数并将响应转换为页面使用的数据。 */
export class CommunicationApi {
    constructor(private readonly request: CommunicationRequest) {}

    /** 获取集群迭代 ID，供公共迭代筛选使用。 */
    async iterations(clusterPath: string, signal?: AbortSignal): Promise<string[]> {
        const result = readRecord(await this.request('communication/duration/iterations', { clusterPath, isCompare: false }, signal));
        return readArray(readRecord(result.iterationOrRankId).compare).map(readId);
    }

    /** 获取指定迭代的通信域，并按并行策略和卡号排序。 */
    async groups(clusterPath: string, iterationId: string, signal?: AbortSignal): Promise<Group[]> {
        return groupsFromResponse(await this.request('communication/matrix/group', { clusterPath, iterationId, baselineIterationId: '', isCompare: false }, signal));
    }

    /** 获取通信耗时图或矩阵的算子选项；matrix 决定使用哪个选项接口。 */
    async operators(scope: Scope, matrix: boolean, signal?: AbortSignal): Promise<string[]> {
        const result = readRecord(await this.request(matrix ? 'communication/matrix/sortOpNames' : 'communication/duration/operatorNames', scopeParams(scope), signal));
        return readArray(result.operatorName).map(readString);
    }

    /** 获取通信耗时图数据，合并分页并保留 ns 时间单位；targetOperatorName 用于算子尾对齐。 */
    async timeline(scope: Scope, operatorName: string, targetOperatorName: string, signal?: AbortSignal): Promise<Timeline> {
        const result = await allPages(this.request, 'communication/operatorLists', { ...durationParams(scope, operatorName), targetOperatorName }, 'data', ['minTime', 'maxTime'], signal);
        return {
            minTime: readFiniteNumber(result.minTime),
            maxTime: readFiniteNumber(result.maxTime),
            rows: readArray(result.data).map(raw => {
                const row = readRecord(raw);
                return {
                    rankId: readId(row.rankId),
                    dbPath: readString(row.dbPath),
                    operators: readArray(readRecord(row.lists).compare).map(item => {
                        const op = readRecord(item);
                        return { operatorName: readString(op.operatorName), startTime: readFiniteNumber(op.startTime), duration: readFiniteNumber(op.duration) };
                    }),
                };
            }),
        };
    }

    /** 获取指定通信域、算子的通信矩阵链路数据。 */
    async matrix(scope: Scope, operatorName: string, signal?: AbortSignal): Promise<MatrixEdge[]> {
        const result = readRecord(await this.request('communication/matrix/bandwidthInfo', { ...scopeParams(scope), operatorName, isCompare: false, baselineGroupIdHash: '' }, signal));
        return readArray(result.matrixList).map(raw => {
            const edge = readRecord(raw); const data = readRecord(readRecord(edge.matrixData).compare);
            return {
                srcRank: readFiniteNumber(edge.srcRank),
                dstRank: readFiniteNumber(edge.dstRank),
                bandwidth: readFiniteNumber(data.bandwidth),
                transitSize: readFiniteNumber(data.transitSize),
                transitTime: readFiniteNumber(data.transitTime),
                transportType: readString(data.transportType),
                opName: readString(data.opName),
            };
        });
    }

    /** 获取潜在慢卡列表及算子耗时差异，供慢卡分析和定位高亮使用。 */
    async slowRanks(scope: Scope, signal?: AbortSignal): Promise<SlowRanks> {
        const result = readRecord(await this.request('communication/duration/slow-rank/list', { ...durationParams(scope), type: 'CommunicationDurationAnalysis' }, signal));
        if (typeof result.hasAdvice !== 'boolean') { throw new Error('Invalid slow-rank advice'); }
        return {
            hasAdvice: result.hasAdvice,
            fastRankId: readId(result.fastRankId),
            fastTotalElapseTime: readFiniteNumber(result.fastTotalElapseTime),
            data: readArray(result.data).map(raw => {
                const row = readRecord(raw);
                return {
                    rankId: readId(row.rankId),
                    totalElapseTime: readFiniteNumber(row.totalElapseTime),
                    totalDiffTime: readFiniteNumber(row.totalDiffTime),
                    opList: readArray(row.opList).map(rawOp => {
                        const op = readRecord(rawOp);
                        return { name: readString(op.name), startTime: readFiniteNumber(op.startTime), elapseTime: readFiniteNumber(op.elapseTime), diffTime: readFiniteNumber(op.diffTime), maxTime: readFiniteNumber(op.maxTime), maxStartTime: readFiniteNumber(op.maxStartTime) };
                    }),
                };
            }),
        };
    }

    /** 获取指定卡的传输类型统计；旧协议的 rankId 为字符串，不能转为数字。 */
    async transports(scope: Scope, rankId: string, signal?: AbortSignal): Promise<Transport[]> {
        const result = readRecord(await this.request('communication/bandwidth', { ...scopeParams(scope), rankId, operatorName: TOTAL_OPERATOR }, signal));
        return readArray(result.items).map(raw => {
            const row = readRecord(raw);
            return { transportType: readString(row.transportType), transitSize: readFiniteNumber(row.transitSize), transitTime: readFiniteNumber(row.transitTime), bandwidth: readFiniteNumber(row.bandwidth), largePacketRatio: readNullableNumber(row.largePacketRatio) };
        });
    }

    /** 获取指定卡、传输类型的数据包分布；通过集群和 rankId 查询，不依赖单卡 dbPath；null 表示该传输类型不存在。 */
    async packets(scope: Scope, rankId: string, transportType: string, signal?: AbortSignal): Promise<Packet[] | null> {
        const result = readRecord(await this.request('communication/distribution', { ...scopeParams(scope), rankId, operatorName: TOTAL_OPERATOR, transportType }, signal));
        // An empty string means this transport is absent; null/{} mean available but empty.
        if (result.distributionData === '') { return null; }
        if (result.distributionData === null || result.distributionData === undefined) { return []; }
        const raw = readString(result.distributionData);
        if (raw === '' || raw === 'null') { return []; }
        return Object.entries(readRecord(JSON.parse(raw))).map(([key, value]) => {
            const pair = readArray(value); const size = readFiniteNumber(Number(key));
            if (pair.length !== 2 || size < 0) { throw new Error('Invalid packet distribution'); }
            const count = readFiniteNumber(pair[0]); const time = readFiniteNumber(pair[1]);
            return { size, count, bandwidth: time === 0 ? 0 : Number((size * count / time).toFixed(4)) };
        }).sort((a, b) => a.size - b.size);
    }

    /** 获取集群级通信诊断建议，该接口不接收迭代或通信域条件。 */
    async clusterAdvice(clusterPath: string, signal?: AbortSignal): Promise<Advice[]> {
        const result = readRecord(await this.request('communication/advisor', { clusterPath }, signal));
        return readArray(result.items).map(raw => {
            const item = readRecord(raw);
            return { name: readString(item.name), statistics: Object.fromEntries(Object.entries(readRecord(item.statistics)).map(([key, values]) => [key, readArray(values).map(readString)])) };
        });
    }

    /** 向 Timeline 模块查询算子定位信息，供右键跳转使用。 */
    async locate(rank: RankSelection, name: string, clusterPath: string, signal?: AbortSignal): Promise<Record<string, unknown>> {
        return readRecord(await this.request('unit/kernelDetail', { ...rank, name, clusterPath }, signal, 'timeline'));
    }

    /** 获取指定迭代的域级耗时极差比（最大值 / 最小值，无量纲）；最小值为 0 时保留 null。 */
    async groupRange(clusterPath: string, iterationId: string, signal?: AbortSignal): Promise<BarResult> {
        return groupBars(await this.request('communication/group/rangeRatio', { clusterPath, iterationId }, signal), 'rangeRatio');
    }

    /** 获取指定迭代、带宽类型的域聚合带宽（总传输量 / 总传输时间）；保留接口原始精度。 */
    async groupBandwidth(clusterPath: string, iterationId: string, transportType: string, signal?: AbortSignal): Promise<BarResult> {
        return groupBars(await this.request('communication/group/bandwidth', { clusterPath, iterationId, transportType }, signal), 'bandwidth');
    }

    /** 获取当前迭代、通信域的算子耗时极差（最大值 - 最小值），以原始精度供图表展示。 */
    async fluctuation(scope: Scope, signal?: AbortSignal): Promise<BarResult> {
        const result = readRecord(await this.request('communication/operator/fluctuation', statisticsParams(scope), signal));
        return {
            items: readArray(result.items).map(raw => {
                const item = readRecord(raw); const name = readString(item.opName);
                return { id: name, name, value: readFiniteNumber(item.rangeTime) };
            }),
        };
    }

    /** 获取域内卡级带宽：服务端全量排序后按 rank 分页；缺失类型不补 0，保留未知类型。 */
    async ranks(scope: Scope, query: RankQuery, signal?: AbortSignal): Promise<RankResult> {
        const result = readRecord(await this.request('communication/rank/statistics', { ...statisticsParams(scope), ...query }, signal));
        return {
            total: readSafeInteger(result.total, 0),
            currentPage: readSafeInteger(result.currentPage, 1),
            pageSize: readSafeInteger(result.pageSize, 1),
            items: readArray(result.items).map(raw => {
                const row = readRecord(raw);
                return {
                    rankId: String(readSafeInteger(row.rankId, 0)),
                    bandwidth: Object.fromEntries(readArray(row.bandwidths).map(rawBandwidth => {
                        const item = readRecord(rawBandwidth);
                        return [readString(item.bandType), readFiniteNumber(item.bandwidth)];
                    })),
                };
            }),
        };
    }
}

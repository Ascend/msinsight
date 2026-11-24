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
export interface Cluster { name: string; path: string; parsed: boolean; durationParsed: boolean }
export interface HostContext {
    project: string;
    clusterPath: string;
    clusters: Cluster[];
    generation: number;
    unitCount: number;
    dark: boolean;
    language: 'zh' | 'en';
    compare: boolean;
}
export interface Group { key: string; label: string; stage: string; pgName: string; groupIdHash: string }
export interface Scope { clusterPath: string; iterationId: string; group: Group }
export interface Operator { operatorName: string; startTime: number; duration: number }
export interface RankTimeline { rankId: string; dbPath: string; operators: Operator[] }
export interface Timeline { minTime: number; maxTime: number; rows: RankTimeline[] }
export interface MatrixEdge {
    srcRank: number; dstRank: number; bandwidth: number; transitSize: number; transitTime: number; transportType: string; opName: string;
}
export interface SlowOperator { name: string; startTime: number; elapseTime: number; diffTime: number; maxTime: number; maxStartTime: number }
export interface SlowRank { rankId: string; totalElapseTime: number; totalDiffTime: number; opList: SlowOperator[] }
export interface SlowRanks { hasAdvice: boolean; fastRankId: string; fastTotalElapseTime: number; data: SlowRank[] }
export interface RankSelection { rankId: string; dbPath: string }
export interface Transport { transportType: string; transitSize: number; transitTime: number; bandwidth: number; largePacketRatio: number | null }
export interface Packet { size: number; count: number; bandwidth: number }
export interface Advice { name: string; statistics: Record<string, string[]> }
export interface BarDatum { id: string; name: string; value: number | null }
/** Chart data adapted from communication statistics; labels are localized by the view. */
export interface BarResult { items: BarDatum[] }
export interface RankStatistics { rankId: string; bandwidth: Record<string, number | null> }
export type RankSort = { order: 'ascend' | 'descend' } & ({ orderBy: 'rankId' } | { orderBy: 'bandwidth'; bandType: string });
export type RankQuery = RankSort & { currentPage: number; pageSize: number };
export interface RankResult { items: RankStatistics[]; total: number; currentPage: number; pageSize: number }
export interface LocationIntent { iterationId?: string; stage?: string; pgName?: string; groupIdHash?: string; operatorName?: string }
export const TOTAL_OPERATOR = 'Total Op Info';
export const TRANSPORTS = ['RDMA', 'SDMA', 'HCCS', 'PCIE', 'SIO'] as const;

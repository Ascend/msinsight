/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import {
    GetParallelismPerformanceData,
    GetParallelismPerformanceRes,
    GetParallelStrategyRes, ImportExpertDataParams,
    ParallelismArrangementParams,
    ParallelismArrangementResult,
    QueryModelInfoResult,
    QueryExpertHotspotParams,
    QueryExpertHotspotResult,
    SetParallelStrategyParams,
    PacketAndBandwidthChartsParams,
    GetSlowRankAdvise,
    GetSlowRankAdviseRes,
    GetSlowRankListResult,
    GetSlowRankListParams,
} from './interface';
import { createCancelableApi, createSmartDebounceRequestFunc } from '@insight/lib/utils';
import { store } from '../store';
import type { AnalysisChartData, OperatorTimeInfo } from '../components/communication/CommunicationTimeAnalysisChart';
import type { DataItem } from '../components/communication/CommunicationTimeChart';
import type { CommunicationAdvice } from '../components/communication/CommunicationDuration/AdviceLabel';

type ParamsWithClusterPath<T> = T & {
    clusterPath?: string;
};

function withClusterPath<T>(params: ParamsWithClusterPath<T>): ParamsWithClusterPath<T> {
    const session = store.sessionStore.activeSession;
    return { ...params, clusterPath: session?.selectedClusterPath };
}

/**
 * 查询所有迭代ID
 * 无参
 * @return {[]} 返回迭代数组[0,1,2,3]
 */
export const queryIterations = async(param: {isCompare: boolean}): Promise<any> => {
    return window.requestDataDebounced('communication/duration/iterations', withClusterPath(param));
};

/**
 * 查询一次迭代下所有通信域
 * 无参
 * @return {[]} 返回迭代数组['(0,1,2)']
 */
export const queryStages = createSmartDebounceRequestFunc(
    async(param: {iterationId: string;baselineIterationId: string;isCompare: boolean }): Promise<any> => {
        return window.requestData('communication/matrix/group', withClusterPath(param));
    }, { delay: 300, leading: true },
);

/**
 * 查询一次迭代下所有Rank ID
 * @param {string} iterationId 迭代ID
 * @return {[]} 返回Rank数组[0,1,2,3]
 */
export const queryRanks = async(param: {iterationId: string }): Promise<any> => {
    return window.requestData('communication/duration/ranks', withClusterPath(param));
};

/**
 * 查询迭代ID下某些RankID上运行的算子的名字
 * @param {string} iterationId 迭代ID
 * @param {number[]} rankList rankId数组
 * @return {[]} 返回算子名数组[0,1,2,3]
 */
export const queryOperators = async(param: {iterationId: string ;stage: string; pgName: string; groupIdHash: string}): Promise<any> => {
    return window.requestData('communication/duration/operatorNames', withClusterPath(param));
};

/**
 * 查询通信矩阵中所有算子的名称
 * @param {string} iterationId 迭代ID
 * @param {string} stage 通信域
 * @return {[]} 返回算子名数组[0,1,2,3]
 */
export const queryMatrixOperators = async(param: {iterationId: string ;stage: string; pgName: string; groupIdHash: string}): Promise<any> => {
    return window.requestDataDebounced('communication/matrix/sortOpNames', withClusterPath(param));
};

/**
 * 查询通信时间详情
 *
 * @param {string} iterationId 迭代ID
 * @param {number[]} rankIds
 * @param {string} operatorName 算子名
 * @return {[]} 返回数组
 */
interface CommunicationDurationParams {
    iterationId: string;
    baselineIterationId?: string;
    operatorName: string;
    stage?: string;
    targetOperatorName?: string;
    isCompare?: boolean;
    pgName: string;
    groupIdHash: string;
    baselineGroupIdHash: string;
}

const COMMUNICATION_DURATION_LIST_PAGE_SIZE = 1000;

export interface CommunicationDurationResponse {
    items: DataItem[];
    advice: CommunicationAdvice[];
    total?: number;
}

const validateDurationPage = (
    page: CommunicationDurationResponse,
    expectedTotal: number,
    currentPage: number,
    firstPage: CommunicationDurationResponse,
): void => {
    const offset = (currentPage - 1) * COMMUNICATION_DURATION_LIST_PAGE_SIZE;
    const expectedLength = Math.min(COMMUNICATION_DURATION_LIST_PAGE_SIZE, Math.max(0, expectedTotal - offset));
    if (page.total !== expectedTotal || !Array.isArray(page.items) || page.items.length !== expectedLength ||
        !Array.isArray(page.advice) || JSON.stringify(page.advice) !== JSON.stringify(firstPage.advice)) {
        throw new Error('Communication duration pagination metadata changed while loading');
    }
};

export const queryCommunication = async(
    param: CommunicationDurationParams,
    isLatestRequest: () => boolean = () => true,
): Promise<CommunicationDurationResponse | undefined> => {
    let currentPage = 1;
    let firstPage: CommunicationDurationResponse | undefined;
    const mergedItems: DataItem[] = [];
    for (;;) {
        const page = await window.requestData('communication/duration/list', withClusterPath({
            ...param,
            currentPage,
            pageSize: COMMUNICATION_DURATION_LIST_PAGE_SIZE,
        })) as CommunicationDurationResponse;
        if (!isLatestRequest()) {
            return undefined;
        }
        if (firstPage === undefined) {
            firstPage = page;
            if (page.total === undefined) {
                return page;
            }
            if (!Number.isSafeInteger(page.total) || page.total < 0 || !Array.isArray(page.advice)) {
                throw new Error('Invalid communication duration pagination metadata');
            }
        }
        const expectedTotal = firstPage.total;
        if (expectedTotal === undefined) {
            return firstPage;
        }
        validateDurationPage(page, expectedTotal, currentPage, firstPage);
        mergedItems.push(...page.items);
        if (mergedItems.length >= expectedTotal) {
            return { items: mergedItems, advice: firstPage.advice };
        }
        currentPage++;
    }
};

/**
 * 查询本张卡的通信耗时分析中的专家建议
 * 无参
 * @return {[]} 返回数组
 */
export const queryCommunicationExpertAdvisor = async(): Promise<any> => {
    return window.requestDataDebounced('communication/advisor', withClusterPath({}));
};

/**
 * 查询通信算子时间列表
 *
 * @param {string} iterationId 迭代ID
 * @param {number[]} rankIds
 * @param {string} operatorName 算子名
 */
const COMMUNICATION_OPERATOR_LIST_PAGE_SIZE = 1000;

export interface CommunicationOperatorListsResponse extends AnalysisChartData {
    total?: number;
}

const validateOperatorListPage = (
    page: CommunicationOperatorListsResponse,
    expectedTotal: number,
    currentPage: number,
    firstPage: CommunicationOperatorListsResponse,
): void => {
    const offset = (currentPage - 1) * COMMUNICATION_OPERATOR_LIST_PAGE_SIZE;
    const expectedLength = Math.min(COMMUNICATION_OPERATOR_LIST_PAGE_SIZE, Math.max(0, expectedTotal - offset));
    if (page.total !== expectedTotal || page.minTime !== firstPage.minTime || page.maxTime !== firstPage.maxTime ||
        !Array.isArray(page.data) || page.data.length !== expectedLength) {
        throw new Error('Communication operator pagination metadata changed while loading');
    }
};

export const queryCommunicationOperatorLists = async(
    param: CommunicationDurationParams,
    isLatestRequest: () => boolean = () => true,
): Promise<CommunicationOperatorListsResponse | undefined> => {
    let currentPage = 1;
    let firstPage: CommunicationOperatorListsResponse | undefined;
    const mergedData: OperatorTimeInfo[] = [];
    for (;;) {
        const page = await window.requestData('communication/operatorLists', withClusterPath({
            ...param,
            currentPage,
            pageSize: COMMUNICATION_OPERATOR_LIST_PAGE_SIZE,
        })) as CommunicationOperatorListsResponse;
        if (!isLatestRequest()) {
            return undefined;
        }
        if (firstPage === undefined) {
            firstPage = page;
            if (page.total === undefined) {
                return page;
            }
            if (!Number.isSafeInteger(page.total) || page.total < 0) {
                throw new Error('Invalid communication operator pagination total');
            }
        }
        const expectedTotal = firstPage.total;
        if (expectedTotal === undefined) {
            return firstPage;
        }
        validateOperatorListPage(page, expectedTotal, currentPage, firstPage);
        mergedData.push(...page.data);
        if (mergedData.length >= expectedTotal) {
            return { minTime: firstPage.minTime, maxTime: firstPage.maxTime, data: mergedData };
        }
        currentPage++;
    }
};

/**
 * 分页查询某个算子的通信时间详情
 *
 * @param {rankId} rankId RankID
 * @param {number} pageSize
 * @param {number} currentPage
 * @return {[]} 返回数组
 */
export const queryOperatorDetails = async(param: {
    iterationId: number; rankId: number; dbPath: string; pageSize: number;currentPage: number;orderBy: string;order: string;
    stage: string;queryType?: string;pgName: string; groupIdHash: string;
}): Promise<any> => {
    return window.requestData('communication/operatorDetails', withClusterPath(param));
};

/**
 * 查询计算分析总览通信/计算时间统计值
 *
 * @param {rankId} rankId RankID
 * @param {string} timeFlag 类型：SCMA、
 * @return {[]} 返回数组
 */
export const querySummaryStatistics = async (param: ParamsWithClusterPath<{rankId: string; dbPath: string; timeFlag: string;stepId: string}>): Promise<any> => {
    return window.requestData('summary/statistic', withClusterPath(param));
};

/**
 * 查询计算分析总览通信/计算时间详情
 *
 * @param {rankId} rankId RankID
 * @param {string} timeFlag 类型：Communication、Computing……
 * @param {number} pageSize
 * @param {number} currentPage
 * @return {[]} 返回数组
 */
export const queryComputeDetail = async (param: ParamsWithClusterPath<{
    rankId: string; dbPath: string; timeFlag: string; pageSize: number;currentPage: number;orderBy: string;order: string;
}>): Promise<any> => {
    return window.requestData('summary/queryComputeDetail', withClusterPath(param));
};

export const queryCommunicationDetail = async (param: ParamsWithClusterPath<{
    rankId: string; dbPath: string; pageSize: number;currentPage: number;orderBy: string;order: string;}>): Promise<any> => {
    return window.requestData('summary/queryCommunicationDetail', withClusterPath(param));
};

/**
 *
 * @param param
 *  step: string | number;
 *  rankIds: string[];
 *  orderBy: string ;
 *  top: number;
 */
export const queryTopSummary = async (param: ParamsWithClusterPath<{isCompare?: boolean}>): Promise<any> => {
    return window.requestData('summary/queryTopData', withClusterPath({
        ...param,
        stepIdList: [],
    }));
};

/**
 * 查询通信矩阵
 *
 * @param {string} iterationId 迭代ID
 * @param {string} stage
 * @param {string} operatorName 算子名
 * @return {[]} 返回数组
 */
export const queryCommunicationMatrix = async(param: { iterationId: string ; pgName: string ; stage: string ; operatorName: string; isCompare: boolean; groupIdHash: string; baselineGroupIdHash: string}):
Promise<any> => {
    return window.requestDataDebounced('communication/matrix/bandwidthInfo', withClusterPath(param));
};

export interface QueryFwpBwdTimelineParams {
    stepId: string;
    stageId: string;
}
export interface TraceItem {
    name: string;
    start: number;
    duration: number;
    pid: string;
    tid: string;
    id: string;
    cname: string;
}

interface RankItem {
    rank: string;
    componentList: Array<{
        component: 'FP/BP' | 'P2P Op';
        traceList: TraceItem[];
    }>;
}
export interface QueryFwpBwdTimelineRes {
    minTime: number;
    maxTime: number;
    rankList: RankItem[];
    flowList: Array<
    Array<{
        rankId: string;
        startTime: number;
        opName: string;
    }>
    >;
}

export const queryFwpBwdTimeline = async(params: ParamsWithClusterPath<QueryFwpBwdTimelineParams>): Promise<QueryFwpBwdTimelineRes> => {
    return window.requestData('parallelism/pipeline/fwdBwdTimeline', withClusterPath(params), 'summary');
};

export const getParallelStrategy = async (params: ParamsWithClusterPath<Record<string, unknown>>): Promise<GetParallelStrategyRes> => {
    return await window.requestData('summary/query/parallelStrategy', params, 'summary');
};

export const setParallelStrategy = async (params: SetParallelStrategyParams): Promise<void> => {
    return await window.requestData('summary/set/parallelStrategy', withClusterPath(params), 'summary');
};

/**
 * 获取并行策略排布数据
 * @param {ParallelismArrangementParams} params
 * @return {ParallelismArrangementResult}
 */
export const queryParallelismArrangementCancelable = createCancelableApi(
    async(params: ParamsWithClusterPath<ParallelismArrangementParams>): Promise<ParallelismArrangementResult> => {
        return await window.requestData('parallelism/arrangement/all', withClusterPath(params), 'summary');
    },
);

export const queryAllConnections = async(params: ParamsWithClusterPath<ParallelismArrangementParams>): Promise<ParallelismArrangementResult> => {
    return await window.requestData('parallelism/arrangement/all', withClusterPath(params), 'summary');
};

export const getParallelismPerformanceDataCancelable = createCancelableApi(
    async (params: GetParallelismPerformanceData): Promise<GetParallelismPerformanceRes> => {
        return await window.requestData('parallelism/performance/data', withClusterPath(params), 'summary');
    },
);

export const slowRankAdvisor = async (params: GetSlowRankAdvise): Promise<GetSlowRankAdviseRes> => {
    return await window.requestData('summary/slowRank/advisor', withClusterPath(params), 'summary');
};

/**
 * 导入 MoE 专家负载均衡数据
 */
export const importExpertData = async(params: ImportExpertDataParams): Promise<void> => {
    return await window.requestData('summary/importExpertData', withClusterPath(params), 'summary');
};

/**
 * 查询 MoE 专家负载均衡数据
 */
export const queryExpertHotspot = async(params: QueryExpertHotspotParams): Promise<QueryExpertHotspotResult> => {
    return await window.requestData('summary/queryExpertHotspot', withClusterPath(params), 'summary');
};

/**
 * 查询 MoE 专家负载均衡搜索条件
 */
export const queryModelInfo = async(): Promise<QueryModelInfoResult> => {
    return await window.requestData('summary/queryModelInfo', withClusterPath({}), 'summary');
};

export async function queryCommunicationBandwidth({ iterationId, rankId, operatorName, stage, pgName, groupIdHash }:
{ iterationId: string; rankId: number; dbPath: string; operatorName: string; stage: string; pgName: string; groupIdHash: string }): Promise<any> {
    const bandwidthDetails = await window.requestData('communication/bandwidth',
        withClusterPath({ iterationId, rankId, operatorName, stage, pgName, groupIdHash }));
    return bandwidthDetails?.items ?? [];
}

export async function queryCommunicationDistribution({ domId, iterationId, rankId, dbPath, operatorName, stage, pgName, groupIdHash }:
PacketAndBandwidthChartsParams): Promise<any> {
    const distributions = await window.requestData('communication/distribution',
        withClusterPath({ iterationId, rankId, dbPath, operatorName, transportType: domId, stage, pgName, groupIdHash }));
    return distributions?.distributionData ?? '{}';
}

export async function queryTimelineUnitKernelDetail(params: {
    name: string;
    rankId: string;
    dbPath: string;
}): Promise<any> {
    return await window.requestData('unit/kernelDetail', withClusterPath(params), 'timeline');
}

// 通信时序图慢卡分析列表
export const getSlowRankList = async(params: GetSlowRankListParams): Promise<GetSlowRankListResult> => {
    return await window.requestDataDebounced('communication/duration/slow-rank/list', withClusterPath(params));
};

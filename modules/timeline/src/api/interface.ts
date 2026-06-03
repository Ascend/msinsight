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

// common
export interface PaginationParams {
    pageSize: number;
    current: number;
}

export interface PaginationModel extends PaginationParams {
    count: number;
}

export interface RankInfo {
    clusterId: string;
    host: string;
    rankName: string;
    rankId: string;
    deviceId: string;
}

export interface CardRankInfo {
    rankInfo: RankInfo;
    dbPath: string;
    index?: number;
    isFtrace?: boolean;
}

// 算子详情信息
export interface OpDetail {
    id?: string;
    cardId: string;
    dbPath?: string;
    tid: string;
    pid: string;
    depth: number;
    duration: number;
    name: string;
    timestamp: number;
    metaType?: string;
}

export interface UnitDetail {
    cardId: string;
    propsCardId: string;
    pid: string;
}

// parseCards
export interface ParseCardsParam {
    cards: string[];
    dbPaths: string[];
}

// getOverallMetrics
export interface GetOverallMetricsParams extends PaginationParams {
    rankId: string;
    dbPath: string;
    startTime: number;
    endTime: number;
}

export interface GetOverallMetricsResultItem {
    id: string;
    level: number;
    name: string;
    totalTime: number;
    ratio: number;
    nums: number;
    avg: number;
    max: number;
    min: number;
    categoryList: string[];
    children?: GetOverallMetricsResultItem[] | null;
}

export interface GetOverallMetricsResult extends PaginationModel {
    data: GetOverallMetricsResultItem[];
}

// getOverallMetricsMoreList
export interface GetOverallMetricsMoreListParams extends PaginationParams {
    rankId: string;
    dbPath: string;
    categoryList: string[];
    orderBy?: keyof GetOverallMetricsMoreListResultItem;
    order?: 'ascend' | 'descend' | null;
    name?: string;
    startTime: number;
    endTime: number;
}

// 综合指标详细算子列表项
export interface GetOverallMetricsMoreListResultItem {
    id: string;
    opId?: number; // 算子id，用于算子跳转获取后获取算子详情信息
    timestamp: number;
    duration: number;
    name: string;
}

// 内存拷贝详细算子列表项
export interface GetMemcpyOverallMetricsMoreListResultItem extends Omit<GetOverallMetricsMoreListResultItem, 'opId'> {
    size: number; // 内存拷贝大小
}

export interface GetOverallMetricsMoreListResult<T> extends PaginationModel {
    sameOperatorsDetails: T[];
    rankId: number;
    count: number;
    pageSize: number;
    currentPage: number;
}

// getMemcpyOverallMetrics
export interface GetMemcpyOverallResult extends PaginationModel {
    data: GetMemcpyOverallResultItem[];
}

export interface GetMemcpyOverallResultItem {
    key: string;
    name: string;
    level: number;
    categoryList: string[]; // [tid, memcpyType]
    rowKey: string; // 唯一的行标识，方便前端使用
    children: GetMemcpyOverallResultItem[] | null;
}

export interface SetCardAliasParams {
    rankId: string;
    dbPath: string;
    cardAlias: string;
}

export interface GetAICoreParams {
    rankId: string;
    dbPath: string;
}

export interface GetAICoreParamsResult {
    hasProblem: boolean;
    percent: number;
}

export interface QueryOperatorDispatchParams extends PaginationParams {
    rankId: string;
    dbPath: string;
    orderBy: string;
    order: string;
    startTime: number;
    endTime: number;
};

export interface BaseSummaryRowItemType {
    id: string;
    rankId: string;
    dbPath: string;
    startTime: number;
    duration: number;
    pid: string;
    tid: string;
    depth: number;
    name?: string;
    originOptimizer?: string;
}

interface OperatorDispatchResultItem extends BaseSummaryRowItemType {
    note: string;
}

export interface OperatorDispatchResult extends OperatorDispatchResultItem {
    count: number;
};

export const queryFtraceStat = async (param: {
    rankId: string; dbPath: string; dataType: number; current: number; pageSize: number;
}): Promise<{ headers: string[]; data: Array<Record<string, number>>; pageParam: { current: number; pageSize: number; total: number } }> => {
    return window.requestData('systemView/systemViewFtraceStat', param, 'timeline');
};

export interface EventViewParams {
    currentPage: number;
    pageSize: number;
    rankId: string;
    dbPath: string;
    processName: string;
    orderBy: string;
    metaType: string;
    pid: string;
    tid: string;
    threadIdList?: string[];
    threadName: string;
    order: string;
    filterCondition: string[];
    startTime: number;
    endTime: number;
}

export interface QueryAllSameOperatorsDurationParams {
    rankId: string;
    dbPath: string;
    tid: string[];
    pid: string;
    startTime: number;
    endTime: number;
    name: string;
    wallDuration: number;
    metaTypeList: string[];
    count: number;
    field: string;
    order: string;
    current: number;
    pageSize: number;
    total: number;
    orderBy: string;
}

export interface OpData {
    startTime: string;
    timestamp: number;
    duration: number;
    id: string;
    depth: number;
    tid: string;
    pid: string;
    metaType?: string;
}

export interface QueryAllSameOperatorsDurationResult {
    currentPage: number;
    pageSize: number;
    sameOperatorsDetails: OpData[];
}

export interface QueryCommunicationKernelDetailParams {
    rankId?: string;
    dbPath?: string;
    name: string;
}

export interface CreateCurveParams {
    fileId: string;
    pid: string;
    tid: string;
    x: string;
    y?: string[];
    type?: string;
}

export interface QueryCommunicationKernelDetailResult {
    step: string;
    group: string;
}

export interface CreateCurveResult {
    curveName: string;
}

export interface GetUnitFlowsParams {
    dbPath?: string;
    rankId: string;
    tid: string;
    pid: string;
    startTime: number;
    endTime: number;
    id: string;
    metaType?: string;
    isSimulation: boolean;
}

interface FlowItemInfoPoint {
    depth: number;
    duration: number;
    id: string;
    metaType: string;
    name: string;
    pid: string;
    rankId: string;
    tid: string;
    timestamp: number;
}

interface FlowItemInfo {
    cat: string;
    id: string;
    title: string;
    from: FlowItemInfoPoint;
    to: FlowItemInfoPoint;
}

export interface FlowItem {
    cat: string;
    flows: FlowItemInfo[];
}

export interface GetUnitFlowsResult {
    unitAllFlows: FlowItem[];
}

export type TimelineAlignmentType = 'LEFT' | 'RIGHT';

export interface QueryTimelineOffsetParams {
    sliceName: string;
    rankId: string;
    fileId: string;
    pid: string;
    metaType: string;
    startTime: string;
    duration: number;
    alignType: TimelineAlignmentType;
}

export interface TimelineOffsetItem {
    rankId: string;
    offset: number;
    processId: string[];
}

export interface QueryTimelineOffsetResult {
    result: TimelineOffsetItem[];
    baseOffset: number;
}

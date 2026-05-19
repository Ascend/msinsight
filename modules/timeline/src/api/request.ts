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
    GetAICoreParams,
    GetAICoreParamsResult,
    GetOverallMetricsMoreListParams,
    GetOverallMetricsMoreListResult,
    GetOverallMetricsParams,
    GetOverallMetricsResult,
    GetOverallMetricsResultItem,
    GetMemcpyOverallResult,
    GetMemcpyOverallResultItem,
    ParseCardsParam,
    SetCardAliasParams,
    QueryOperatorDispatchParams,
    OperatorDispatchResult,
    QueryAllSameOperatorsDurationResult,
    QueryAllSameOperatorsDurationParams,
    QueryCommunicationKernelDetailParams,
    QueryCommunicationKernelDetailResult,
    CreateCurveParams, CreateCurveResult, GetUnitFlowsParams, GetUnitFlowsResult,
    GetOverallMetricsMoreListResultItem,
    GetMemcpyOverallMetricsMoreListResultItem,
    QueryTimelineOffsetParams,
    QueryTimelineOffsetResult,
} from './interface';
import connector from '../connection';

// 根据cardId集合解析timeline
export const parseCards = async(param: ParseCardsParam): Promise<any> => {
    return window.requestData('parse/cards', param, 'timeline');
};

// 获取system view综合指标列表
export const getOverallMetrics = async (params: GetOverallMetricsParams): Promise<GetOverallMetricsResult> => {
    const res = await window.requestData('systemView/overall', params, 'timeline', { silent: true });
    // 递归设置 categoryList 字段
    const maxDepth = 10;
    const addCatField = (data: GetOverallMetricsResultItem[], parentCat: string[] = [], depth = 1): void => {
        if (depth > maxDepth) {
            return;
        }
        data?.forEach(item => {
            item.categoryList = [...parentCat, item.name];

            if (item.children && item.children.length > 0) {
                addCatField(item.children, item.categoryList, depth + 1);
            }
        });
    };

    if (res.data.length !== 0) {
        addCatField(res.data);
        return res;
    }
    return new Promise((resolve) => {
        connector.addListener('OverallMetrics', async (e) => {
            if (e?.data?.body?.data?.dbId !== params.dbPath) {
                return;
            }
            const result = await window.requestData('systemView/overall', params, 'timeline', { silent: true });
            addCatField(result.data);
            resolve(result);
        });
    });
};

// 获取system view综合指标详细算子列表
export const getOverallMetricsMoreList = async (params: GetOverallMetricsMoreListParams): Promise<
GetOverallMetricsMoreListResult<GetOverallMetricsMoreListResultItem>> => {
    return window.requestData('systemView/overall/more/details', params, 'timeline');
};

// 获取system view内存拷贝综合指标列表
export const getMemcpyOverallMetrics = async (params: GetOverallMetricsParams): Promise<GetMemcpyOverallResult> => {
    const res = await window.requestData('memcpy/total/list', params, 'timeline', { silent: true });
    // 递归设置 categoryList 字段
    const maxDepth = 10;
    const addCatField = (data: GetMemcpyOverallResultItem[], parentCat: string[] = [], depth = 1): void => {
        if (depth > maxDepth) {
            return;
        }
        data?.forEach(item => {
            item.categoryList = [...parentCat, item.key];
            item.rowKey = item.categoryList.join('-');

            if (item.children && item.children.length > 0) {
                addCatField(item.children, item.categoryList, depth + 1);
            }
        });
    };

    if (res.data.length !== 0) {
        addCatField(res.data);
    }
    return res;
};

// 获取system view内存拷贝综合指标详细算子列表
export const getMemcpyOverallMetricsMoreList = async (params: GetOverallMetricsMoreListParams): Promise<
GetOverallMetricsMoreListResult<GetMemcpyOverallMetricsMoreListResultItem>> => {
    return window.requestData('memcpy/detail/list', params, 'timeline');
};

// 设置卡级别别名
export const setCardAliasReq = async (params: SetCardAliasParams): Promise<any> => {
    return window.requestData('unit/setCardAlias', params, 'timeline');
};

// 获取ExpertSummary中的AI Core降频分析
export const queryExpertAnalysis = async (params: GetAICoreParams): Promise<GetAICoreParamsResult> => {
    return window.requestData('expertAnalysis/AICoreFreq', params, 'timeline');
};

// 获取算子下发路径问题列表
export const queryOperatorDispatch = async (params: QueryOperatorDispatchParams): Promise<OperatorDispatchResult> => {
    return window.requestData('advisor/operatorDispatch', params, 'advisor');
};

export async function queryAllSameOperatorsDuration(params: QueryAllSameOperatorsDurationParams): Promise<QueryAllSameOperatorsDurationResult> {
    return window.requestData('query/all/same/operators/duration', params, 'timeline');
}

export async function queryCommunicationKernelDetail(params: QueryCommunicationKernelDetailParams): Promise<QueryCommunicationKernelDetailResult> {
    return window.requestData('unit/kernelDetail', params, 'timeline');
}

export async function createCurve(params: CreateCurveParams): Promise<CreateCurveResult> {
    return window.requestData('create/curve', params, 'timeline');
}

export async function getUnitFlows(params: GetUnitFlowsParams): Promise<GetUnitFlowsResult> {
    return window.requestData('unit/flows', params, 'timeline');
}

export async function queryTimelineOffset(params: QueryTimelineOffsetParams): Promise<QueryTimelineOffsetResult> {
    return window.requestData('timeline/rankOffset', params, 'timeline');
}

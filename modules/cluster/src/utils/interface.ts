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

import { AnalysisType } from '../components/communication/Filter';

export interface ErrorInfo {
    code: number;
    message?: string;
}

export interface StringMap {
    [prop: string]: string;
}

export interface VoidFunction {
    (...rest: any[]): void;
}

export interface optionDataType {
    key?: string;
    label: React.ReactNode;
    value: number | string ;
}

export interface optionMapDataType {
    [props: string]: optionDataType[];
}

export interface FormatterParams {
    marker: string;
    name: string;
    value: any;
    data: any ;
    seriesName: string;
    seriesType: string;
}

export interface GetParallelStrategyResCommon {
    algorithm: 'megatron-lm(tp-cp-ep-dp-pp)' | 'megatron-lm(tp-cp-pp-dp-ep)' | 'mindspeed(tp-cp-ep-dp-pp)' | 'mindie-llm(tp-dp-ep-pp-moetp)' | 'vllm(tp-pp-dp-ep)';
    dpSize: number;
    ppSize: number;
    tpSize: number;
    epSize: number;
    cpSize: number;
    moeTpSize: number | null;
}

export interface GetParallelStrategyRes extends GetParallelStrategyResCommon {
    level: string;
}

export interface GetSlowRankAdvise extends GetParallelStrategyResCommon {
    dimension: 'ep-dp' | 'ep-dp-pp' | 'ep-dp-pp-cp' | 'ep-dp-pp-cp-tp';
}

export type SetParallelStrategyParams = Omit<GetParallelStrategyRes, 'level'>;

export type ParallelismType = 'ep' | 'dp' | 'cp' | 'pp' | 'tp' | 'moeTp';
export type ConnectionType = 'exp' | 'dp' | 'cp' | 'pp' | 'tp' | 'moeTp';
export interface ParallelismArrangementParams extends SetParallelStrategyParams {
    dimension: 'ep-dp' | 'ep-dp-pp' | 'ep-dp-pp-cp' | 'ep-dp-pp-cp-tp';
}

export interface IndicatorsItem {
    name: string;
    key: string;
    renderHeatMap: boolean;
    renderChart: boolean;
    chart: 'bar' | 'line';
    stack: string;
    yAxisType: 'time' | 'ratio';
    visible: boolean;
    unit: 'μs' | '%';
}

export interface ConnectionsItem {
    type: ConnectionType;
    list: number[];
    group: string[];
}

export interface ArrangementItem {
    index: number;
    name: string;
    position: { x: number; y: number };
    indicator: Record<string, number>;
    formattedRanks: string;
    attribute: {
        [K in ParallelismType as `${K}Index`]: number;
    };
}

export interface ParallelismArrangementResult {
    size: number;
    arrangements: ArrangementItem[];
    indicators: IndicatorsItem[];
    connections: ConnectionsItem[];
    domains: string[];
    rankDbPathList: Array<{ rankId: string; dbPath: string }>;
}

export interface GetParallelismPerformanceData extends ParallelismArrangementParams {
    indexList?: number[];
    orderBy?: string;
    step?: string;
    isCompare: boolean;
}

export interface PerformanceDataItem {
    [key: string]: any;
    index: number;
}

export interface TopElements {
    name: string;
    index: number;
    dpSynchronizeTime: number;
    cpSynchronizeTime?: number;
    tpSynchronizeTime?: number;
}

export interface AntdTableRow {
    [header: string]: string | number;
    key: string;
}

export interface GetParallelismPerformanceRes {
    performance: PerformanceDataItem[];
    advice: string[];
}

export interface GetSlowRankAdviseRes {
    hasSlowRank: boolean;
    matchSuccess: boolean;
    topNElements: TopElements[];
}

export interface ClickOperatorItem {
    name: string;
    rankId: number;
    timestamp: number;
    duration: number;
}

export interface ChartZoomData {
    start: number;
    end: number;
}

export interface CompareData<T> {
    compare: T;
    baseline: T;
    diff: T;
}

export interface ImportExpertDataParams {
    filePath: string;
    version: 'unbalanced' | 'balanced' | 'profiling';
}

export interface QueryExpertHotspotParams {
    layerNum: number;
    expertNum: number;
    modelStage: 'prefill' | 'decode';
    version: 'unbalanced' | 'balanced' | 'profiling';
    denseLayerList: number[];
}

export interface QueryExpertHotspotItem {
    modelStage: 'prefill' | 'decode';
    rankId: number;
    visits: number;
    layer: number;
    expertId: number;
    expertIndex: number;
    version: 1 | 2; // 1: 均衡前  2: 均衡后
}

export interface QueryExpertHotspotResult {
    hotspotInfos: QueryExpertHotspotItem[];
}

export interface QueryModelInfoResult {
    layerNum: number;
    expertNum: number;
    denseLayerList: number[];
}

export interface WrapBandwidthDataParams {
    domId: string;
    iterationId: string;
    rankId: number;
    dbPath: string;
    operatorName: string;
    stage: string;
    isDark: boolean;
    pgName: string;
    groupIdHash: string;
}

export interface PacketAndBandwidthChartsParams extends WrapBandwidthDataParams {
    locale: string;
}

export interface GetSlowRankListParams {
    iterationId: string;
    stage: string;
    type: AnalysisType;
    operatorName: string;
    pgName?: string;
    baselineIterationId: string;
    isCompare: boolean;
}

export interface SlowRankOpListItem {
    name: string;
    startTime: number;
    elapseTime: number;
    diffTime: number;
    maxTime: number;
    maxStartTime: number;
}

export interface SlowRankListItem {
    rankId: string;
    totalElapseTime: number;
    totalDiffTime: number;
    opList: SlowRankOpListItem[];
}

export interface GetSlowRankListResult {
    hasAdvice: boolean;
    fastTotalElapseTime: number;
    fastRankId: string;
    data: SlowRankListItem[];
}

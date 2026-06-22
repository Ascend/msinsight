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
import type { IOriginData as IRooflineData } from './detail/Roofline/Index';
import type { QueryCacheRecordReturn } from '@/components/cacheKit/defs';
/**
 * 查询源代码
 *
 * @param {sourceName} rankId RankID
 * @return {fileContent:string}
 */
export const querySourceCode = async(sourceName: string): Promise<any> => {
    return window.requestData('source/code/file', { sourceName, retry: true });
};

/**
 * 查询代码行执行信息
 *
 * @param {sourceName} 源文件名
 * @param {coreName} 内核
 * @return {lines:[]}
 */
export const queryApiLine = async(param: {sourceName: string; coreName: string}): Promise<any> => {
    return window.requestData('source/api/line', param);
};

/**
 * 查询动态代码行属性信息
 *
 * @param {sourceName} 源文件名
 * @param {coreName} 内核
 * @return {lines:[]}
 */
export const queryDynamicLine = async(param: {sourceName: string; coreName: string}): Promise<any> => {
    return window.requestData('source/api/line/dynamic', { ...param, retry: true });
};

/**
 * 查询代码行执行信息
 *
 * @return {instructions:string}
 */
export const queryApiInstr = async(): Promise<any> => {
    return window.requestData('source/api/instructions', {});
};

/**
 * 查询动态指令表
 *
 * @param {coreName} 内核
 * @return {instructions:string}
 */
export const queryDynamicInstr = async(param: {coreName: string}): Promise<any> => {
    return window.requestData('source/api/instructions/dynamic', { ...param, retry: true });
};

/**
 * 查询算子基本信息
 *
 * @return {[x:string]:string|number}
 */
export const queryBaseInfo = async(param: { isCompared: boolean }): Promise<any> => {
    return window.requestData('source/details/baseInfo', param);
};

/**
 * 查询算子计算负载数据
 *
 * @return {[x:string]:string|number}
 */
export const queryComputeWorkload = async(param: { isCompared: boolean }): Promise<any> => {
    return window.requestData('source/details/computeworkload', param);
};

/**
 * 查询内存负载表
 *
 * @param {blockId}
 * @return {
 * memoryTable:[{
 *     blockId:string;
 *     tableDetail:[{
 *       headerName:string[];
 *       row:{
 *         name:string;
 *         value:string[];
 *       }
 *     }]
 *   }]
 * }
 */
export const queryMemoryTable = async(param: { blockId: string; isCompared: boolean }): Promise<any> => {
    return window.requestData('source/details/memoryTable', param);
};

/**
 * 查询内存流量图
 *
 * @param {blockId}
 * @return {
 * coreMemory:[{
 *     blockId:string;
 *     blockType:string;
 *     chipType:string;
 *     memoryUnit:[{
 *       memoryPath:string;
 *       request:string;
 *       requestPerByte:string;
 *     }];
 *     L2catch:{
 *         hit:string;
 *         miss:string,
 *         totalRequest:string;
 *         hitRatio:string;
 *     }
 *   }]
 * }
 */
export const queryMemoryGraph = async(param: { blockId: string; isCompared: boolean }): Promise<any> => {
    return window.requestData('source/details/memoryGraph', param);
};

/**
 * 查询内存流量图
 *
 * @param {}
 * @return {
 * {
 *     soc:string; // 算子运行平台
 *     opType:string; // 算子类型：vector, cube, mix
 *     advice:string; // 专家建议
 *     opDetails:[{
 *       coreId:number;  // core
 *       subCoreDetails:[{
 *         subCoreName:string; // sub core名字+序号：cubeX，vectoreX
 *         "cycles": {
 *           "value": number, // 时钟周期
 *           "level": number // 颜色的级别，0~10,0代表没有数据或者数据为0，显示为灰色；1~10代表十种颜色区间
 *         },
 *         "throughput": {
 *           "value": number, // 核吞吐数据 单位：（GB/s）
 *           "level": number
 *         },
 *         "cacheHitRate": {
 *           "value": number, //  L2cache命中率 单位：(%)
 *           "level": number
 *         }
 *       }]
 *     }]
 * }
 */
export const queryCoreOccupancy = async(isCompared: boolean): Promise<any> => {
    return window.requestData('source/details/interCoreLoadAnalysis', { isCompared });
};

export const queryRoofline = async(): Promise<IRooflineData | undefined> => {
    return window.requestData('source/details/roofline', {});
};

/**
 * 查询内存流量图
 * @return {cacheRecords:[]}
 */
export const queryCacheRecord = async(): Promise<QueryCacheRecordReturn> => {
    return window.requestData('source/cache/cachelineRecords', {}) as Promise<QueryCacheRecordReturn>;
};

export const queryTopWarpStallReason = async(param: { isCompared: boolean }): Promise<any> => {
    return window.requestData('source/queryTopWarpStallReason', param);
};

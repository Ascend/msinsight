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
import { makeAutoObservable } from 'mobx';
import {
    Communicator,
    communicatorContainerData,
    RankDyeingData,
} from '../components/communicatorContainer/ContainerUtils';
import type { ChartZoomData, ClickOperatorItem, IndicatorsItem, PerformanceDataItem } from '../utils/interface';

export type PerformanceDataMap = Map<number, PerformanceDataItem>;

export interface ClusterInfo {
    name: string;
    path: string;
    dbPath?: string;
    parsed: boolean;
    durationParsed: boolean;
}

export class Session {
    language: 'zhCN' | 'enUS' = 'enUS';
    parseCompleted: boolean = false;
    unitcount: number = 0;
    renderId: number = 1;
    isFullDb: boolean = false;
    id = '';

    selectedProjectName: string = '';
    selectedClusterPath: string = '';
    allRankIds: any[] = [];
    communicatorData: communicatorContainerData = {
        clusterPath: '',
        partitionModes: [],
        defaultPPSize: 0,
    };

    activeCommunicator: Communicator | undefined;
    rankCount: number = 0;
    arrangementRankCount: number = 0;
    indicatorList: IndicatorsItem[] = []; // 性能指标项列表
    dynamicsIndicatorList: IndicatorsItem[] = []; // 动态性能指标列表
    performanceData: PerformanceDataItem[] = []; // 性能数据
    performanceDataMap: PerformanceDataMap = new Map(); // 性能数据map
    rankDyeingData: RankDyeingData = {}; // 着色图例数据
    rankDbPathMap: Map<string, string> = new Map(); // rank: `{clusterId} {host} {rankId} {deviceId}` -> dbPath: string
    communicationDomains: string[] = []; // 通信域（包含所有连线、框）
    ppCommunicationDomains: string[] = []; // PP 通信域（仅包含所有 PP 连线）
    stepList: string[] = [];
    baselineStepList: string[] = [];
    // 集群对比
    isCompare: boolean = false;
    // 右键选中的算子
    targetOperator: ClickOperatorItem | undefined = undefined;
    communicationChartZoomData?: ChartZoomData; // start 图表缩放的起始位置百分比, end 图表缩放的结束位置百分比
    profilingExpertDataParsed: boolean | null = null; // 专家热力图数据解析结果，一次项目导入只需更新一次
    private _clusterList: ClusterInfo[] = [];

    constructor(conf?: Partial<Session>) {
        makeAutoObservable(this);

        window.closeWaiting = (): void => {
            this.parseCompleted = true;
        };
    }

    // Data Type 指标选项（着色指标）
    get dataTypeOptions(): IndicatorsItem[] {
        return this.indicatorList.filter(item => item.renderHeatMap);
    }

    // Order By 指标选项（性能图表指标）
    get performanceChartsIndicators(): IndicatorsItem[] {
        return this.indicatorList.filter(item => item.renderChart);
    }

    get indicatorMap(): Map<string, IndicatorsItem> {
        const map: Map<string, IndicatorsItem> = new Map();
        this.indicatorList.forEach(item => {
            map.set(item.key, item);
        });
        return map;
    }

    get dynamicsIndicatorMap(): Map<string, IndicatorsItem> {
        const map: Map<string, IndicatorsItem> = new Map();
        this.dynamicsIndicatorList.forEach(item => {
            map.set(item.key, item);
        });
        return map;
    }

    get clusterCompleted(): boolean {
        return this._clusterList.find(({ path }) => path === this.selectedClusterPath)?.parsed ?? false;
    }

    get durationFileCompleted(): boolean {
        return this._clusterList.find(({ path }) => path === this.selectedClusterPath)?.durationParsed ?? false;
    }

    // 当前 selectedClusterPath 是否在 clusterList 中（即当前数据是否有 cluster 维度）
    get isClusterPathInList(): boolean {
        return this._clusterList.some(({ path }) => path === this.selectedClusterPath);
    }

    get clusterList(): ClusterInfo[] {
        return this._clusterList;
    }

    set clusterList(value) {
        this._clusterList = value;
    }

    // 设置着色数据图例
    setRankDyeingData(): void {
        const data: Record<string, { min: number; max: number }> = {};
        this.dataTypeOptions.forEach(dataType => {
            data[dataType.key] = { min: Number.MAX_SAFE_INTEGER, max: 0 };
        });
        this.performanceData.forEach(item => {
            this.dataTypeOptions.forEach(dataType => {
                const key = dataType.key;
                data[key].max = Math.max(data[key].max, item[key]);
                data[key].min = Math.min(data[key].min, item[key]);
            });
            Object.keys(item.commCompare).forEach(key => {
                if (!(key in data)) {
                    data[key] = { min: Number.MAX_SAFE_INTEGER, max: 0 };
                }
                data[key].max = Math.max(data[key].max, item.commCompare[key]);
                data[key].min = Math.min(data[key].min, item.commCompare[key]);
            });
        });

        this.rankDyeingData = data;
    }

    resetForProjectChange(): void {
        this.selectedProjectName = '';
        this.selectedClusterPath = '';
        this._clusterList = [];
        this.parseCompleted = false;
        this.unitcount = 0;
        this.communicatorData = { clusterPath: '', partitionModes: [], defaultPPSize: 0 };
        this.profilingExpertDataParsed = null;
        this.resetForClusterChange();
    }

    resetForClusterChange(): void {
        this.allRankIds = [];
        this.activeCommunicator = undefined;
        this.indicatorList = [];
        this.performanceData = [];
        this.performanceDataMap = new Map();
        this.communicationDomains = [];
        this.ppCommunicationDomains = [];
        this.stepList = [];
        this.rankDyeingData = {};
        this.arrangementRankCount = 0;
        this.targetOperator = undefined;
    }
}

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
import { observer } from 'mobx-react-lite';
import React, { useEffect, useRef, useState } from 'react';
import ResizeObserver from 'resize-observer-polyfill';
import { getInstanceByDom } from 'echarts';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { COLOR, getDecimalCount, getCompareName, getBaselineName } from '../Common';
import type { ConditionDataType } from './Filter';
import type { CompareData, VoidFunction } from '../../utils/interface';
import { queryCommunicationMatrix } from '../../utils/RequestUtils';
import { cloneDeep } from 'lodash';
import { type Session } from '../../entity/session';
import { CollapsiblePanel } from '@insight/lib/components';
import { safeStr, disposeAdaptiveEchart, getAdaptiveEchart, getDefaultChartOptions } from '@insight/lib/utils';
import type { TooltipComponentOption, VisualMapComponentOption } from 'echarts/components';
import type { PiecewiseVisualMapOption } from 'echarts/types/dist/shared';
import type { Condition, MatrixTypeValues, Range } from './CommunicationMatrix/Filter';
import Filter, { MatrixType } from './CommunicationMatrix/Filter';

import {
    getTransportTypeSerie,
    getTransportTypeVisualMap,
} from './CommunicationMatrix/transportType';

interface DataSource {
    data: MatrixItem[];
    rankIds: number[];
}
interface MatrixItem {
    srcRank: number;
    dstRank: number;
    matrixData: CompareData<MatrixData>;
}
interface MatrixData {
    opName: string;
    bandwidth: number;
    transitSize: number;
    transitTime: number;
    transportType: string;
}

interface MergedMatrixData {
    opName: string;
    bandwidth: number[];
    transitSize: number[];
    transitTime: number[];
    transportType: string[];
}

interface ChartData {
    data: HeatmapData[];
    rankIds: number[];
    type: MatrixType;
    min: number;
    max: number;
    isCompare: boolean;
}
export type HeatmapData = [string, string, React.Key, CompareData<MergedMatrixData>];
export enum HeatmapDataIndex {
    SRC_RANK = 0,
    DST_RANK = 1,
    VALUE = 2,
    DATA = 3,
}

const matrixDataTypeUnits = {
    bandwidth: '(GB/s)',
    transitSize: '(MB)',
    transitTime: '(ms)',
};

function InitChart(data: ChartData, t: TFunction): void {
    const chartDom = document.getElementById('matrixchart');
    if (chartDom !== null && chartDom.clientWidth > 0 && chartDom.clientHeight > 0) {
        disposeAdaptiveEchart(chartDom);
        const myChart = getAdaptiveEchart(chartDom);
        myChart.setOption(wrapData(data, t), { replaceMerge: ['series', 'xAxis', 'yAxis'] });
        myChart.on('dataZoom', (): void => {
            const option = myChart.getOption();
            const xAxisData = (option as any).xAxis[0].data;
            const dataZoom = (option as any).dataZoom[0];

            // 当前显示的区间
            const start = dataZoom.start ?? 0;
            const end = dataZoom.end ?? 100;

            const total = xAxisData.length;
            const visibleCount = Math.round((end - start) / 100 * total);

            const showLabel = visibleCount <= 16;
            myChart.setOption({
                series: [{
                    label: {
                        show: showLabel,
                    },
                }],
            }, false);
        });
    }
}

function wrapData(dataSource: ChartData, t: TFunction): any {
    const { data, rankIds, type, min, max, isCompare } = dataSource;
    const option: any = cloneDeep(baseOption);
    option.xAxis.data = rankIds;
    option.yAxis.data = rankIds;
    option.series = [getSeries({ data, rankIds, t, type, isCompare })];
    option.visualMap = getVisualMap({ type, min, max, dataLength: data.length, isCompare, t });
    return option;
}

const baseOption: any = {
    xAxis: {
        type: 'category',
        name: 'Src Rank Id',
        splitArea: {
            show: true,
        },
    },
    yAxis: {
        type: 'category',
        name: 'Dst Rank Id',
    },
    tooltip: { show: true },
    textStyle: getDefaultChartOptions().textStyle,
    dataZoom: [
        {
            type: 'inside',
            xAxisIndex: [0],
            start: 0,
            end: 100,
        },
        {
            type: 'inside',
            yAxisIndex: [0],
            start: 0,
            end: 100,
        },
        {
            type: 'inside',
            start: 0,
            end: 100,
        },
    ],
    grid: {
        left: '100',
        right: '100',
        height: '80%',
        top: '10%',
    },
};

export const baseSeries = {
    type: 'heatmap',
    emphasis: {
        itemStyle: {
            shadowBlur: 10,
            shadowColor: COLOR.GREY_50,
        },
    },
};
function getSeries({ data, rankIds, type, isCompare, t }: {
    data: HeatmapData[];
    rankIds: number[];
    type: MatrixType;
    isCompare: boolean;
    t: TFunction;
}): any {
    if (type === MatrixType.TRANSPORT_TYPE) {
        return getTransportTypeSerie({ data, rankIds, type, isCompare, t });
    }

    return {
        ...baseSeries,
        data,
        label: {
            show: rankIds.length <= 16,
            formatter: function (params: any): string {
                const dataList = isCompare ? params?.data[3].diff : params?.data[3].compare;
                if (!dataList) {
                    return '';
                }
                return dataList[type].length > 1 ? `[${dataList[type].join(',')}]` : dataList[type][0];
            },
        },
        tooltip: getTooltip({ t, type, isCompare }),
    };
}

interface Label {
    label: string | number;
    content: string | number;
    contentClass?: string;
    key?: MatrixType;
}
// 提示框
export function getTooltip({ t, type, isCompare }: {t: TFunction;type: MatrixTypeValues;isCompare: boolean}):
TooltipComponentOption {
    return {
        show: true,
        formatter: function (params: any): string {
            const list = getDisplayList({ t, type, isCompare, data: params.data });
            return list.map(labelItem => {
                const unit = matrixDataTypeUnits[labelItem.key as keyof typeof matrixDataTypeUnits];
                return `<span>
                    ${safeStr(labelItem.label)}:
                 </span>
                 <span class="tooltip-value ${labelItem.contentClass ?? ''}">
                    ${safeStr(labelItem.content)}
                    <span style="font-weight: normal;color: var(--mi-text-color-tertiary)">${unit ?? ''}</span>
                 </span>
                 <br/>`;
            }).join('');
        },
    };
}

function getDisplayList({ t, type, isCompare, data }:
{data: HeatmapData;t: TFunction;type: MatrixTypeValues;isCompare: boolean}): Label[] {
    const [srcRank, dstRank, value, { compare, baseline }] = data;
    const list: Label[] = [
        { label: 'Src Rank -> Dst Rank', content: `${srcRank} -> ${dstRank}` },
    ];

    if (isCompare) {
        // 算子名
        if (compare.opName !== '') {
            list.push({ label: t(getCompareName('operatorName')), content: compare.opName });
        }
        if (baseline.opName !== '') {
            list.push({ label: t(getBaselineName('operatorName')), content: baseline.opName });
        }
        if (type !== MatrixType.TRANSPORT_TYPE) {
            list.push({ label: t('Difference'), content: value, key: type, contentClass: typeof value === 'number' && value >= 0 ? 'positive-number' : 'negative-number' });
        }
        list.push(
            { label: t(getCompareName(type)), content: compare[type][0], key: type },
            { label: t(getBaselineName(type)), content: baseline[type][0], key: type },
        );
    } else {
        // 算子名
        if (compare.opName !== '') {
            list.push({ label: t('operatorName'), content: compare.opName });
        }
        const allTypeList = Object.values(MatrixType).map((itemType) => {
            let content;
            if (compare[itemType].length > 1) {
                content = `[${compare[itemType].join(', ')}]`;
            } else {
                content = compare[itemType][0];
            }

            return { label: t(itemType), content, key: itemType };
        });
        list.push(...allTypeList);
    }

    return list;
}

// 图例（颜色条）
const baseVisualMap: PiecewiseVisualMapOption = {
    orient: 'horizontal',
    left: 'center',
    bottom: '0',
    textStyle: { color: COLOR.GREY_40 },
    dimension: 2,
};
function getVisualMap({ dataLength, min, max, type, isCompare = false, t }: {
    dataLength: number;min: number;max: number;isCompare?: boolean;type: MatrixType; t: TFunction;
}): VisualMapComponentOption {
    if (type === MatrixType.TRANSPORT_TYPE) {
        return getTransportTypeVisualMap(isCompare, t);
    }
    if (dataLength > 0 || isFinite(max)) {
        let inRange = { color: [COLOR.BAND_0, COLOR.BAND_1, COLOR.BAND_2, COLOR.BAND_3] };

        if (type === MatrixType.TRANSIT_TIME) {
            inRange.color.reverse();
        }

        if (min === max) {
            inRange = { color: [COLOR.BAND_1] };
        }

        return {
            ...baseVisualMap,
            calculable: true,
            itemHeight: 300, // 调整宽度,
            inRange,
            min,
            max,
            precision: Math.max(getDecimalCount(min), getDecimalCount(max)),
        };
    }
    return baseVisualMap;
}

// 将 MatrixType 对应的 value 转为对象
function mapValuesToEnum(data: Record<string, any>): Omit<MergedMatrixData, 'opName'> {
    return Object.fromEntries(
        Object.values(MatrixType).map((key) => [key, [data[key]]]),
    ) as Omit<MergedMatrixData, 'opName'>;
}

interface UpdateChartParams {
    dataSource: DataSource;
    switchCondition: Condition;
    range?: Range;
    shouldUpdateRange: boolean;
    setRange: (val: Range) => void;
    t: TFunction;
    isCompare: boolean;
}
// 图表更新
const updateChart = ({ dataSource, switchCondition, range, shouldUpdateRange, setRange, t, isCompare }: UpdateChartParams): void => {
    const { data, rankIds } = dataSource;
    const result = data.reduce((acc: Record<string, HeatmapData>, cur) => {
        const { srcRank, dstRank, matrixData: { compare, baseline, diff } } = cur;
        const key = `${srcRank}-${dstRank}`;
        const compareValue = compare[switchCondition.type];
        const diffValue = diff[switchCondition.type];
        const value = isCompare ? diffValue : compareValue;
        let match = rankIds.includes(srcRank) && rankIds.includes(dstRank);
        if (!switchCondition.showInner) {
            match = match && srcRank !== dstRank;
        }
        if (range) {
            match = match && typeof value === 'number' && value >= range.min && value <= range.max;
        }
        if (match) {
            // 数据去重、值合并：如果存在重复的 key 时，将重复数据合并为 acc[key][3] 中各项指标的值拼接为 [value1,value2,...]
            if (!acc[key]) {
                acc[key] = [String(srcRank), String(dstRank), value,
                    {
                        compare: {
                            opName: compare.opName,
                            ...mapValuesToEnum(compare),
                        },
                        baseline: {
                            opName: baseline.opName,
                            ...mapValuesToEnum(baseline),
                        },
                        diff: {
                            opName: diff.opName,
                            ...mapValuesToEnum(diff),
                        },
                    },
                ];
            } else {
                for (const item of Object.values(MatrixType)) {
                    (acc[key][3].compare[item] as Array<string | number>).unshift(compare[item]);
                }
            }
        }

        return acc;
    }, {});

    const dataList = Object.values(result);
    const values: number[] = dataList.map((item: HeatmapData) => typeof item[HeatmapDataIndex.VALUE] === 'number' ? item[HeatmapDataIndex.VALUE] as number : 0);
    const min = dataList.length > 0 ? Math.min(...values) : 0;
    const max = dataList.length > 0 ? Math.max(...values) : 0;
    if (shouldUpdateRange) {
        setRange({ min, max });
    }
    InitChart({ ...dataSource, data: dataList, type: switchCondition.type, min: range?.min ?? min, max: range?.max ?? max, isCompare }, t);
};

// 数据更新
const updateData = async(condition: ConditionDataType, setDataSource: VoidFunction, isCompare: boolean): Promise<void> => {
    const { iterationId, stage, operatorName, baselineIterationId, pgName, groupIdHash, baselineGroupIdHash } = condition;
    if (stage === '' || operatorName === '') {
        setDataSource({ data: [], rankIds: [] });
        return;
    }
    const param = { iterationId, pgName, stage, operatorName, isCompare, baselineIterationId, groupIdHash, baselineGroupIdHash };
    const res = await queryCommunicationMatrix(param);
    const data = res?.matrixList ?? [];
    // 从矩阵数据中获取要展示的rankId列表
    let rankIds: number[];
    if (stage === 'p2p') {
        rankIds = Array.from(
            new Set(
                data.flatMap(({ srcRank, dstRank }: {srcRank: number; dstRank: number}) => [srcRank, dstRank]),
            ),
        ).sort((a, b) => (a as number) - (b as number)) as number[];
    } else {
        rankIds = stage.replace(/[(),]/, '')
            .split(',').map(value => Number.parseInt(value))
            .filter(value => !Number.isNaN(value))
            .sort((a, b) => a - b);
    }
    setDataSource({ data, rankIds });
};

// 通信矩阵
const CommunicationMatrix = observer(({ isShow, conditions, session }: { isShow: boolean;conditions: ConditionDataType;session: Session}) => {
    const { t } = useTranslation('communication');
    const [switchCondition, setSwitchCondition] = useState<Condition>({ type: MatrixType.BANDWIDTH, showInner: false });
    const [range, setRange] = useState<Range>({ min: 0, max: 1 });
    const [dataSource, setDataSource] = useState<DataSource>({ data: [], rankIds: [] });
    const chartRef = useRef<HTMLDivElement>(null);
    const needsChartUpdate = useRef(true);
    const [hasChartSize, setHasChartSize] = useState(false);

    useEffect(() => {
        const chartDom = chartRef.current;
        if (!chartDom) {
            return undefined;
        }
        const resizeChart = (): void => {
            const hasSize = chartDom.clientWidth > 0 && chartDom.clientHeight > 0;
            setHasChartSize(hasSize);
            if (hasSize) {
                getInstanceByDom(chartDom)?.resize();
            }
        };
        const resizeObserver = new ResizeObserver(resizeChart);
        resizeObserver.observe(chartDom);
        resizeChart();
        return (): void => {
            resizeObserver.disconnect();
            disposeAdaptiveEchart(chartDom);
        };
    }, []);

    const handleFilterChange = (filed: string, val: string | boolean): void => {
        setSwitchCondition({ ...switchCondition, [filed]: val });
    };
    const handleRangeChange = (rangeVal: Range): void => {
        updateChart({ shouldUpdateRange: false, range: rangeVal, setRange, switchCondition, dataSource, t, isCompare: session.isCompare });
    };

    useEffect(() => {
        if (isShow) {
            if (session.clusterCompleted) {
                updateData(conditions, setDataSource, session.isCompare);
            } else {
                setDataSource({ data: [], rankIds: [] });
            }
        }
    }, [isShow, conditions, session.isCompare]);

    useEffect(() => {
        needsChartUpdate.current = true;
    }, [dataSource, switchCondition, t, session.isCompare]);

    useEffect(() => {
        const chartDom = chartRef.current;
        // Visibility changes only resize the chart; rebuild it when its data or filters change.
        if (hasChartSize && isShow && needsChartUpdate.current && chartDom && chartDom.clientWidth > 0 && chartDom.clientHeight > 0) {
            updateChart({ shouldUpdateRange: true, setRange, switchCondition, dataSource, t, isCompare: session.isCompare });
            needsChartUpdate.current = false;
        }
    }, [dataSource, switchCondition, t, session.isCompare, hasChartSize, isShow]);

    return <CollapsiblePanel style={{ display: isShow ? 'block' : 'none' }} title={t('sessionTitle.MatrixModel')} padding={'16px 24px'}>
        <Filter condition={switchCondition} handleChange={handleFilterChange} range={range} onRangeChange={handleRangeChange}/>
        <div ref={chartRef} id={'matrixchart'} style={{ width: 'calc(100vw - 80px)', height: '800px' }}></div>
    </CollapsiblePanel>;
});

export default CommunicationMatrix;

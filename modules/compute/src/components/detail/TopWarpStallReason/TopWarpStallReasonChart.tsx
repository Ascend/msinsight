/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React, { useEffect, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import * as echarts from 'echarts';
import { COLOR, getAdaptiveEchart, chartVisbilityListener, safeStr, sortFunc, chartColors, getDefaultChartOptions } from '@insight/lib/utils';
import { cloneDeep } from 'lodash';
import { CompareData } from '../../../utils/interface';
import { type IStallReasonData } from './Index';

interface Iprops {
    data: Array<CompareData<IStallReasonData>>;
    isCompared: boolean;
}

interface SeriesData {
    value: number;
    source?: string;
}

interface Series {
    name: string;
    type: string;
    data: SeriesData[];
    itemStyle: { borderColor: string };
    barMaxWidth: number;
}

const baseOption = {
    textStyle: getDefaultChartOptions().textStyle,
    color: chartColors,
    title: {
        text: 'Top Warp Stall Reason',
        textStyle: { color: COLOR.Grey50 },
        x: 'center',
    },
    tooltip: {
        trigger: 'axis',
        axisPointer: {
            type: 'shadow',
        },
        confine: true,
        formatter: function (params: any[]): string {
            let result: string = `${safeStr(params[0]?.name)}`;
            params.forEach(param => {
                const source = param.data?.source;
                if (source !== undefined) {
                    result += `<br/>${param?.marker} ${safeStr(source)}: ${param?.data?.value}`;
                } else {
                    result += `<br/>${param?.marker} ${safeStr(param?.name)}: ${param?.data?.value}`;
                }
            });
            return result;
        },
    },
    legend: {
        show: false,
    },
    grid: {
        left: '120',
        right: '4%',
        bottom: '5%',
        containLabel: false,
    },
    xAxis: {
        type: 'value',
        boundaryGap: [0, 0.01],
        axisLabel: {
            formatter: '{value}',
            color: COLOR.Grey40,
        },
    },
    yAxis: {
        type: 'category',
        axisLabel: {
            formatter: '{value}',
            color: COLOR.Grey40,
        },
        data: [] as string[],
    },
    series: [
        {
            name: 'stall',
            type: 'bar',
            data: [] as SeriesData[],
            itemStyle: {
                borderColor: 'white',
            },
            barMaxWidth: 30,
        },
    ],
};

const defaultSeries: Series = {
    name: 'stall',
    type: 'bar',
    data: [],
    itemStyle: {
        borderColor: 'white',
    },
    barMaxWidth: 30,
};

const chartID = 'TopWarpStallReason';

function InitCharts(data: Array<CompareData<IStallReasonData>>, title: string, isCompared: boolean): void {
    const chartDom = document.getElementById(chartID);
    if (chartDom === null || chartDom.offsetParent === null) {
        return;
    }
    const myChart: echarts.ECharts = getAdaptiveEchart(chartDom);
    myChart.setOption(wrapData(data, title, isCompared), { replaceMerge: ['series'] });
}

function wrapData(data: Array<CompareData<IStallReasonData>>, title: string, isCompared: boolean): any {
    const option = cloneDeep(baseOption);
    option.title.text = title;
    const sorted = [...data].sort((a, b) => sortFunc(String(a.compare.value), String(b.compare.value)));
    const namelist = sorted.map(item => item.compare.name);
    option.yAxis.data = namelist;
    option.series = [];
    if (isCompared) {
        const compareValueList = sorted.map(item => ({ value: item.compare.value, source: 'Comparison' } as SeriesData));
        const baselineValueList = sorted.map(item => ({ value: item.baseline.value, source: 'Baseline' } as SeriesData));
        const compare: Series = cloneDeep(defaultSeries);
        const baseline: Series = cloneDeep(defaultSeries);
        baseline.name = 'baseline';
        compare.data = compareValueList;
        baseline.data = baselineValueList;
        option.series.push(compare);
        option.series.push(baseline);
    } else {
        const valueList = sorted.map(item => ({ value: item.compare.value } as SeriesData));
        const compare: Series = cloneDeep(defaultSeries);
        compare.data = valueList;
        option.series.push(compare);
    }
    // 左边距
    let maxLength = 0;
    namelist.forEach(item => {
        if (item.length > maxLength) {
            maxLength = item.length;
        }
    });
    option.grid.left = String(maxLength * 9);
    return option;
}

function TopWarpStallReasonChart({ data, isCompared }: Iprops): JSX.Element {
    const { t } = useTranslation('details');
    const showData = useMemo(() => data, [data]);
    const title = t('TopWarpStallReason');
    const unit = t('Stall Count');

    chartVisbilityListener(chartID, () => {
        InitCharts(showData, title, isCompared);
    });

    useEffect(() => {
        setTimeout(() => {
            InitCharts(showData, title, isCompared);
        });
    }, [showData, title, isCompared]);

    return (
        <div style={{ marginBottom: '20px' }}>
            <div style={{ color: COLOR.Grey40, marginBottom: '8px' }}>{unit}</div>
            <div id={chartID} style={{ height: '400px' }}></div>
        </div>
    );
}

export default TopWarpStallReasonChart;

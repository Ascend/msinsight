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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React, { useEffect, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import * as echarts from 'echarts';
import { COLOR, getAdaptiveEchart, chartVisbilityListener, safeStr, sortFunc, chartColors, getDefaultChartOptions } from '@insight/lib/utils';
import { cloneDeep } from 'lodash';

interface Iprops {
    data: Array<{ name: string; value: number }>;
}

interface SeriesData {
    value: number;
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
                result += `<br/>${param?.marker} ${safeStr(param?.name)}: ${param?.data?.value}`;
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

function InitCharts(data: Array<{ name: string; value: number }>, title: string): void {
    const chartDom = document.getElementById(chartID);
    if (chartDom === null || chartDom.offsetParent === null) {
        return;
    }
    const myChart: echarts.ECharts = getAdaptiveEchart(chartDom);
    myChart.setOption(wrapData(data, title), { replaceMerge: ['series'] });
}

function wrapData(data: Array<{ name: string; value: number }>, title: string): any {
    const option = cloneDeep(baseOption);
    option.title.text = title;
    const sorted = [...data].sort((a, b) => sortFunc(String(a.value), String(b.value)));
    const namelist = sorted.map(item => item.name);
    option.yAxis.data = namelist;
    const valueList = sorted.map(item => ({ value: item.value } as SeriesData));
    const series: Series = cloneDeep(defaultSeries);
    series.data = valueList;
    option.series = [series];
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

function TopWarpStallReasonChart({ data }: Iprops): JSX.Element {
    const { t } = useTranslation('details');
    const showData = useMemo(() => data, [data]);
    const title = t('TopWarpStallReason');
    const unit = t('Stall Count');

    chartVisbilityListener(chartID, () => {
        InitCharts(showData, title);
    });

    useEffect(() => {
        setTimeout(() => {
            InitCharts(showData, title);
        });
    }, [showData, title]);

    return (
        <div style={{ marginBottom: '20px' }}>
            <div style={{ color: COLOR.Grey40, marginBottom: '8px' }}>{unit}</div>
            <div id={chartID} style={{ height: '400px' }}></div>
        </div>
    );
}

export default TopWarpStallReasonChart;

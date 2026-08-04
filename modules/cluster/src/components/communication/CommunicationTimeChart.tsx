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
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Spin, CollapsiblePanel, Tooltip } from '@insight/lib/components';
import { HelpIcon, ResetIcon } from '@insight/lib/icon';
import { chartVisbilityListener, COLOR, commonEchartsOptions } from '../Common';
import type { Session } from '../../entity/session';
import i18n from '@insight/lib/i18n';
import { cloneDeep } from 'lodash';
import { chartColors, getAdaptiveEchart, getDefaultChartOptions, safeStr } from '@insight/lib/utils';
import type { LegendComponentOption, TooltipComponentOption } from 'echarts/components';
import type { ECharts } from 'echarts';
import { CompareData, FormatterParams } from '../../utils/interface';

const ZOOM_SIZE = 6;

export interface DataItem {
    index: number;
    rankId: string;
    dbPath: string;
    compareData: CompareData<Duration>;
}

export interface Duration {
    elapseTime: number;
    transitTime: number;
    synchronizationTime: number;
    waitTime: number;
    synchronizationTimeRatio: number;
    waitTimeRatio: number;
    startTime: number;
    idleTime: number;
    sdmaBw: number;
    rdmaBw: number;
}

export interface ChartData {
    rankId: string[];
    elapseTime?: number[];
    transitTime?: number[];
    synchronizationTime?: number[];
    waitTime?: number[];
    synchronizationTimeRatio?: number[];
    waitTimeRatio?: number[];
}

function handleChartClick(myChart: ECharts, data: ChartData, event: { dataIndex?: number }): void {
    const { dataIndex } = event;
    if (typeof dataIndex !== 'number' || data.rankId.length === 0) {
        return;
    }

    const startIndex = Math.max(dataIndex - ZOOM_SIZE / 2, 0);
    const endIndex = Math.min(dataIndex + ZOOM_SIZE / 2, data.rankId.length - 1);
    myChart.dispatchAction({
        type: 'dataZoom',
        startValue: data.rankId[startIndex],
        endValue: data.rankId[endIndex],
    });
}

interface DataZoomOption {
    start?: number;
    end?: number;
}

const isChartZoomed = (chart: ECharts): boolean => {
    const dataZoom = chart.getOption().dataZoom as DataZoomOption[] | undefined;
    const { start = 0, end = 100 } = dataZoom?.[0] ?? {};
    return start > 0 || end < 100;
};

function InitCharts(data: ChartData, isCompare: boolean, onDataZoom: (isZoomed: boolean) => void): ECharts | null {
    const chartDom = document.getElementById('main');
    if (chartDom === null || chartDom.offsetParent === null) {
        return null;
    }
    const myChart = getAdaptiveEchart(chartDom);
    myChart.setOption(wrapData(data, isCompare), { replaceMerge: ['series', 'xAxis', 'yAxis', 'legend', 'dataZoom'] });
    myChart.off('click');
    myChart.on('click', (event) => handleChartClick(myChart, data, event));
    myChart.off('datazoom');
    myChart.on('datazoom', () => onDataZoom(isChartZoomed(myChart)));
    onDataZoom(false);
    return myChart;
}
function wrapData(data: ChartData, isCompare: boolean): any {
    const options = cloneDeep(baseOption);
    options.xAxis[0].data = data.rankId;
    options.legend = getLegend();
    options.series = getSeries({ data });
    options.tooltip = getTooltip(isCompare);
    return options;
}

function getLegend(): LegendComponentOption {
    const legend = baseOption.legend;
    return {
        ...legend,
        data: legend.data.map((legendDataItem: any) => ({
            ...legendDataItem,
            name: i18n.t(`tableHead.${legendDataItem.name}`, { ns: 'communication' }),
        }))
        ,
    };
}

function getSeries({ data }: {data: ChartData}): any {
    return baseOption.series.map((serie: any) => ({
        ...serie,
        name: i18n.t(`tableHead.${serie.name}`, { ns: 'communication' }),
        data: data[serie.id as keyof ChartData],
    }));
}

// isCompare：是否对比状态
function getTooltip(isCompare: boolean): TooltipComponentOption {
    return {
        ...commonEchartsOptions.tooltip,
        confine: true,
        formatter: (params: FormatterParams[]): string => getTooltipFormatter(params, isCompare),
    };
}

function getTooltipFormatter(params: FormatterParams[], isCompare: boolean): string {
    let html = params[0].name;
    params.forEach(serie => {
        const { marker, seriesName, seriesType, value } = serie;
        let valueClass = '';
        if (isCompare) {
            valueClass = value >= 0 ? 'positive-number' : 'negative-number';
        }
        html += `
<div>
    <span>${marker}${safeStr(seriesName)}</span>
    <span class="tooltip-value ${valueClass}">${safeStr(value)} ${seriesType === 'line' ? '' : 'ms'}</span>
</div>`;
    });
    return html;
}

const baseOption: any = {
    textStyle: getDefaultChartOptions().textStyle,
    color: chartColors,
    tooltip: {
        ...commonEchartsOptions.tooltip,
        confine: true,
    },
    toolbox: {
        show: false,
    },
    legend: {
        bottom: 0,
        data: [
            { name: 'Elapse Time', textStyle: { color: COLOR.GREY_50 } },
            { name: 'Transit Time', textStyle: { color: COLOR.GREY_50 } },
            { name: 'Synchronization Time', textStyle: { color: COLOR.GREY_50 } },
            { name: 'Wait Time', textStyle: { color: COLOR.GREY_50 } },
            { name: 'Synchronization Time Ratio', textStyle: { color: COLOR.GREY_50 } },
            { name: 'Wait Time Ratio', textStyle: { color: COLOR.GREY_50 } }],
        tooltip: {
            show: true,
            formatter: function () {
                const div = document.createElement('div');
                div.className = 'legend-tooltip';
                div.append(i18n.t('chart:switchTooltip'));
                return div;
            },
        },
    },
    dataZoom: [
        {
            type: 'inside',
            xAxisIndex: 0,
        },
    ],
    xAxis: [
        {
            type: 'category',
            data: [],
            axisPointer: {
                type: 'shadow',
            },
            axisLabel: {
                color: COLOR.GREY_40,
            },
        },
    ],
    yAxis: [
        {
            type: 'value',
            name: 'Time(ms)',
            axisLabel: {
                formatter: '{value}',
                color: COLOR.GREY_40,
            },
        },
        {
            type: 'value',
            name: 'Ratio',
            axisLabel: {
                formatter: '{value}',
                color: COLOR.GREY_40,
            },
            splitLine: commonEchartsOptions.splitLineY,
        },
    ],
    series: [
        {
            id: 'elapseTime',
            name: 'Elapse Time',
            type: 'bar',
            tooltip: {
                valueFormatter: (value: any): string => {
                    return `${value}ms`;
                },
            },
            data: [],
        },
        {
            id: 'transitTime',
            name: 'Transit Time',
            type: 'bar',
            tooltip: {
                valueFormatter: (value: any): string => {
                    return `${value}ms`;
                },
            },
            data: [],
        },
        {
            id: 'synchronizationTime',
            name: 'Synchronization Time',
            type: 'bar',
            tooltip: {
                valueFormatter: (value: any): string => {
                    return `${value}ms`;
                },
            },
            data: [],
        },
        {
            id: 'waitTime',
            name: 'Wait Time',
            type: 'bar',
            tooltip: {
                valueFormatter: (value: any): string => {
                    return `${value}ms`;
                },
            },
            data: [],
        },
        {
            id: 'synchronizationTimeRatio',
            name: 'Synchronization Time Ratio',
            type: 'line',
            yAxisIndex: 1,
            tooltip: {
                valueFormatter: (value: any): string => {
                    return value;
                },
            },
            data: [],
        },
        {
            id: 'waitTimeRatio',
            name: 'Wait Time Ratio',
            type: 'line',
            yAxisIndex: 1,
            tooltip: {
                valueFormatter: (value: any): string => {
                    return value;
                },
            },
            data: [],
        },
    ],
    grid: {
        left: 100,
        right: 100,
    },
};

const wrapChartData = (data: DataItem[], isCompare: boolean): ChartData => {
    const chartData: ChartData = {} as ChartData;
    chartData.rankId = data.map((item: DataItem) => item.rankId);
    const fields: Array<keyof Duration & keyof ChartData> = ['elapseTime', 'transitTime', 'synchronizationTime',
        'waitTime', 'synchronizationTimeRatio', 'waitTimeRatio'];
    fields.forEach(field => {
        chartData[field] = data.map((item: DataItem) => isCompare ? item.compareData.diff[field] : item.compareData.compare[field]);
    });
    return chartData;
};

const ChartTitle = (): JSX.Element => {
    const { t } = useTranslation('communication');
    return <>
        {t('sessionTitle.VisualizedCommunicationTime')}
        <Tooltip title={<div style={{ padding: '1rem' }}>{t('Chart Zoom Tooltip')}</div>}>
            <HelpIcon style={{ cursor: 'pointer', marginLeft: '3px' }} height={20} width={20}/>
        </Tooltip>
    </>;
};

// 通信时长图 Visualized Communication Time
const CommunicationTimeChart = observer(({ dataSource, session }: {dataSource: DataItem[]; session: Session}) => {
    const { t } = useTranslation('communication');
    const chartRef = useRef<ECharts | null>(null);
    const [isZoomed, setIsZoomed] = useState(false);
    const data = useMemo(() => wrapChartData(dataSource, session.isCompare), [dataSource, session.isCompare]);
    const initCharts = (): void => {
        chartRef.current = InitCharts(data, session.isCompare, setIsZoomed);
    };
    const handleResetZoom = (): void => {
        chartRef.current?.dispatchAction({
            type: 'dataZoom',
            start: 0,
            end: 100,
        });
    };

    chartVisbilityListener('main', initCharts);
    useEffect(() => {
        setTimeout(initCharts);
    }, [dataSource, t, session.isCompare]);
    return (
        <CollapsiblePanel title={<ChartTitle/>}>
            <Spin spinning={session.clusterCompleted && !session.durationFileCompleted } tip="">
                <div style={{ display: 'flex', justifyContent: 'flex-end', paddingRight: '16px' }}>
                    <Tooltip title={t('Reset Zoom')}>
                        <ResetIcon
                            data-testid="communication-time-chart-reset-zoom"
                            disabled={!isZoomed}
                            style={{ cursor: isZoomed ? 'pointer' : 'not-allowed' }}
                            onClick={isZoomed ? handleResetZoom : undefined}
                        />
                    </Tooltip>
                </div>
                <div id={'main'} style={{ height: '400px' }} ></div>
            </Spin>
        </CollapsiblePanel>
    );
});

export default CommunicationTimeChart;

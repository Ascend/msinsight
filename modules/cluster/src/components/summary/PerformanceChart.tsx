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

import React, { useEffect, useRef, useState } from 'react';
import { MIChart } from '@insight/lib/components';
import type { ChartsHandle } from '@insight/lib';
import type { EChartsOption } from 'echarts';
import { merge } from 'lodash';
import { PerformanceDataMap, Session } from '../../entity/session';
import { FormatterParams, PerformanceDataItem } from '../../utils/interface';
import { Advice, safeStr } from '@insight/lib/utils';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { GenerateConditions } from '../../store/parallelism';

const VALUE_ALL = 'All';
const ZOOM_SIZE = 20;

const baseOptions: EChartsOption = {
    legend: {
        type: 'scroll',
    },
    tooltip: {
        trigger: 'axis',
        axisPointer: {
            type: 'cross',
        },
    },
    xAxis: {
        type: 'category',
        name: 'Rank',
        data: [],
        nameGap: 40,
    },
    dataZoom: [
        {
            type: 'inside',
            xAxisIndex: 0,
        },
    ],
    yAxis: [
        {
            type: 'value',
            name: 'Time(μs)',
        },
        {
            type: 'value',
            name: 'Ratio',
            min: function (value: {min: number; max: number}): number {
                if (value.min >= 0) {
                    return 0;
                }
                const base = parseInt((value.min - 10).toFixed(0));
                return base - (base % 10);
            },
            axisLabel: {
                formatter: '{value}%',
            },
        },
    ],
    series: [],
};

interface PerformanceChartProps extends GenerateConditions {
    session: Session;
    group: string;
    orderBy: string;
    top: string;
    setActiveRankId: (rankId: string) => void;
    loading: boolean;
    advices: string[];
}

const getSeries = (session: Session, datasource: PerformanceDataItem[], t: TFunction): any => {
    return session.performanceChartsIndicators?.map(indicator => {
        const { chart, key, name, stack, yAxisType, unit } = indicator;
        const data = datasource.map(item => ({ value: item[key], unit }));
        const yAxisIndex = yAxisType === 'time' ? 0 : 1;
        return {
            name: t(name),
            type: chart,
            stack,
            emphasis: {
                focus: 'series',
            },
            tooltip: {
                valueFormatter: function (value: string): string {
                    return `${value} ${unit}`;
                },
            },
            yAxisIndex,
            data,
        };
    });
};

const getLegend = (session: Session, t: TFunction, legendSelected: null | Record<string, boolean>): EChartsOption['legend'] => {
    const legendData: string[] = [];
    const defaultLegendSelected: Record<string, boolean> = {};

    session.performanceChartsIndicators.forEach(indicator => {
        const { name, visible } = indicator;
        const tName = t(name);
        // 默认显示的图例排在前面
        if (visible) {
            legendData.unshift(tName);
        } else {
            legendData.push(tName);
        }

        defaultLegendSelected[tName] = visible;
    });

    return {
        data: legendData,
        selected: { ...defaultLegendSelected, ...legendSelected },
    };
};

// isCompare：是否对比状态
function getTooltip(isCompare: boolean): any {
    return {
        ...baseOptions.tooltip,
        confine: true,
        formatter: (params: any): string => getTooltipFormatter(params, isCompare),
    };
}

function getTooltipFormatter(params: FormatterParams[], isCompare: boolean): string {
    let html = params[0].name;
    const displaylist = params.map(serie => {
        const { marker, seriesName, value, data } = serie;
        let valueClass = '';
        if (isCompare) {
            valueClass = value >= 0 ? 'positive-number' : 'negative-number';
        }
        return { marker, name: seriesName, content: `${value} ${data.unit}`, contentClass: valueClass };
    });

    html += displaylist.map((displayItem) => `
<div class="tooltip-row">
    <span>${displayItem.marker}${safeStr(displayItem.name)}</span>
    <span class="tooltip-value theme ${displayItem.contentClass}">${safeStr(displayItem.content)}</span>
</div>`).join('');
    return html;
}

export const PerformanceChart = observer((props: PerformanceChartProps): JSX.Element => {
    const { session, setActiveRankId, advices, top, group, orderBy, loading } = props;
    const chartRef = useRef<ChartsHandle>(null);
    const canvasEl = chartRef.current?.getChartDom()?.querySelector('canvas');
    const [chartOptions, setChartOptions] = useState<EChartsOption>({});
    const [datasource, setDatasource] = useState<PerformanceDataItem[]>([]);
    const datasourceRef = useRef<PerformanceDataItem[]>([]);
    const [legendSelected, setLegendSelected] = useState<Record<string, boolean> | null>(null);
    const { t } = useTranslation('summary');

    // 图表的默认宽为100，此处是fix图表初始化时宽度未撑开的问题
    if (canvasEl?.width === 100) {
        chartRef.current?.getInstance()?.resize();
    }

    const filterData = (performanceData: PerformanceDataItem[], performanceDataMap: PerformanceDataMap): PerformanceDataItem[] => {
        let result = session.isCompare
            ? performanceData.map(performanceDataItem => ({
                ...performanceDataItem, ...performanceDataItem.diff,
            }))
            : performanceData;
        if (group !== VALUE_ALL) {
            const groupList = group.split(',');
            const emptyData: Omit<PerformanceDataItem, 'index'> = {};
            Object.keys(performanceData[0] ?? []).forEach(key => {
                if (!['index', 'diff'].includes(key)) {
                    emptyData[key] = 0;
                }
            });
            result = groupList.map(item => {
                const index = Number(item);
                let performanceDataItem = performanceDataMap.get(index);
                if (session.isCompare && performanceDataItem !== undefined) {
                    performanceDataItem = { ...performanceDataItem, ...performanceDataItem.diff };
                }
                return performanceDataItem ?? {
                    index,
                    ...emptyData,
                };
            });
        }

        result = result.slice(0, top === VALUE_ALL ? result.length : Number(top));

        result.sort((a, b) => {
            if (orderBy === 'rankId') {
                return a.index - b.index;
            }
            return a[orderBy] - b[orderBy];
        });

        return result;
    };

    useEffect(() => {
        const legend = getLegend(session, t, legendSelected);
        const series = getSeries(session, datasource, t);

        const options = merge({}, baseOptions, {
            legend,
            xAxis: {
                data: datasource.map(item => item.index),
            },
            series,
            tooltip: getTooltip(session.isCompare),
        });
        setChartOptions(options);
    }, [datasource, t, session.indicatorList, session.isCompare]);

    useEffect(() => {
        const filteredData = filterData(session.performanceData, session.performanceDataMap);
        const firstRankId = filteredData[0]?.index;

        datasourceRef.current = filteredData;
        setDatasource(filteredData);
        if (firstRankId !== undefined) {
            setActiveRankId(firstRankId.toString());
        }
    }, [top, group, orderBy, session.performanceData]);

    const handleChartClick = (event: { name: string; dataIndex?: number }): void => {
        setActiveRankId(event.name);
        const { dataIndex } = event;
        const currentDatasource = datasourceRef.current;
        if (typeof dataIndex !== 'number' || currentDatasource.length === 0) {
            return;
        }

        const startIndex = Math.max(dataIndex - ZOOM_SIZE / 2, 0);
        const endIndex = Math.min(dataIndex + ZOOM_SIZE / 2, currentDatasource.length - 1);
        chartRef.current?.getInstance()?.dispatchAction({
            type: 'dataZoom',
            startValue: currentDatasource[startIndex].index,
            endValue: currentDatasource[endIndex].index,
        });
    };

    return <>
        <MIChart
            width={'calc(100vw - 80px)'}
            ref={chartRef}
            loading={loading}
            options={chartOptions}
            onEvents={
                {
                    click: handleChartClick,
                    legendselectchanged(e): void {
                        setLegendSelected((prevState) => ({ ...prevState, ...e.selected }));
                    },
                }
            }
        />
        {
            advices.length > 0
                ? <div>
                    <Advice style={{ marginBottom: 0 }} text={advices}/>
                </div>
                : <></>
        }
    </>;
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

import type { EChartsOption, SeriesOption } from 'echarts';
import {
    communicationFields, timeFields,
    type SummaryKind, type SummaryResponse, type SummaryResponses, type TimeField,
} from './api';

export type RankOrder = 'rank' | 'total';

const formatValue = (value: unknown): string => typeof value === 'number' && Number.isFinite(value) ? value.toFixed(3) : '-';
const formatPercent = (value: unknown): string => `${formatValue(value)}%`;

export function sortRanks<T extends { rankId: string; time: Record<string, number> }>(ranks: T[], order: RankOrder): T[] {
    const total = (row: T): number => row.time.total ?? communicationFields.reduce((sum, key) => sum + row.time[key], 0);
    return [...ranks].sort((a, b) => {
        const difference = order === 'total' ? total(b) - total(a) : 0;
        return difference || a.rankId.localeCompare(b.rankId, undefined, { numeric: true });
    });
}

export function chartOptions(
    kind: SummaryKind, response: SummaryResponse, label: (key: string) => string, order: RankOrder,
    selectedTimeField?: TimeField,
): EChartsOption {
    const opacity = (field?: string): number => selectedTimeField === undefined || selectedTimeField === field ? 1 : 0.2;
    if (kind === 'clusterTime') {
        const data = response as SummaryResponses['clusterTime'];
        const total = timeFields.reduce((sum, key) => sum + data.time[key], 0);
        return {
            textStyle: { fontFamily: 'sans-serif' },
            color: ['#0062DC', '#38B5FA', '#29B383', '#BA43E8', '#FF5432'],
            xAxis: { show: false },
            yAxis: { show: false },
            aria: { enabled: true },
            legend: { type: 'scroll', top: 0 },
            tooltip: { trigger: 'item', valueFormatter: value => `${formatValue(value)} ${data.units.time}` },
            title: {
                text: total.toLocaleString(undefined, { minimumFractionDigits: 3, maximumFractionDigits: 3 }),
                subtext: `${label('Total')} (${data.units.time})`,
                left: 'center',
                top: '48%',
            },
            series: [{
                type: 'pie',
                radius: ['45%', '65%'],
                center: ['50%', '58%'],
                stillShowZeroSum: false,
                percentPrecision: 3,
                label: { formatter: params => formatPercent(params.percent), fontFamily: 'sans-serif', fontSize: 12, textBorderWidth: 0, color: '#8D98AA' },
                emphasis: { label: { textBorderWidth: 0 } },
                data: timeFields.map(key => ({
                    name: label(key),
                    value: data.time[key],
                    itemStyle: { opacity: opacity(key) },
                    emphasis: { itemStyle: { opacity: opacity(key) } },
                })),
            }],
        };
    }

    const base: EChartsOption = {
        textStyle: { fontFamily: 'sans-serif' },
        color: ['#0062DC', '#38B5FA', '#29B383', '#BA43E8', '#FF5432', '#0077FF', '#FF4D8D'],
        aria: { enabled: true },
        legend: { type: 'scroll', top: 0 },
        tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' }, valueFormatter: formatValue },
        grid: { left: 16, right: 32, top: 70, bottom: 52, containLabel: true },
        dataZoom: [],
    };
    if (kind === 'clusterBandwidth') {
        const data = response as SummaryResponses['clusterBandwidth'];
        return {
            ...base,
            xAxis: { type: 'category', data: data.transportTypes.map(row => row.transportType) },
            yAxis: { type: 'value', name: `${label('Bandwidth')} (${data.units.bandwidth})`, axisLabel: { formatter: formatValue } },
            series: [{ type: 'bar', name: label('Bandwidth'), barMaxWidth: 48, data: data.transportTypes.map(row => row.bandwidth) }],
        };
    }
    const data = response as SummaryResponses['rankTime'];
    const ranks = sortRanks(data.ranks, order);
    const fields = kind === 'communicationTime' ? communicationFields : timeFields;
    const series: SeriesOption[] = fields.map(key => ({
        type: 'bar',
        name: label(key),
        stack: 'time',
        barMaxWidth: 48,
        data: ranks.map(row => (row.time as Record<string, number>)[key]),
        ...(kind === 'rankTime'
            ? {
                itemStyle: { opacity: opacity(key) }, emphasis: { itemStyle: { opacity: opacity(key) } },
            }
            : {}),
    }));
    if (kind === 'rankTime') {
        (['compute', 'communication'] as const).forEach(key => {
            series.push({
                type: 'line',
                name: label(key),
                yAxisIndex: 1,
                data: ranks.map(row => row.ratio[key]),
                lineStyle: { opacity: opacity() },
                itemStyle: { opacity: opacity() },
                emphasis: { lineStyle: { opacity: opacity() }, itemStyle: { opacity: opacity() } },
                tooltip: { valueFormatter: formatPercent },
            });
        });
    }
    const timeAxis = { type: 'value' as const, name: `${label('Time')} (${data.units.time})`, axisLabel: { formatter: formatValue } };
    return {
        ...base,
        xAxis: { type: 'category', name: 'Rank', nameLocation: 'middle', nameGap: 32, data: ranks.map(row => row.rankId) },
        yAxis: kind === 'communicationTime'
            ? timeAxis
            : [
                timeAxis,
                { type: 'value', name: `${label('Ratio')} (%)`, min: 0, max: 100, axisLabel: { formatter: formatPercent } },
            ],
        series,
    };
}

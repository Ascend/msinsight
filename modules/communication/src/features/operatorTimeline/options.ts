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
import { graphic, type EChartsOption } from 'echarts';
import { getDefaultChartOptions } from '@insight/lib/utils';
import type { Theme } from '@emotion/react';
import type { CommunicationWebGLIndex } from './dataIndex';
import { palette } from './colors';
import type { Range } from './navigation';

export const chartHeight = (rankCount: number): number => Math.min(800, Math.max(300, rankCount * 30)) + 160;
export const formatTime = (value: number): string => value.toFixed(6).replace(/\.?0+$/, '');

/** ECharts owns axes and both zoom sliders in WebGL and fallback modes. */
export function chartOptions(index: CommunicationWebGLIndex, theme: Theme, webgl: boolean, initialRange?: Range): EChartsOption {
    const height = chartHeight(index.ranks.length) - 160;
    const operators = webgl ? [] : index.ranks.flatMap(rank => rank.operators.items);
    const fallbackRange: Range = [0, operators.length > 10000 ? Math.ceil(10000 / operators.length * 100) : 100];
    const horizontal = initialRange ?? (webgl ? [0, 100] : fallbackRange);
    return {
        animation: false,
        textStyle: getDefaultChartOptions().textStyle,
        dataZoom: [
            {
                type: 'slider',
                filterMode: 'weakFilter',
                showDataShadow: false,
                top: height + 100,
                labelFormatter: '',
                start: horizontal[0],
                end: horizontal[1],
                xAxisIndex: 0,
                bottom: 10,
                height: 20,
                borderColor: '#d2dbee80',
            },
            {
                type: 'slider',
                filterMode: 'weakFilter',
                showDataShadow: false,
                labelFormatter: '',
                start: !webgl && index.ranks.length > 516 ? 100 - Math.ceil(516 / index.ranks.length * 100) : 0,
                end: 100,
                yAxisIndex: 0,
                right: 10,
                width: 20,
                borderColor: '#d2dbee80',
            },
            { type: 'inside', filterMode: 'weakFilter', zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: 'shift' },
        ],
        grid: { left: 100, right: 120, height },
        xAxis: {
            type: 'value',
            scale: true,
            name: 'Time(ms)',
            min: index.minTime,
            max: index.maxTime,
            axisLabel: { formatter: (value: number) => formatTime(Math.max(0, value)), color: theme.textColorTertiary },
            axisLine: { lineStyle: { color: theme.textColorTertiary } },
            splitLine: { lineStyle: { color: theme.borderColorLight } },
        },
        yAxis: {
            type: 'category',
            data: index.ranks.map(rank => rank.rankId),
            name: 'Rank ID',
            axisLabel: { color: theme.textColorTertiary },
            axisLine: { lineStyle: { color: theme.textColorTertiary } },
        },
        tooltip: { show: false },
        series: webgl
            ? []
            : [{
                type: 'custom',
                encode: { x: [1, 2], y: 0 },
                renderItem: (params: any, api: any) => {
                    const start = api.coord([api.value(1), api.value(0)]);
                    const end = api.coord([api.value(2), api.value(0)]);
                    const rowHeight = api.size([0, 1])[1] * 0.6;
                    const shape = graphic.clipRectByRect({ x: start[0], y: start[1] - rowHeight / 2, width: end[0] - start[0], height: rowHeight },
                        { x: params.coordSys.x, y: params.coordSys.y, width: params.coordSys.width, height: params.coordSys.height });
                    return shape
                        ? {
                            type: 'rect',
                            shape,
                            style: api.style(),
                            emphasis: {
                                style: {
                                    stroke: '#ffd666', lineWidth: 4, shadowBlur: 12, shadowColor: 'rgba(255, 214, 102, 0.9)',
                                },
                            },
                        }
                        : undefined;
                },
                data: operators.map(op => ({
                    name: String(op.id),
                    value: [op.rankIndex, op.startTime, op.endTime, op.duration],
                    itemStyle: { color: theme.colorPalette[palette[op.colorIndex]] },
                })),
            }],
    };
}

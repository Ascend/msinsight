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
import type { Theme } from '@emotion/react';
import type { EChartsOption, ECharts } from 'echarts';
import type { TFunction } from 'i18next';
import { BANDS, LINK_TYPES, cellLabel, decimalPlaces, type Cell, type Metric } from './data';
import { matrixTooltip } from './tooltip';

const GRID_TOP = 16;
const GRID_BOTTOM = 110;
/** Cap each rank row at 50px and the entire canvas (including axes and legend) at 800px. */
export const matrixHeight = (rankCount: number): number => Math.min(800, rankCount * 50 + GRID_TOP + GRID_BOTTOM);

export function matrixOptions(data: Cell[], ranks: number[], metric: Metric, extent: [number, number], theme: Theme, t: TFunction): EChartsOption {
    const [min, max] = extent;
    const colors = min === max ? [BANDS[1]] : (metric === 'transitTime' ? BANDS.slice(0, 4).reverse() : BANDS.slice(0, 4));
    const legend = { orient: 'horizontal' as const, bottom: 0, left: 'center', dimension: 2, textStyle: { color: theme.textColorTertiary } };
    return {
        animation: false,
        grid: { left: 100, right: 100, top: GRID_TOP, bottom: GRID_BOTTOM },
        // The legacy matrix uses ECharts' light HTML tooltip in both themes.
        tooltip: {
            show: true,
            renderMode: 'html',
            backgroundColor: '#fff',
            borderWidth: 1,
            padding: 10,
            textStyle: { color: '#666', fontSize: 14 },
            formatter: params => matrixTooltip((params as unknown as { data: Cell }).data, t),
        },
        xAxis: { type: 'category', name: 'Src Rank Id', nameLocation: 'middle', nameGap: 28, data: ranks.map(String), splitArea: { show: true } },
        yAxis: { type: 'category', name: 'Dst Rank Id', nameLocation: 'middle', nameGap: 45, data: ranks.map(String) },
        visualMap: metric === 'transportType'
            ? { ...legend, type: 'piecewise', splitNumber: 1, pieces: LINK_TYPES.map((label, value) => ({ label, value, color: BANDS[value] })) }
            : { ...legend, min, max, calculable: true, itemHeight: 300, precision: Math.max(decimalPlaces(min), decimalPlaces(max)), inRange: { color: colors } },
        // Keep zoom and pan inside the plot, without visible slider controls.
        dataZoom: [
            { type: 'inside', xAxisIndex: [0], start: 0, end: 100, zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false, moveOnMouseMove: true },
            { type: 'inside', yAxisIndex: [0], start: 0, end: 100, zoomOnMouseWheel: 'ctrl', moveOnMouseWheel: false, moveOnMouseMove: true },
        ],
        series: [{
            type: 'heatmap',
            data,
            progressive: 2000,
            emphasis: { itemStyle: { shadowBlur: 10, shadowColor: theme.textColorTertiary } },
            label: { show: ranks.length <= 16, formatter: params => cellLabel((params as unknown as { data: Cell }).data, metric) },
        }],
    };
}

/** Read the X viewport rather than the event's first axis, which may be a Y-only zoom. */
export function updateMatrixLabels(chart: Pick<ECharts, 'getOption' | 'setOption'>): boolean {
    const option = chart.getOption() as { xAxis: Array<{ data: unknown[] }>; dataZoom: Array<{ start?: number; end?: number }> };
    const { start = 0, end = 100 } = option.dataZoom[0];
    const visibleCount = Math.round((end - start) / 100 * option.xAxis[0].data.length);
    chart.setOption({ series: [{ label: { show: visibleCount <= 16 } }] });
    return option.dataZoom.some(zoom => (zoom.start ?? 0) > 0 || (zoom.end ?? 100) < 100);
}

export function resetMatrixZoom(chart: Pick<ECharts, 'dispatchAction'>): void {
    chart.dispatchAction({ type: 'dataZoom', batch: [0, 1].map(dataZoomIndex => ({ dataZoomIndex, start: 0, end: 100 })) });
}

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
import type { ECharts } from 'echarts';

export type ChartNavigation = 'zoomInX' | 'zoomOutX' | 'left' | 'right' | 'zoomInY' | 'zoomOutY' | 'up' | 'down';
export interface ChartPointer { x: number; y: number }

const ZOOM_FACTOR = 1.2;
const PAN_RATIO = 0.1;
const clamp = (value: number, min: number, max: number): number => Math.min(max, Math.max(min, value));

export const getChartNavigation = (key: string, shift: boolean): ChartNavigation | undefined => {
    if (shift) {
        return ({ w: 'zoomInY', s: 'zoomOutY' } as const)[key.toLowerCase() as 'w' | 's'];
    }
    const actions: Record<string, ChartNavigation> = {
        w: 'zoomInX', s: 'zoomOutX', a: 'left', d: 'right', arrowup: 'up', arrowdown: 'down',
    };
    return actions[key.toLowerCase()];
};

const getAnchor = (chart: ECharts, pointer: ChartPointer | null, vertical: boolean, inverse: boolean): number => {
    if (!pointer || !chart.containPixel({ gridIndex: 0 }, [pointer.x, pointer.y])) {
        return 0.5;
    }
    // Use the plotting rectangle, excluding axis labels and the zoom sliders.
    const rect = (chart as any).getModel()?.getComponent('grid', 0)?.coordinateSystem?.getRect();
    if (!rect || rect.width <= 0 || rect.height <= 0) {
        return 0.5;
    }
    const position = vertical ? 1 - (pointer.y - rect.y) / rect.height : (pointer.x - rect.x) / rect.width;
    return clamp(inverse ? 1 - position : position, 0, 1);
};

export const navigateCommunicationChart = (chart: ECharts, action: ChartNavigation, pointer: ChartPointer | null): void => {
    const vertical = ['zoomInY', 'zoomOutY', 'up', 'down'].includes(action);
    const axisIndex = vertical ? 1 : 0;
    const option = chart.getOption();
    const zoom = (option.dataZoom as Array<{ start?: number; end?: number }>)?.[axisIndex];
    const axis = (option.yAxis as Array<{ data?: unknown[]; inverse?: boolean }>)?.[0];
    const rankCount = axis?.data?.length ?? 0;
    if (!zoom || rankCount === 0 || (vertical && rankCount === 1)) {
        return;
    }
    const start = zoom.start ?? 0;
    const end = zoom.end ?? 100;
    const zoomingIn = action === 'zoomInX' || action === 'zoomInY';
    const zooming = zoomingIn || action === 'zoomOutX' || action === 'zoomOutY';
    const anchor = getAnchor(chart, pointer, vertical, vertical && !!axis?.inverse);
    let nextStart: number;
    let nextEnd: number;
    if (vertical) {
        // Category windows use whole ranks so repeated keystrokes never stall between rows.
        const lastRank = rankCount - 1;
        const first = Math.round(start / 100 * lastRank);
        const last = Math.round(end / 100 * lastRank);
        const count = last - first + 1;
        const nextCount = zooming
            ? clamp(zoomingIn ? Math.floor(count / ZOOM_FACTOR) : Math.ceil(count * ZOOM_FACTOR), 1, rankCount)
            : count;
        const direction = (action === 'up' ? 1 : -1) * (axis?.inverse ? -1 : 1);
        const nextFirst = clamp(zooming
            ? Math.round(first + (count - nextCount) * anchor)
            : first + direction * Math.max(1, Math.round(count * PAN_RATIO)), 0, rankCount - nextCount);
        nextStart = nextFirst / lastRank * 100;
        nextEnd = (nextFirst + nextCount - 1) / lastRank * 100;
    } else {
        const span = end - start;
        const nextSpan = zooming ? clamp(span * (zoomingIn ? 1 / ZOOM_FACTOR : ZOOM_FACTOR), 0.000001, 100) : span;
        nextStart = clamp(zooming
            ? start + (span - nextSpan) * anchor
            : start + (action === 'left' ? -1 : 1) * span * PAN_RATIO, 0, 100 - nextSpan);
        nextEnd = nextStart + nextSpan;
    }
    chart.dispatchAction({ type: 'dataZoom', dataZoomIndex: axisIndex, start: nextStart, end: nextEnd });
};

export const resetCommunicationChartZoom = (chart: ECharts): void => {
    chart.dispatchAction({
        type: 'dataZoom',
        batch: [0, 1].map(dataZoomIndex => ({ dataZoomIndex, start: 0, end: 100 })),
    });
};

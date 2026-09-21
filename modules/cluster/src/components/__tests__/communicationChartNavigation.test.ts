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
import { getChartNavigation, navigateCommunicationChart, resetCommunicationChartZoom } from '../communication/communicationChartNavigation';

const createChart = (rankCount = 100, inverse = false): ECharts => {
    const option = {
        dataZoom: [{ start: 20, end: 60 }, { start: 0, end: 100 }],
        yAxis: [{ data: Array.from({ length: rankCount }, (_, index) => `${index}`), inverse }],
    };
    return {
        getOption: () => option,
        containPixel: () => true,
        getModel: () => ({ getComponent: () => ({ coordinateSystem: { getRect: () => ({ x: 100, y: 50, width: 800, height: 300 }) } }) }),
        dispatchAction: jest.fn(({ dataZoomIndex, start, end }) => {
            if (dataZoomIndex !== undefined) {
                option.dataZoom[dataZoomIndex] = { start, end };
            }
        }),
    } as unknown as ECharts;
};

it('keeps horizontal shortcuts and explicitly maps Shift to vertical zoom', () => {
    expect(['w', 's', 'a', 'd', 'ArrowUp', 'ArrowDown'].map(key => getChartNavigation(key, false)))
        .toEqual(['zoomInX', 'zoomOutX', 'left', 'right', 'up', 'down']);
    expect(getChartNavigation('W', true)).toBe('zoomInY');
    expect(getChartNavigation('S', true)).toBe('zoomOutY');
    expect(getChartNavigation('a', true)).toBeUndefined();
    expect(getChartNavigation('Tab', false)).toBeUndefined();
});

it('keeps the time under the pointer fixed and leaves the other axis unchanged', () => {
    const chart = createChart();
    navigateCommunicationChart(chart, 'zoomInX', { x: 300, y: 100 });
    const [zoom, ranks] = chart.getOption().dataZoom as Array<{ start: number; end: number }>;
    expect(zoom.end - zoom.start).toBeCloseTo(40 / 1.2);
    expect(zoom.start + (zoom.end - zoom.start) * 0.25).toBeCloseTo(30);
    expect(ranks).toEqual({ start: 0, end: 100 });
});

it('zooms at the center without a pointer and clamps panning at both boundaries', () => {
    const chart = createChart();
    navigateCommunicationChart(chart, 'zoomInX', null);
    let zoom = (chart.getOption().dataZoom as Array<{ start: number; end: number }>)[0];
    expect((zoom.start + zoom.end) / 2).toBeCloseTo(40);
    for (let index = 0; index < 40; index++) { navigateCommunicationChart(chart, 'left', null); }
    zoom = (chart.getOption().dataZoom as Array<{ start: number; end: number }>)[0];
    expect(zoom.start).toBe(0);
    expect(zoom.end - zoom.start).toBeCloseTo(40 / 1.2);
    for (let index = 0; index < 40; index++) { navigateCommunicationChart(chart, 'right', null); }
    zoom = (chart.getOption().dataZoom as Array<{ start: number; end: number }>)[0];
    expect(zoom.end).toBe(100);
});

it('can zoom four ranks down to one and back out without getting stuck', () => {
    const chart = createChart(4);
    for (let index = 0; index < 5; index++) { navigateCommunicationChart(chart, 'zoomInY', null); }
    let zoom = (chart.getOption().dataZoom as Array<{ start: number; end: number }>)[1];
    expect(zoom.start).toBe(zoom.end);
    for (let index = 0; index < 5; index++) { navigateCommunicationChart(chart, 'zoomOutY', null); }
    zoom = (chart.getOption().dataZoom as Array<{ start: number; end: number }>)[1];
    expect(zoom).toEqual({ start: 0, end: 100 });
});

it.each([false, true])('pans toward the top of the plot with inverse=%s', inverse => {
    const chart = createChart(100, inverse);
    navigateCommunicationChart(chart, 'zoomInY', null);
    const before = (chart.getOption().dataZoom as Array<{ start: number }>)[1].start;
    navigateCommunicationChart(chart, 'up', null);
    const after = (chart.getOption().dataZoom as Array<{ start: number }>)[1].start;
    expect(inverse ? after < before : after > before).toBe(true);
    expect(after / 100 * 99).toBeCloseTo(Math.round(after / 100 * 99));
});

it.each([0, 1])('does not produce invalid rank windows with %s ranks', count => {
    const chart = createChart(count);
    navigateCommunicationChart(chart, 'zoomInY', null);
    expect(chart.dispatchAction).not.toHaveBeenCalled();
});

it('resets both axes in one action', () => {
    const chart = createChart();
    resetCommunicationChartZoom(chart);
    expect(chart.dispatchAction).toHaveBeenCalledWith({
        type: 'dataZoom',
        batch: [
            { dataZoomIndex: 0, start: 0, end: 100 }, { dataZoomIndex: 1, start: 0, end: 100 },
        ],
    });
});

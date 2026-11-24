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
import { themeInstance } from '@insight/lib/theme';
import { ChartController } from '../ChartController';
import { createCommunicationTimeWebGLRenderer } from '../WebGLRenderer';
import { disposeAdaptiveEchart, getAdaptiveEchart } from '@insight/lib/utils';
import { getWebGLLayout } from '../geometry';

jest.mock('@insight/lib/utils', () => ({
    ...jest.requireActual('@insight/lib/utils'), getAdaptiveEchart: jest.fn(), disposeAdaptiveEchart: jest.fn(),
}));
jest.mock('../WebGLRenderer', () => ({ ...jest.requireActual('../WebGLRenderer'), createCommunicationTimeWebGLRenderer: jest.fn() }));
jest.mock('../geometry', () => ({ ...jest.requireActual('../geometry'), getWebGLLayout: jest.fn() }));
const theme = themeInstance.getTheme().light;
const data = { minTime: 0, maxTime: 10000000, rows: [{ rankId: '4', dbPath: '/4.db', operators: [{ operatorName: '<allReduce>', startTime: 1000000, duration: 5000000 }] }] };
let node: HTMLDivElement;
let chart: any;
let renderer: { setColors: jest.Mock; setData: jest.Mock; setHoveredOperator: jest.Mock; setSelectedOperator: jest.Mock; clear: jest.Mock; dispose: jest.Mock };
let events: Record<string, (...args: any[]) => void>;
let pointerEvents: Record<string, (...args: any[]) => void>;
let loseContext: () => void;
beforeEach(() => {
    jest.useFakeTimers();
    node = document.createElement('div'); document.body.appendChild(node);
    events = {}; pointerEvents = {};
    const zr = { on: jest.fn((name, fn) => { pointerEvents[name] = fn; }), off: jest.fn() };
    let options: any;
    chart = {
        setOption: jest.fn(value => { options = value; events.rendered?.(); }),
        getOption: () => options,
        on: jest.fn((name, fn) => { events[name] = fn; }),
        off: jest.fn(),
        getZr: () => zr,
        resize: jest.fn(),
        dispatchAction: jest.fn(action => {
            if (action.type !== 'dataZoom') { return; }
            (action.batch ?? [action]).forEach((zoom: any) => {
                options.dataZoom[zoom.dataZoomIndex] = { ...options.dataZoom[zoom.dataZoomIndex], ...zoom };
            });
            events.datazoom?.();
        }),
    };
    renderer = { setColors: jest.fn(), setData: jest.fn(), setHoveredOperator: jest.fn(), setSelectedOperator: jest.fn(), clear: jest.fn(), dispose: jest.fn() };
    (getAdaptiveEchart as jest.Mock).mockReturnValue(chart);
    (createCommunicationTimeWebGLRenderer as jest.Mock).mockImplementation((_node, lost) => { loseContext = lost; return renderer; });
    (getWebGLLayout as jest.Mock).mockReturnValue({
        canvasWidth: 800,
        canvasHeight: 460,
        gridLeft: 100,
        gridTop: 60,
        gridWidth: 580,
        gridHeight: 300,
        xMin: 0,
        xMax: 10,
        rankY0: 210,
        rankStep: -300,
        rowHeight: 300,
    });
});
afterEach(() => { node.remove(); jest.useRealTimers(); });
it('uses WebGL for operators while ECharts owns both slider axes', () => {
    const controller = new ChartController(node, data, theme, { onMenu: jest.fn(), onZoom: jest.fn() });
    jest.advanceTimersByTime(20);
    expect(node.dataset.renderer).toBe('webgl');
    expect(chart.getOption().dataZoom.slice(0, 2).map((zoom: any) => zoom.type)).toEqual(['slider', 'slider']);
    expect(chart.getOption().series).toEqual([]);
    expect(renderer.setData.mock.calls[0][0][0].original).toBe(data.rows[0].operators[0]);
    controller.dispose();
    expect(renderer.dispose).toHaveBeenCalledTimes(1);
    expect(disposeAdaptiveEchart).toHaveBeenCalledWith(node);
    expect(node.querySelector('[role="tooltip"]')).toBeNull();
    expect(jest.getTimerCount()).toBe(0);
});
it('shows escaped tooltip fields with a color marker and emits the original hit data', () => {
    const menu = jest.fn();
    const controller = new ChartController(node, data, theme, { onMenu: menu, onZoom: jest.fn() });
    pointerEvents.mousemove({ offsetX: 220, offsetY: 210 });
    const tip = node.querySelector('[role="tooltip"]') as HTMLElement;
    expect(tip.textContent).toContain('<allReduce>');
    expect(tip.textContent).toContain('1ms');
    expect(tip.textContent).toContain('5ms');
    expect(tip.querySelector('allreduce')).toBeNull();
    expect(tip.querySelectorAll('strong')).toHaveLength(4);
    expect(tip.querySelector('.operator-timeline-tooltip-dot')).not.toBeNull();
    pointerEvents.contextmenu({ offsetX: 220, offsetY: 210 });
    expect(menu).toHaveBeenCalledWith({ row: data.rows[0], op: data.rows[0].operators[0] });
    expect(tip.style.display).toBe('none');
    controller.dispose();
});
it('falls back to ECharts on context loss while preserving both zoom windows', () => {
    const controller = new ChartController(node, data, theme, { onMenu: jest.fn(), onZoom: jest.fn() }, [25, 50]);
    chart.dispatchAction({ type: 'dataZoom', dataZoomIndex: 1, start: 20, end: 80 });
    loseContext();
    expect(node.dataset.renderer).toBe('echarts');
    expect(chart.getOption().series[0].type).toBe('custom');
    expect(chart.getOption().dataZoom[0]).toMatchObject({ start: 25, end: 50 });
    expect(chart.getOption().dataZoom[1]).toMatchObject({ start: 20, end: 80 });
    controller.dispose();
    expect(renderer.dispose).toHaveBeenCalledTimes(1);
});
it('falls back when WebGL is unavailable and clears the five-second selection timer', () => {
    (createCommunicationTimeWebGLRenderer as jest.Mock).mockReturnValue(null);
    const controller = new ChartController(node, data, theme, { onMenu: jest.fn(), onZoom: jest.fn() });
    expect(node.dataset.renderer).toBe('echarts');
    controller.highlight({ rankId: '4', name: '<allReduce>', startValue: 1, endValue: 6 });
    expect(chart.dispatchAction).toHaveBeenCalledWith(expect.objectContaining({ type: 'highlight' }));
    jest.advanceTimersByTime(5000);
    expect(chart.dispatchAction).toHaveBeenLastCalledWith(expect.objectContaining({ type: 'downplay' }));
    controller.dispose();
    expect(jest.getTimerCount()).toBe(0);
});

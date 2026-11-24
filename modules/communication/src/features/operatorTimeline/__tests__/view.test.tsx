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
import React from 'react';
import { act, fireEvent } from '@testing-library/react';
import { renderWithProviders as render } from '../../../../../testing/render';
import { OperatorTimeline } from '..';
import { ChartController } from '../ChartController';
import type { OperatorHit } from '../types';

jest.mock('../ChartController', () => ({ ChartController: jest.fn() }));
const data = { minTime: 0, maxTime: 10000000, rows: [{ rankId: '4', dbPath: '/4.db', operators: [{ operatorName: 'allReduce', startTime: 0, duration: 5000000 }] }] };
const defaults = { data, dark: false, canLocate: true, aligned: '', onLocate: jest.fn(), onAlign: jest.fn() };
let callbacks: { onMenu: (hit: OperatorHit) => void; onZoom: (range: [number, number], zoomed: boolean) => void };
let controller: { chart: any; update: jest.Mock; highlight: jest.Mock; clearHighlight: jest.Mock; resize: jest.Mock; dispose: jest.Mock; setMenuOpen: jest.Mock };
beforeEach(() => {
    jest.spyOn(HTMLElement.prototype, 'clientWidth', 'get').mockReturnValue(800);
    jest.spyOn(HTMLElement.prototype, 'clientHeight', 'get').mockReturnValue(460);
    Object.defineProperty(window, 'ResizeObserver', { configurable: true, value: class { observe(): void {} disconnect(): void {} } });
    controller = {
        chart: {
            getOption: () => ({ dataZoom: [{ start: 0, end: 100 }, { start: 0, end: 100 }], yAxis: [{ data: ['4'] }] }),
            dispatchAction: jest.fn(),
            containPixel: () => false,
        },
        update: jest.fn(),
        highlight: jest.fn(),
        clearHighlight: jest.fn(),
        resize: jest.fn(),
        dispose: jest.fn(),
        setMenuOpen: jest.fn(),
    };
    (ChartController as jest.Mock).mockImplementation((_node, _data, _theme, handlers) => { callbacks = handlers; return controller; });
});
it('keeps keyboard navigation scoped to the focused chart and releases its resources', () => {
    const view = render(<OperatorTimeline {...defaults} />);
    const region = view.getByRole('region');
    fireEvent.keyDown(region, { key: 'w' });
    expect(controller.chart.dispatchAction).not.toHaveBeenCalled();
    act(() => region.focus()); fireEvent.keyDown(region, { key: 'w' });
    expect(controller.chart.dispatchAction).toHaveBeenCalledWith(expect.objectContaining({ type: 'dataZoom', dataZoomIndex: 0 }));
    expect(fireEvent.wheel(region, { deltaY: 10 })).toBe(true);
    fireEvent.keyDown(region, { key: 'Escape' }); expect(region).not.toHaveFocus();
    view.unmount(); expect(controller.dispose).toHaveBeenCalledTimes(1);
});
it('routes the hit operator to alignment and Timeline with its actual rank path', async () => {
    const align = jest.fn(); const locate = jest.fn();
    const view = render(<OperatorTimeline {...defaults} onAlign={align} onLocate={locate} />);
    const hit = { row: data.rows[0], op: data.rows[0].operators[0] };
    await act(async () => callbacks.onMenu(hit));
    fireEvent.click(view.getByRole('menuitem', { name: 'Align according to selected operator' }));
    expect(align).toHaveBeenCalledWith('allReduce');
    await act(async () => callbacks.onMenu(hit));
    fireEvent.click(view.getByRole('menuitem', { name: 'Find in Timeline' }));
    expect(locate).toHaveBeenCalledWith(hit.row, hit.op);
});
it('preserves a saved horizontal viewport and forwards zoom changes to the section', () => {
    const change = jest.fn();
    render(<OperatorTimeline {...defaults} initialRange={[25, 50]} onRangeChange={change} />);
    expect(ChartController).toHaveBeenCalledWith(expect.any(HTMLElement), data, expect.any(Object), expect.any(Object), [25, 50]);
    act(() => callbacks.onZoom([30, 60], true));
    expect(change).toHaveBeenLastCalledWith([30, 60]);
});

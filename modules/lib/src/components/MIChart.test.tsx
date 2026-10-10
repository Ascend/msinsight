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
import { act, cleanup, render } from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import * as echarts from 'echarts';
import ResizeObserver from 'resize-observer-polyfill';
import { light } from '../theme/light';
import { MIChart, type ChartsHandle } from './MIChart';

jest.mock('echarts', () => ({ init: jest.fn() }));
jest.mock('resize-observer-polyfill', () => jest.fn());

describe('MIChart visibility lifecycle', () => {
    let notifyResize: () => void;
    const disconnect = jest.fn();
    const chart = {
        setOption: jest.fn(),
        resize: jest.fn(),
        on: jest.fn(),
        off: jest.fn(),
        showLoading: jest.fn(),
        hideLoading: jest.fn(),
        dispose: jest.fn(),
    };

    beforeEach(() => {
        jest.clearAllMocks();
        (echarts.init as jest.Mock).mockReturnValue(chart);
        (ResizeObserver as jest.Mock).mockImplementation((callback) => {
            notifyResize = callback;
            return { observe: jest.fn(), disconnect };
        });
    });

    afterEach(cleanup);

    it('waits for layout, applies the latest props, resizes and cleans up', () => {
        const ref = React.createRef<ChartsHandle>();
        const firstClick = jest.fn();
        const latestClick = jest.fn();
        const view = render(<ThemeProvider theme={light}>
            <MIChart ref={ref} options={{ title: { text: 'Before' } }} onEvents={{ click: firstClick }} />
        </ThemeProvider>);
        const dom = ref.current?.getChartDom() as HTMLDivElement;
        expect(echarts.init).not.toHaveBeenCalled();
        expect(ref.current?.getInstance()).toBeNull();
        view.rerender(<ThemeProvider theme={light}>
            <MIChart ref={ref} options={{ title: { text: 'Latest' } }} loading onEvents={{ click: latestClick }} />
        </ThemeProvider>);
        Object.defineProperties(dom, {
            clientWidth: { configurable: true, value: 640 },
            clientHeight: { configurable: true, value: 400 },
        });
        act(() => notifyResize());
        expect(echarts.init).toHaveBeenCalledTimes(1);
        expect(echarts.init).toHaveBeenCalledWith(dom);
        expect(ref.current?.getInstance()).toBe(chart);
        expect(ref.current?.chartInstance).toBe(chart);
        expect(chart.setOption).toHaveBeenLastCalledWith(expect.objectContaining({
            title: expect.objectContaining({ text: 'Latest' }),
        }), true);
        expect(chart.showLoading).toHaveBeenCalled();
        expect(chart.on).toHaveBeenCalledWith('click', latestClick);
        expect(chart.on).not.toHaveBeenCalledWith('click', firstClick);

        Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 0 });
        act(() => notifyResize());
        expect(chart.resize).not.toHaveBeenCalled();
        Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 800 });
        act(() => notifyResize());
        expect(chart.resize).toHaveBeenCalledTimes(1);
        expect(echarts.init).toHaveBeenCalledTimes(1);

        view.rerender(<ThemeProvider theme={light}>
            <MIChart ref={ref} options={{}} />
        </ThemeProvider>);
        expect(chart.off).toHaveBeenCalledWith('click', latestClick);
        expect(chart.hideLoading).toHaveBeenCalled();
        view.unmount();
        expect(disconnect).toHaveBeenCalledTimes(1);
        expect(chart.dispose).toHaveBeenCalledTimes(1);
        window.dispatchEvent(new Event('resize'));
        expect(chart.resize).toHaveBeenCalledTimes(1);
    });

    it('disconnects without creating a chart when unmounted while hidden', () => {
        const view = render(<ThemeProvider theme={light}><MIChart options={{}} /></ThemeProvider>);
        view.unmount();
        expect(disconnect).toHaveBeenCalledTimes(1);
        expect(echarts.init).not.toHaveBeenCalled();
        expect(chart.dispose).not.toHaveBeenCalled();
    });
});

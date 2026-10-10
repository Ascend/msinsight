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
import * as testingLibrary from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import ResizeObserver from 'resize-observer-polyfill';
import { getInstanceByDom } from 'echarts';
import { disposeAdaptiveEchart, getAdaptiveEchart } from '@insight/lib/utils';
import { light } from '../../../../lib/src/theme/light';
import CommunicationMatrix from '../communication/CommunicationMatrix';
import { defaultCondition } from '../communication/Filter';
import { queryCommunicationMatrix } from '../../utils/RequestUtils';

jest.mock('resize-observer-polyfill', () => jest.fn());
jest.mock('echarts', () => ({ ...jest.requireActual('echarts'), getInstanceByDom: jest.fn() }));
jest.mock('@insight/lib/utils', () => ({
    ...jest.requireActual('@insight/lib/utils'),
    getAdaptiveEchart: jest.fn(),
    disposeAdaptiveEchart: jest.fn(),
}));
jest.mock('../../utils/RequestUtils', () => ({ queryCommunicationMatrix: jest.fn() }));

const { act, fireEvent, render, waitFor } = testingLibrary;

it('defers a hidden matrix, renders loaded data and retains its range when shown again', async() => {
    const callbacks = new Map<Element, () => void>();
    const disconnect = jest.fn();
    (ResizeObserver as jest.Mock).mockImplementation((callback) => ({
        observe: (target: Element): void => { callbacks.set(target, callback); },
        disconnect,
    }));
    const chart = { setOption: jest.fn(), on: jest.fn(), resize: jest.fn() };
    (getAdaptiveEchart as jest.Mock).mockReturnValue(chart);
    (getInstanceByDom as jest.Mock).mockReturnValue(chart);
    const matrixData = (bandwidth: number): object => {
        const data = { opName: 'allReduce', bandwidth, transitSize: 1, transitTime: 1, transportType: 'HCCS' };
        return { compare: data, baseline: data, diff: data };
    };
    (queryCommunicationMatrix as jest.Mock).mockResolvedValue({
        matrixList: [
            { srcRank: 0, dstRank: 1, matrixData: matrixData(2) },
            { srcRank: 1, dstRank: 0, matrixData: matrixData(4) },
        ],
    });
    session.selectedClusterPath = 'cluster';
    session.clusterList = [{ name: 'cluster', path: 'cluster', parsed: true, durationParsed: true }];
    const conditions = { ...defaultCondition, stage: '(0,1)', operatorName: 'allReduce' };
    const view = render(<ThemeProvider theme={light}>
        <CommunicationMatrix isShow conditions={conditions} session={session} />
    </ThemeProvider>);
    await waitFor(() => expect(queryCommunicationMatrix).toHaveBeenCalledTimes(1));
    const dom = view.container.querySelector('#matrixchart') as HTMLDivElement;
    const notifyResize = callbacks.get(dom) as () => void;
    expect(getAdaptiveEchart).not.toHaveBeenCalled();
    Object.defineProperties(dom, {
        clientWidth: { configurable: true, value: 800 },
        clientHeight: { configurable: true, value: 800 },
    });
    act(() => notifyResize());
    await waitFor(() => expect(chart.setOption).toHaveBeenCalledWith(expect.objectContaining({
        series: [expect.objectContaining({
            data: expect.arrayContaining([
                expect.arrayContaining(['0', '1', 2]), expect.arrayContaining(['1', '0', 4]),
            ]),
        })],
    }), expect.anything()));

    const minInput = view.getByTestId('communicationMatrixMinRangeInput');
    fireEvent.change(minInput, { target: { value: '3' } });
    fireEvent.click(view.getByRole('button', { name: 'Confirm' }));
    expect(chart.setOption.mock.calls.at(-1)?.[0].series[0].data).toHaveLength(1);
    const filteredOptions = chart.setOption.mock.calls.at(-1)?.[0];
    const renderCount = chart.setOption.mock.calls.length;
    Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 0 });
    act(() => notifyResize());
    expect(chart.setOption).toHaveBeenCalledTimes(renderCount);
    Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 1000 });
    act(() => notifyResize());
    expect(chart.setOption).toHaveBeenCalledTimes(renderCount);
    expect(chart.setOption.mock.calls.at(-1)?.[0].series[0].data).toEqual(filteredOptions.series[0].data);

    Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 0 });
    act(() => notifyResize());
    (queryCommunicationMatrix as jest.Mock).mockResolvedValue({
        matrixList: [
            { srcRank: 0, dstRank: 1, matrixData: matrixData(5) },
        ],
    });
    view.rerender(<ThemeProvider theme={light}>
        <CommunicationMatrix isShow conditions={{ ...conditions, iterationId: '2' }} session={session} />
    </ThemeProvider>);
    await waitFor(() => expect(queryCommunicationMatrix).toHaveBeenCalledTimes(2));
    expect(chart.setOption).toHaveBeenCalledTimes(renderCount);
    Object.defineProperty(dom, 'clientWidth', { configurable: true, value: 1000 });
    act(() => notifyResize());
    expect(chart.setOption.mock.calls.at(-1)?.[0].series[0].data[0][2]).toBe(5);
    view.unmount();
    expect(disconnect).toHaveBeenCalled();
    expect(disposeAdaptiveEchart).toHaveBeenLastCalledWith(dom);
});

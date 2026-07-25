import React from 'react';
import { act, render } from '@testing-library/react';
import CommunicationTimeAnalysisChart, { AnalysisChartData } from '../communication/CommunicationTimeAnalysisChart';
import { useEventBus } from '../../utils/eventBus';

const mockRefValues: Array<{ current: unknown }> = [];

jest.mock('react', () => {
    const actual = jest.requireActual('react');
    const useRef = (initialValue: unknown): { current: unknown } => mockRefValues.shift() ?? actual.useRef(initialValue);
    const react = { ...actual, useRef };
    return { __esModule: true, ...react, default: react };
});
jest.mock('mobx-react-lite', () => ({ observer: (component: React.ComponentType) => component }));
jest.mock('../../utils/eventBus', () => ({ useEventBus: jest.fn() }));
jest.mock('../../connection', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('@insight/lib/theme', () => ({ themeInstance: { getThemeType: () => 'light' } }), { virtual: true });

it('downplays a highlighted slow operator after five seconds', () => {
    jest.useFakeTimers();
    const chart = {
        getOption: jest.fn(() => ({ yAxis: [{ data: [] }] })),
        dispatchAction: jest.fn(),
    };
    mockRefValues.push({ current: null }, { current: chart });

    let slowRankHandler: ((params: unknown) => void) | undefined;
    (useEventBus as jest.Mock).mockImplementation((event, handler) => {
        if (event === 'onClickSlowRankOp') {
            slowRankHandler = handler;
        }
    });

    render(<CommunicationTimeAnalysisChart
        dataSource={{ minTime: 0, maxTime: 0, data: [] } as AnalysisChartData}
        session={global.session}
        loading={false}
    />);

    act(() => slowRankHandler?.({ startValue: 0, endValue: 1, rankId: 0, name: 'Send' }));
    expect(chart.dispatchAction).toHaveBeenCalledWith({
        type: 'highlight',
        seriesIndex: 0,
        name: '0-Send',
    });

    act(() => jest.advanceTimersByTime(5000));
    expect(chart.dispatchAction).toHaveBeenLastCalledWith({
        type: 'downplay',
        seriesIndex: 0,
        name: '0-Send',
    });
});

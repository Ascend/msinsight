import React from 'react';
import { act, render } from '@testing-library/react';
import CommunicationTimeAnalysisChart, {
    AnalysisChartData,
    consumeCommunicationChartZoomData,
    getEChartsTooltipStyle,
    getRankDataZoomRange,
    initWebGLDataZoom,
} from '../communication/CommunicationTimeAnalysisChart';
import { useEventBus } from '../../utils/eventBus';
import * as insightUtils from '@insight/lib/utils';

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
jest.mock('@insight/lib/theme', () => ({
    themeInstance: {
        getThemeType: () => ({ colorPalette: new Proxy({}, { get: () => '#000' }) }),
    },
}), { virtual: true });
jest.mock('@insight/lib/utils', () => ({
    ...jest.requireActual('@insight/lib/utils'),
    disposeAdaptiveEchart: jest.fn(),
    getAdaptiveEchart: jest.fn(),
}));
jest.mock('../communication/CommunicationTimeWebGLRenderer', () => ({
    ...jest.requireActual('../communication/CommunicationTimeWebGLRenderer'),
    createCommunicationTimeWebGLRenderer: jest.fn(() => null),
}));

it('uses the same visual defaults as the ECharts HTML tooltip', () => {
    expect(getEChartsTooltipStyle()).toEqual({
        background: '#fff',
        color: '#666',
        boxShadow: '1px 2px 10px rgba(0, 0, 0, .2)',
        borderRadius: '4px',
        borderStyle: 'solid',
        borderWidth: '1px',
        padding: '10px',
        fontFamily: '\'Inter\', -apple-system, BlinkMacSystemFont, \'Segoe UI\', Roboto, Oxygen, Ubuntu, Cantarell, \'Fira Sans\', \'Droid Sans\', sans-serif',
        fontSize: '14px',
        lineHeight: '21px',
    });
});

it('starts WebGL charts with the full rank and time range', () => {
    const chartOption = {
        dataZoom: [
            { start: 0, end: 10 },
            { start: 90, end: 100 },
        ],
    };

    initWebGLDataZoom(chartOption);

    expect(chartOption.dataZoom).toEqual([
        { start: 0, end: 100 },
        { start: 0, end: 100 },
    ]);
});

it('restores a saved horizontal range without limiting the WebGL rank range', () => {
    const chartOption = {
        dataZoom: [
            { start: 0, end: 100 },
            { start: 90, end: 100 },
        ],
    };

    initWebGLDataZoom(chartOption, { start: 20, end: 40 });

    expect(chartOption.dataZoom).toEqual([
        { start: 20, end: 40 },
        { start: 0, end: 100 },
    ]);
});

it('keeps the saved horizontal range until the chart update consumes it', () => {
    const session = { communicationChartZoomData: { start: 20, end: 40 } };
    const update = jest.fn();

    consumeCommunicationChartZoomData(session, update);

    expect(update).toHaveBeenCalledWith({ start: 20, end: 40 });
    expect(session.communicationChartZoomData).toBeUndefined();
});

it('uses string rank values when positioning within ten thousand ranks', () => {
    const rankIds = Array.from({ length: 10000 }, (_, index) => `${index}`);

    expect(getRankDataZoomRange(rankIds, '5050')).toEqual({
        startValue: '5040',
        endValue: '5060',
    });
});

it('uses the latest operator data when a hidden chart initializes after becoming visible', () => {
    jest.useFakeTimers();
    const originalResizeObserver = global.ResizeObserver;
    let resizeCallback: ResizeObserverCallback = () => {};
    global.ResizeObserver = class {
        constructor(callback: ResizeObserverCallback) {
            resizeCallback = callback;
        }

        observe(): void {}
        unobserve(): void {}
        disconnect(): void {}
    } as unknown as typeof ResizeObserver;
    const chart = {
        on: jest.fn(),
        off: jest.fn(),
        getZr: jest.fn(() => ({ on: jest.fn(), off: jest.fn() })),
        isDisposed: jest.fn(() => false),
        setOption: jest.fn(),
        resize: jest.fn(),
    };
    const getAdaptiveEchart = insightUtils.getAdaptiveEchart as jest.Mock;
    const disposeAdaptiveEchart = insightUtils.disposeAdaptiveEchart as jest.Mock;
    getAdaptiveEchart.mockReturnValue(chart);
    global.session.selectedClusterPath = 'cluster';
    global.session.clusterList = [{
        name: 'cluster',
        path: 'cluster',
        parsed: true,
        durationParsed: true,
    }];
    const emptyData = { minTime: 0, maxTime: 0, data: [] } as AnalysisChartData;
    const operatorData = {
        minTime: 0,
        maxTime: 100,
        data: [{
            rankId: '0',
            dbPath: 'rank-0.db',
            lists: {
                compare: [{ operatorName: 'AllReduce', startTime: 0, duration: 100 }],
                baseline: [],
            },
        }],
    } as AnalysisChartData;

    const { container, rerender } = render(<CommunicationTimeAnalysisChart
        dataSource={emptyData}
        session={global.session}
        loading={false}
    />);
    rerender(<CommunicationTimeAnalysisChart
        dataSource={operatorData}
        session={global.session}
        loading={false}
    />);
    const chartDom = container.querySelector('#hccl') as HTMLDivElement;
    Object.defineProperties(chartDom, {
        clientHeight: { configurable: true, value: 460 },
        clientWidth: { configurable: true, value: 1000 },
    });

    act(() => {
        resizeCallback([], {} as ResizeObserver);
        jest.advanceTimersByTime(20);
    });

    expect(getAdaptiveEchart).toHaveBeenCalledTimes(1);
    expect(chart.setOption).toHaveBeenCalledWith(expect.objectContaining({
        yAxis: expect.objectContaining({ data: ['0'] }),
    }), { notMerge: true });

    getAdaptiveEchart.mockReset();
    disposeAdaptiveEchart.mockReset();
    global.ResizeObserver = originalResizeObserver;
});

it('targets both zoom axes when locating a slow operator with a string rank ID', () => {
    jest.useFakeTimers();
    const rankIds = Array.from({ length: 10000 }, (_, index) => `${index}`);
    const chart = {
        getOption: jest.fn(() => ({ yAxis: [{ data: rankIds }] })),
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

    act(() => slowRankHandler?.({ startValue: 10, endValue: 20, rankId: '5050', name: 'Send' }));
    expect(chart.dispatchAction).toHaveBeenNthCalledWith(1, {
        type: 'dataZoom',
        dataZoomIndex: 0,
        startValue: 10,
        endValue: 20,
    });
    expect(chart.dispatchAction).toHaveBeenNthCalledWith(2, {
        type: 'dataZoom',
        dataZoomIndex: 1,
        startValue: '5040',
        endValue: '5060',
    });
});

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

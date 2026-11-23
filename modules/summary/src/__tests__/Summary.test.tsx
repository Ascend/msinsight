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
import '@testing-library/jest-dom';
import { act, render, fireEvent } from '@testing-library/react';
import * as testing from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { runInAction } from 'mobx';
import Summary from '../index';
import { OverviewSession, connectOverviewContext } from '../session';
import { chartOptions, sortRanks } from '../charts';
import { parseSummaryResponse, type RankTime } from '../api';
import { light } from '../../../lib/src/theme/light';
import { dark } from '../../../lib/src/theme/dark';
const { screen, waitFor } = testing;
const time = { computingNotOverlapped: 2, communicationNotOverlapped: 3, computingCommunicationOverlapped: 4, free: 5, preparing: 1 };
const responses: Record<string, unknown> = {
    'summary/queryTopData': {
        baseInfo: {
            compare: { rankCount: 16, stepNum: 100, stepList: ['1', '2'], collectStartTime: new Date(2024, 9, 29, 14, 30).getTime(), collectDuration: 2722800000 },
        },
    },
    'summary/clusterTime': { units: { time: 'us' }, time },
    'summary/rankTime': { units: { time: 'us', ratio: 'percent' }, ranks: [{ rankId: '0', time: { ...time, total: 15, computing: 6, communication: 7 }, ratio: { compute: 40, communication: 46 } }] },
    'summary/communicationTime': { units: { time: 'us' }, ranks: [{ rankId: '0', time: { transit: 1, wait: 2, synchronization: 3, free: 4 } }] },
    'summary/clusterBandwidth': { units: { bandwidth: 'GB/s' }, transportTypes: [{ transportType: 'HCCS', bandwidth: 12.5 }] },
};
jest.mock('@insight/lib/components', () => ({
    FormItem: jest.requireActual('../../../lib/src/components/MIFormItem').MIFormItem,
    Tooltip: jest.requireActual('../../../lib/src/components/MITooltip').MITooltip,
    Select: ({ options, onChange, ...props }: { options: Array<{ value: string; label: string }>; onChange: (value: string) => void }) =>
        <select {...props} onChange={event => onChange(event.target.value)}>{options.map(option => <option key={option.value} value={option.value}>{option.label}</option>)}</select>,
    MIChart: ({ options }: { options: unknown }) => <output>{JSON.stringify(options)}</output>,
    Spin: () => <span>loading</span>,
    Empty: ({ description }: { description: string }) => <span>{description}</span>,
    Button: ({ children, onClick }: { children: React.ReactNode; onClick: () => void }) => <button onClick={onClick}>{children}</button>,
}));
function session(ready = true): OverviewSession {
    const value = new OverviewSession();
    value.selectedProjectName = 'project';
    value.updateClusters({ selectedClusterPath: '/a', clusterList: ['a', 'b'].map(name => ({ name, path: `/${name}`, parsed: ready, durationParsed: ready })) });
    return value;
}
beforeEach(() => { window.requestData = jest.fn(async command => responses[command]); });
it.each(['parsed', 'durationParsed'] as const)('waits for both stages (%s first), loads metadata and requests the selected Step', async first => {
    const state = session(false);
    render(<Summary session={state} />);
    act(() => { runInAction(() => { state.clusterList[0][first] = true; }); });
    expect(window.requestData).not.toHaveBeenCalled();
    await act(async () => { runInAction(() => { state.clusterList[0][first === 'parsed' ? 'durationParsed' : 'parsed'] = true; }); });
    expect(window.requestData).toHaveBeenCalledTimes(5);
    expect(screen.getAllByRole('status')).toHaveLength(4);
    for (const value of ['2024-10-29 14:30:00', '45.38', '16', '100']) expect(screen.getByText(value)).toBeInTheDocument();
    expect(screen.getAllByRole('definition')).toHaveLength(4);
    fireEvent.change(screen.getByRole('combobox', { name: 'Step' }), { target: { value: '2' } });
    await waitFor(() => expect(window.requestData).toHaveBeenCalledTimes(9));
    for (const command of Object.keys(responses).filter(key => key !== 'summary/queryTopData')) {
        expect(window.requestData).toHaveBeenCalledWith(command, { clusterPath: '/a', step: '2' }, undefined, { signal: expect.any(AbortSignal) });
    }
    act(() => { runInAction(() => { state.clusterList[0][first] = false; }); });
    expect(screen.queryAllByRole('status')).toHaveLength(0);
    expect(window.requestData).toHaveBeenCalledTimes(9);
    expect(screen.queryByRole('note')).not.toBeInTheDocument();
});
it('cancels waits and ignores late metadata/chart replies on cluster change and unmount', async () => {
    const pending: Array<{ command: string; signal?: AbortSignal; resolve: (value: unknown) => void }> = [];
    window.requestData = jest.fn((command, _params, _module, options) => new Promise(resolve => pending.push({ command, signal: options?.signal, resolve })));
    const state = session();
    const view = render(<Summary session={state} />);
    act(() => state.selectCluster('/b'));
    expect(pending).toHaveLength(10);
    expect(pending.slice(0, 5).every(item => item.signal?.aborted)).toBe(true);
    await act(async () => { pending.slice(5).forEach(item => item.resolve(responses[item.command])); });
    const current = screen.getAllByRole('status').map(chart => chart.textContent);
    await act(async () => { pending.slice(0, 5).forEach(item => item.resolve({ invalid: true })); });
    expect(state.stepList).toEqual(['1', '2']);
    expect(screen.getAllByRole('status').map(chart => chart.textContent)).toEqual(current);
    act(() => { runInAction(() => { state.summaryStep = '1'; }); });
    expect(pending.slice(5, 10).filter(item => item.command !== 'summary/queryTopData').every(item => item.signal?.aborted)).toBe(true);
    view.unmount();
    expect(pending.every(item => item.signal?.aborted)).toBe(true);
});
it('isolates backend and payload errors, retries one panel and distinguishes empty data from zero', async () => {
    const request = jest.fn(async command => {
        if (command === 'summary/rankTime') throw new Error('offline');
        return command === 'summary/clusterBandwidth' ? { units: { bandwidth: 'GB/s' }, transportTypes: [] } : responses[command];
    });
    window.requestData = request;
    render(<Summary session={session()} />);
    await waitFor(() => expect(screen.getByRole('alert')).toBeInTheDocument());
    expect(screen.getByText('No data')).toBeInTheDocument();
    expect(screen.getAllByRole('status')).toHaveLength(2);
    request.mockResolvedValueOnce({ invalid: true });
    fireEvent.click(screen.getByRole('button', { name: 'Retry' }));
    await waitFor(() => expect(request).toHaveBeenCalledTimes(6));
    await waitFor(() => expect(screen.getByRole('alert')).toBeInTheDocument());
    request.mockResolvedValueOnce(responses['summary/rankTime']);
    fireEvent.click(screen.getByRole('button', { name: 'Retry' }));
    await waitFor(() => expect(screen.getAllByRole('status')).toHaveLength(3));
    expect(request).toHaveBeenCalledTimes(7);
});
it('reports only user selections, interpolates parsing labels and switches locale', async () => {
    const state = session(false);
    const onClusterChange = jest.fn();
    render(<Summary session={state} onClusterChange={onClusterChange} />);
    expect(screen.getByRole('option', { name: 'b (parsing)' })).toBeInTheDocument();
    fireEvent.change(screen.getByRole('combobox', { name: 'Cluster' }), { target: { value: '/b' } });
    expect(onClusterChange).toHaveBeenCalledWith('/b');
    act(() => { state.updateClusters({ clusterList: state.clusterList, selectedClusterPath: '/a' }); });
    expect(onClusterChange).toHaveBeenCalledTimes(1);
    act(() => { runInAction(() => { state.language = 'zhCN'; }); });
    expect(screen.getByRole('combobox', { name: '\u96c6\u7fa4' })).toHaveValue('/a');
    expect(screen.getByRole('option', { name: 'a\uff08\u89e3\u6790\u4e2d\uff09' })).toBeInTheDocument();
    expect(screen.getByRole('heading', { name: '\u96c6\u7fa4\u8017\u65f6\u5206\u6790' })).toBeInTheDocument();
});
it.each([[light, 'enUS', '#E4EAF1', 'Time metric definitions', 'Total Computing ='], [dark, 'zhCN', '#576372', '\u8017\u65f6\u6307\u6807\u8bf4\u660e', '\u8ba1\u7b97\u65f6\u95f4 =']] as const)(
    'preserves layout, readable borders and keyboard help in $0.mode', async (theme, locale, border, helpName, definition) => {
        const state = session(false);
        state.language = locale;
        render(<ThemeProvider theme={theme}><Summary session={state} /></ThemeProvider>);
        const page = screen.getByRole('main');
        const contentBackground = theme.mode === 'dark' ? theme.bgColor : theme.bgColorLighter;
        expect(page).toHaveStyle({ backgroundColor: theme.bgColorDark, padding: '16px' });
        expect(page.firstElementChild).toHaveStyle({ backgroundColor: contentBackground, gap: '12px', minWidth: '0' });
        const definitions = screen.getAllByRole('definition');
        expect(definitions[0].closest('dl')).toHaveStyle({ border: `1px solid ${border}` });
        expect(definitions[1].parentElement).toHaveStyle({ borderLeft: `1px solid ${border}` });
        const surface = definitions[0].closest('section')?.parentElement;
        expect(surface).toHaveStyle({ backgroundColor: contentBackground, borderRadius: '8px', padding: '16px' });
        expect(surface).not.toContainElement(screen.getAllByRole('combobox')[0]);
        screen.getAllByRole('heading').forEach(heading => expect(heading).toHaveStyle({ fontSize: '16px', color: theme.textColorPrimary }));
        const help = screen.getByRole('button', { name: helpName });
        expect(help.closest('h3')).toHaveStyle({ lineHeight: '24px', alignItems: 'center' });
        expect(help).toHaveStyle({ alignSelf: 'center', justifyContent: 'center', lineHeight: '0' });
        expect(help.querySelector('svg')).toHaveAttribute('width', '20');
        fireEvent.focus(help);
        const tooltip = await screen.findByRole('tooltip');
        expect(tooltip).toHaveTextContent(definition);
        expect(tooltip).toHaveTextContent('Overlap Analysis');
        expect(tooltip).toHaveStyle({ color: theme.textColorPrimary });
        const plots = screen.getAllByRole('heading');
        [plots[2], plots[3]].forEach(heading => expect(heading.parentElement?.querySelector('select')).toHaveStyle({ fontSize: '14px' }));
        expect(plots[3].parentElement).toHaveStyle({ paddingRight: '32px', boxSizing: 'border-box' });
        expect(plots[2].parentElement).toHaveStyle({ paddingRight: '0' });
    },
);

const rankTime = { preparing: 10, computing: 60, computingNotOverlapped: 40, communication: 40, communicationNotOverlapped: 20, computingCommunicationOverlapped: 20, free: 10 };
const rank = (rankId: string, total: number): RankTime => ({ rankId, time: { ...rankTime, total }, ratio: { compute: 60, communication: 40 } });
const units = { time: 'us', ratio: 'percent' as const };
const label = (key: string): string => key;

it('keeps exclusive segments, backend ratios and unclipped axis labels without a zoom slider', () => {
    const options = chartOptions('rankTime', { units, ranks: [rank('0', 100)] }, label, 'rank');
    const series = options.series as Array<{ type: string; data: number[]; yAxisIndex?: number }>;
    expect(series.filter(item => item.type === 'bar').map(item => item.data[0])).toEqual([40, 20, 20, 10, 10]);
    expect(series.filter(item => item.type === 'line').map(item => [item.data[0], item.yAxisIndex])).toEqual([[60, 1], [40, 1]]);
    expect(options.xAxis).toMatchObject({ nameLocation: 'middle', nameGap: 32 });
    expect(options.grid).toMatchObject({ bottom: 52 });
    expect(options.dataZoom).toEqual([]);
});
it('sorts ranks naturally or by descending total without mutating data', () => {
    const ranks = [rank('10', 200), rank('2', 100), rank('1', 300)];
    expect(sortRanks(ranks, 'rank').map(item => item.rankId)).toEqual(['1', '2', '10']);
    expect(sortRanks(ranks, 'total').map(item => item.rankId)).toEqual(['1', '10', '2']);
    expect(ranks.map(item => item.rankId)).toEqual(['10', '2', '1']);
});
it('counts synchronization in communication totals and preserves bandwidth values and units', () => {
    const options = chartOptions('communicationTime', {
        units,
        ranks: [
            { rankId: '0', time: { transit: 1, wait: 2, synchronization: 100, free: 3 } },
            { rankId: '1', time: { transit: 4, wait: 5, synchronization: 6, free: 7 } },
        ],
    }, label, 'total');
    expect(options.xAxis).toMatchObject({ data: ['0', '1'], nameLocation: 'middle', nameGap: 32 });
    expect((options.series as Array<{ data: number[] }>).map(item => item.data[0])).toEqual([1, 2, 100, 3]);
    const bandwidth = chartOptions('clusterBandwidth', {
        units: { bandwidth: 'GB/s' },
        transportTypes: [{ transportType: 'HCCS', bandwidth: 12.5 }],
    }, label, 'rank');
    expect(bandwidth.series).toMatchObject([{ data: [12.5] }]);
    expect(bandwidth.yAxis).toMatchObject({ name: 'Bandwidth (GB/s)' });
});
it('formats pie values and percentages to three decimal places without changing data', () => {
    const time = { preparing: 1, computingNotOverlapped: 110668.08196699999, communicationNotOverlapped: 0, computingCommunicationOverlapped: 0, free: 0 };
    const options = chartOptions('clusterTime', { units: { time: 'ms' }, time }, label, 'rank');
    expect(options).toMatchObject({ xAxis: { show: false }, yAxis: { show: false } });
    expect(options.series).toMatchObject([{ label: { textBorderWidth: 0 } }]);
    const tooltip = options.tooltip as { valueFormatter: (value: number) => string };
    expect(tooltip.valueFormatter(time.computingNotOverlapped)).toBe('110668.082 ms');
    expect(tooltip.valueFormatter(0)).toBe('0.000 ms');
    const series = (options.series as Array<{ percentPrecision: number; label: { formatter: (params: { percent: number }) => string }; data: Array<{ value: number }> }>)[0];
    expect(series.percentPrecision).toBe(3);
    expect(series.label.formatter({ percent: 34.31056 })).toBe('34.311%');
    expect(series.label.formatter({ percent: 0 })).toBe('0.000%');
    expect(series.data[0].value).toBe(time.computingNotOverlapped);
    expect(options.title).toMatchObject({ text: (110669.082).toLocaleString(undefined, { minimumFractionDigits: 3, maximumFractionDigits: 3 }) });
});
it('formats all Cartesian tooltips and numeric axes but preserves rank IDs and precision', () => {
    const row = rank('2', 100.00049);
    row.ratio.compute = 55.78706471792149;
    const charts = [
        chartOptions('rankTime', { units, ranks: [row] }, label, 'rank'),
        chartOptions('communicationTime', { units, ranks: [{ rankId: '2', time: { transit: 1, wait: 2, synchronization: 3, free: 4 } }] }, label, 'rank'),
        chartOptions('clusterBandwidth', { units: { bandwidth: 'GB/s' }, transportTypes: [{ transportType: 'HCCS', bandwidth: 24.123456 }] }, label, 'rank'),
    ];
    for (const options of charts) {
        const tooltip = options.tooltip as { valueFormatter: (value: number) => string };
        expect(tooltip.valueFormatter(12.123456)).toBe('12.123');
        expect(tooltip.valueFormatter(0)).toBe('0.000');
        const axis = (Array.isArray(options.yAxis) ? options.yAxis[0] : options.yAxis) as { axisLabel: { formatter: (value: number) => string } };
        expect(axis.axisLabel.formatter(25)).toBe('25.000');
    }
    const ratioAxis = (charts[0].yAxis as Array<{ axisLabel: { formatter: (value: number) => string } }>)[1];
    expect(ratioAxis.axisLabel.formatter(80)).toBe('80.000%');
    const line = (charts[0].series as Array<{ type: string; data: number[]; tooltip: { valueFormatter: (value: number) => string } }>).find(series => series.type === 'line');
    expect(line?.tooltip.valueFormatter(row.ratio.compute)).toBe('55.787%');
    expect(line?.data).toEqual([55.78706471792149]);
    expect(charts[0].xAxis).toMatchObject({ data: ['2'] });
    expect(charts[2].series).toMatchObject([{ data: [24.123456] }]);
});
it.each([-1, NaN, Infinity, undefined])('rejects invalid bandwidth %s', bandwidth => {
    expect(() => parseSummaryResponse('clusterBandwidth', {
        units: { bandwidth: 'GB/s' },
        transportTypes: [{ transportType: 'HCCS', bandwidth }],
    })).toThrow();
});
it.each([
    [[], true], [[rank('0', 0)], true], [[rank('0', 100), rank('0', 100)], false],
    [[{ ...rank('0', 100), ratio: { compute: 101, communication: 40 } }], false],
    [[{ rankId: '0', time: { transit: 1 } }], false],
])('validates empty, zero, duplicate and malformed ranks: %j', (ranks, valid) => {
    const parse = (): unknown => parseSummaryResponse('rankTime', { units, ranks });
    if (valid) expect(parse).not.toThrow();
    else expect(parse).toThrow();
});

const context = { selectedClusterPath: '/a', clusterList: ['a', 'b'].map(name => ({ name, path: `/${name}`, parsed: true, durationParsed: true })) };
function connect(session: OverviewSession): {
    receive: (event: string, body: unknown) => void;
    dispose: () => void;
    listeners: Map<string, (message: MessageEvent) => void>;
    theme: jest.Mock;
    language: jest.Mock;
    bridge: { send: jest.Mock };
} {
    const listeners = new Map<string, (message: MessageEvent) => void>();
    const theme = jest.fn();
    const language = jest.fn();
    const bridge = {
        addListener: (event: string, callback: (message: MessageEvent) => void) => { listeners.set(event, callback); return { event, sequence: 0 }; },
        removeListener: ({ event }: { event: string }) => { listeners.delete(event); },
        send: jest.fn(() => {
            for (const event of ['frame/loaded', 'switchLanguage', 'setTheme', 'clusterBaselineToggle', 'module.reset']) expect(listeners.has(event)).toBe(true);
        }),
    };
    const dispose = connectOverviewContext(session, bridge, theme, language);
    const receive = (event: string, body: unknown): void => { listeners.get(event)?.(new MessageEvent('message', { data: { body } })); };
    return { listeners, theme, language, bridge, dispose, receive };
}
it('initializes before ready, follows host context idempotently and releases listeners', () => {
    const session = new OverviewSession();
    const { bridge, receive, dispose, listeners, theme, language } = connect(session);
    expect(bridge.send).toHaveBeenCalledWith({ event: 'clusterPage/ready', body: { version: 'modern' } });
    const directory = { selectedProjectName: 'one', pageInfo: { cluster: context } };
    receive('frame/loaded', directory);
    const revision = session.renderId;
    runInAction(() => { session.summaryStep = '2'; });
    receive('frame/loaded', directory);
    expect([session.renderId, session.summaryStep, session.ready]).toEqual([revision, '2', true]);
    receive('updateSession', { clusterPageInfo: { selectedClusterPath: '/b' } });
    expect([session.selectedClusterPath, session.summaryStep]).toEqual(['/b', 'All']);
    receive('updateSession', { parseCompleted: false });
    receive('updateClusterPageInfo', { clusterList: null });
    expect(session.ready).toBe(true);
    receive('updateSession', { clusterPageInfo: { clusterList: context.clusterList.map(cluster => ({ ...cluster, durationParsed: false })) } });
    expect(session.ready).toBe(false);
    expect(context.clusterList[1].durationParsed).toBe(true);
    receive('updateClusterPageInfo', context);
    receive('setTheme', { isDark: false });
    receive('switchLanguage', { lang: 'zhCN' });
    expect(theme).toHaveBeenCalledWith(false);
    expect(language).toHaveBeenCalledWith('zhCN');
    expect(session.language).toBe('zhCN');
    receive('clusterBaselineToggle', { status: true });
    expect(session.ready).toBe(false);
    receive('clusterBaselineToggle', { status: false });
    expect(session.ready).toBe(true);
    receive('switchDirectory', { selectedProjectName: 'two' });
    expect([session.selectedProjectName, session.selectedClusterPath, session.clusterList]).toEqual(['two', '', []]);
    expect(bridge.send).toHaveBeenCalledTimes(1);
    dispose();
    expect(listeners.size).toBe(0);
});
it.each(['remote/remove', 'remote/reset', 'module.reset'])('clears stale context on %s', event => {
    const session = new OverviewSession();
    const { receive, dispose } = connect(session);
    receive('frame/loaded', { selectedProjectName: 'project', pageInfo: { cluster: context } });
    runInAction(() => { session.summaryStep = '2'; session.stepList = ['2']; });
    receive(event, {});
    expect([session.selectedProjectName, session.selectedClusterPath, session.stepList, session.summaryStep, session.ready]).toEqual(['', '', [], 'All', false]);
    dispose();
});

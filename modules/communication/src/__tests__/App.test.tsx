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
import { act, fireEvent, render } from '@testing-library/react';
import * as testing from '@testing-library/react';
import { App } from '../app/App';
import { CommunicationStore } from '../store/CommunicationStore';
import { HostBridge } from '../bridge/HostBridge';
import { CommunicationApi } from '../api';
import { context, request as fixture } from '../dev/fixture';
import type { OperatorTimeline } from '../features/operatorTimeline';
import type { HostContext } from '../domain/types';
import type { EChartsOption } from 'echarts';
const { screen, waitFor } = testing;
const mockChartRender = jest.fn();
jest.mock('../components/Chart', () => {
    const react = jest.requireActual<typeof import('react')>('react');
    return {
        Chart: react.forwardRef(function Chart({ label, option }: { label: string; option: EChartsOption }, _ref) {
            mockChartRender(label, option);
            return <div role="img" aria-label={label} />;
        }),
    };
});
jest.mock('../features/operatorTimeline', () => ({
    OperatorTimeline: ({ highlight, onAlign, aligned, onLocate, data }: React.ComponentProps<typeof OperatorTimeline>) => <div>
        <span>Timeline rendered</span>
        <output aria-label="timeline highlight">{JSON.stringify(highlight) ?? ''}</output>
        <output aria-label="timeline alignment">{aligned}</output>
        <button onClick={() => onAlign('allReduce_0')}>Align operator</button>
        <button onClick={() => onLocate(data.rows[0], data.rows[0].operators[0])}>Locate operator</button>
    </div>,
}));
function setup(initial: Partial<HostContext> = context): { bridge: HostBridge; store: CommunicationStore; request: jest.Mock } {
    const store = new CommunicationStore(); store.updateHost(initial); const bridge = new HostBridge(store); const request = jest.fn(fixture);
    jest.spyOn(window.parent, 'postMessage').mockImplementation(() => {});
    render(<App services={{ store, bridge, api: new CommunicationApi(request) }} />);
    return { bridge, store, request };
}
async function select(input: HTMLElement, label: string): Promise<void> {
    fireEvent.mouseDown(input.closest('.ant-select')?.querySelector('.ant-select-selector') as Element);
    const option = await screen.findAllByText(label); fireEvent.click(option.find(node => node.classList.contains('ant-select-item-option-content')) as Element);
}

it.each(['context first', 'appearance first'])('does not flash an empty selection before initialization (%s)', order => {
    const { request } = setup({});
    const send = (event: string, body: object): void => {
        act(() => window.dispatchEvent(new MessageEvent('message', {
            source: window.parent, origin: window.location.origin, data: JSON.stringify({ event, body }),
        })));
    };
    const messages: Array<[string, object]> = [
        ['frame/loaded', { selectedProjectName: 'p', pageInfo: { cluster: { selectedClusterPath: '/cluster', clusterList: [{ name: 'cluster', path: '/cluster', parsed: false, durationParsed: false }] } } }],
        ['setTheme', { isDark: false }],
        ['switchLanguage', { lang: 'zhCN' }],
    ];
    if (order === 'appearance first') { messages.reverse(); }
    send('updateSession', { unitcount: 3 });
    for (const [event, body] of messages) {
        expect(screen.getByRole('status')).toBeInTheDocument();
        expect(document.querySelector('.ant-empty')).toBeNull();
        expect(screen.queryByRole('combobox')).not.toBeInTheDocument();
        expect(request).not.toHaveBeenCalled();
        send(event, body);
    }
    expect(screen.getByRole('combobox', { name: '集群' })).toBeInTheDocument();
    expect(screen.getByText('正在解析集群数据')).toBeInTheDocument();
    expect(document.querySelector('main')).toHaveAttribute('data-theme', 'light');
    expect(document.querySelector('.ant-empty')).toBeNull();
    expect(request).not.toHaveBeenCalled();
    send('updateClusterPageInfo', { selectedClusterPath: '', clusterList: [] });
    expect(screen.getByText('请选择集群数据')).toBeInTheDocument();
});

it('switches shared labels and new chart titles together when the host changes language', async () => {
    const { request } = setup();
    // The initial render waits for iteration, group and chart requests in sequence.
    await screen.findByText('Timeline rendered', {}, { timeout: 5000 });
    expect(screen.getByRole('combobox', { name: '通信域' })).toBeInTheDocument();
    expect(screen.getByRole('heading', { name: '极差比分析' })).toBeInTheDocument();
    for (const [axis, metric, suffix] of [
        ['极差比', '极差比', ''], ['带宽（GB/s）', '带宽', ' GB/s'], ['耗时（ms）', '耗时', ' ms'],
    ]) {
        await screen.findByRole('img', { name: axis });
        const option = mockChartRender.mock.calls.find(([label]) => label === axis)?.[1];
        expect(option).toMatchObject({
            yAxis: { name: axis, nameLocation: 'end', nameTextStyle: { align: 'center' } },
            series: [{ name: metric }], legend: { show: false },
        });
        expect(option.tooltip.valueFormatter(null)).toBe('-');
        expect(option.tooltip.valueFormatter(0)).toBe(`0.000000${suffix}`);
        expect(option.tooltip.valueFormatter(70.9885437)).toBe(`70.988544${suffix}`);
    }
    const count = request.mock.calls.length;
    act(() => window.dispatchEvent(new MessageEvent('message', {
        source: window.parent,
        origin: window.location.origin,
        data: JSON.stringify({ event: 'switchLanguage', body: { lang: 'enUS' } }),
    })));
    expect(await screen.findByRole('combobox', { name: 'Communication Group' })).toBeInTheDocument();
    expect(screen.getByRole('heading', { name: 'Range ratio' })).toBeInTheDocument();
    expect(screen.getByRole('img', { name: 'Range ratio' })).toBeInTheDocument();
    expect(screen.getByRole('img', { name: 'Bandwidth (GB/s)' })).toBeInTheDocument();
    expect(screen.getByRole('img', { name: 'Elapsed time (ms)' })).toBeInTheDocument();
    expect(mockChartRender).toHaveBeenCalledWith('Elapsed time (ms)', expect.objectContaining({
        series: [expect.objectContaining({ name: 'Elapsed time' })],
    }));
    expect(request).toHaveBeenCalledTimes(count);
});
it('keeps local operator filters independent and resets them on a public iteration change', async () => {
    const { request } = setup(); await screen.findByText('Timeline rendered');
    const operators = screen.getAllByRole('combobox', { name: '算子名称' });
    const before = request.mock.calls.filter(call => call[0] === 'communication/matrix/bandwidthInfo').length;
    await select(operators[0], 'allReduce_0');
    await waitFor(() => expect(request.mock.calls.some(call => call[0] === 'communication/operatorLists' && call[1].operatorName === 'allReduce_0')).toBe(true));
    expect(request.mock.calls.filter(call => call[0] === 'communication/matrix/bandwidthInfo')).toHaveLength(before);
    await select(screen.getByRole('combobox', { name: '迭代ID' }), '2');
    await waitFor(() => expect(request.mock.calls.some(call => call[0] === 'communication/operatorLists' && call[1].iterationId === '2' && call[1].operatorName === 'Total Op Info')).toBe(true));
});
it('uses the iteration after the cluster selector for overview and scoped charts, and keeps transport local', async () => {
    const { request } = setup();
    const iteration = await screen.findByRole('combobox', { name: '迭代ID' });
    await waitFor(() => expect(iteration).toBeEnabled());
    const cluster = screen.getByRole('combobox', { name: '集群' });
    expect(cluster.closest('.ant-select')).not.toHaveClass('ant-select-borderless');
    expect(cluster.closest('.ant-select')).toHaveStyle({ width: 'auto', minWidth: '160px' });
    expect(cluster.closest('label')?.nextElementSibling).toBe(iteration.closest('label'));
    const overview = screen.getByRole('heading', { name: '通信域性能波动分析' }).closest('section') as HTMLElement;
    expect(request.mock.calls.filter(call => call[0] === 'communication/duration/iterations')).toHaveLength(1);
    await select(iteration, '2');
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/group/rangeRatio').pop()?.[1]).toEqual({ clusterPath: '/fixture/cluster', iterationId: '2' }));
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/operator/fluctuation').pop()?.[1]).toMatchObject({ iterationId: '2', groupName: '2-0' }));
    await testing.within(overview).findByRole('img', { name: '带宽（GB/s）' });
    expect(request.mock.calls.filter(call => call[0] === 'communication/group/bandwidth').pop()?.[1]).toMatchObject({ iterationId: '2', transportType: 'SDMA' });
    expect(request.mock.calls.filter(call => call[0] === 'communication/advisor')).toHaveLength(1);
    const scopeCalls = request.mock.calls.filter(call => call[0] === 'communication/operator/fluctuation').length;
    const rangeCalls = request.mock.calls.filter(call => call[0] === 'communication/group/rangeRatio').length;
    await select(testing.within(overview).getByRole('combobox', { name: '带宽类型' }), 'HCCS');
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/group/bandwidth').pop()?.[1]).toEqual({ clusterPath: '/fixture/cluster', iterationId: '2', transportType: 'HCCS' }));
    expect(request.mock.calls.filter(call => call[0] === 'communication/group/rangeRatio')).toHaveLength(rangeCalls);
    expect(request.mock.calls.filter(call => call[0] === 'communication/operator/fluctuation')).toHaveLength(scopeCalls);
});
it('resets rank pagination and sorting when the public iteration or group changes', async () => {
    const { request } = setup();
    const rankSection = (await screen.findByRole('heading', { name: '通信时间与带宽分析' }, { timeout: 5000 })).closest('section') as HTMLElement;
    await testing.within(rankSection).findAllByRole('button', { name: '带宽分析' });
    fireEvent.click(rankSection.querySelector('.ant-pagination-next button') as Element);
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/rank/statistics').pop()?.[1].currentPage).toBe(2));
    await select(screen.getByRole('combobox', { name: '迭代ID' }), '2');
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/rank/statistics').pop()?.[1]).toMatchObject({ iterationId: '2', groupName: '2-0', currentPage: 1, orderBy: 'rankId', order: 'ascend' }));
    await screen.findAllByRole('button', { name: '带宽分析' });
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/rank/statistics').pop()?.[1].orderBy).toBe('bandwidth'));
    const groupInput = screen.getByRole('combobox', { name: '通信域' });
    fireEvent.mouseDown(groupInput.closest('.ant-select')!.querySelector('.ant-select-selector')!);
    fireEvent.change(groupInput, { target: { value: 'group_name_1' } });
    const option = await screen.findByText('group_name_1 · tp');
    expect(option.closest('.ant-select-item-option-content')).toHaveTextContent(/^group_name_1 · tp$/);
    fireEvent.mouseEnter(option);
    expect(await screen.findByRole('tooltip')).toHaveTextContent(`(${Array.from({ length: 32 }, (_, rank) => rank).join(', ')})`);
    fireEvent.click(option);
    expect(groupInput.closest('.ant-select')!.querySelector('.ant-select-selection-item')).toHaveTextContent(/^group_name_1 · tp$/);
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/rank/statistics').pop()?.[1]).toMatchObject({ iterationId: '2', groupName: '2-1', currentPage: 1, orderBy: 'rankId', order: 'ascend' }));
}, 15000);
it('makes the whole page inert while the drawer is open and invalidates it on host reset', async () => {
    const { store } = setup();
    const buttons = await screen.findAllByRole('button', { name: '带宽分析' }); fireEvent.click(buttons[0]);
    await screen.findByRole('dialog'); expect(document.querySelector('main')).toHaveAttribute('inert');
    await act(async () => store.resetHost());
    await waitFor(() => expect(screen.queryByRole('dialog')).not.toBeInTheDocument());
    expect(document.querySelector('main')).not.toHaveAttribute('inert');
});
it('allows main-page interaction again after closing the drawer', async () => {
    setup(); const buttons = await screen.findAllByRole('button', { name: '带宽分析' }); buttons[0].focus(); fireEvent.click(buttons[0]);
    const dialog = await screen.findByRole('dialog'); fireEvent.click(dialog.querySelector('.ant-drawer-close') as Element);
    await waitFor(() => expect(document.querySelector('main')).not.toHaveAttribute('inert'));
    expect(buttons[0]).toHaveFocus();
});

it('shows the fluctuation chart while duration is parsing and loads the slow-rank panel when ready', async () => {
    const cluster = { path: '/cluster', name: 'cluster', parsed: true, durationParsed: false };
    const { store, request } = setup({ ...context, clusterPath: cluster.path, clusters: [cluster] });
    const chart = await screen.findByLabelText('耗时（ms）', {}, { timeout: 5000 });
    const section = testing.within(chart.closest('section') as HTMLElement);
    const requestCount = (name: string): number => request.mock.calls.filter(([command]) => command === name).length;

    expect(requestCount('communication/duration/slow-rank/list')).toBe(0);
    expect(section.queryByText('暂无数据')).not.toBeInTheDocument();
    expect(section.getByText('数据解析中')).toBeInTheDocument();

    act(() => store.updateHost({ clusters: [{ ...cluster, durationParsed: true }] }));
    expect(await section.findByRole('heading', { name: '专家建议' })).toBeInTheDocument();
    expect(requestCount('communication/duration/slow-rank/list')).toBe(1);
    expect(section.queryByText('数据解析中')).not.toBeInTheDocument();
    expect(requestCount('communication/operator/fluctuation')).toBe(1);
});

it('keeps the slow-rank summary without expandable operator rows and preserves independent alignment', async () => {
    const { request } = setup();
    await screen.findByText('Timeline rendered');
    const fluctuation = (await screen.findByText('算子性能波动分析')).closest('section') as HTMLElement;
    await testing.within(fluctuation).findAllByText('90.343912');
    expect(testing.within(fluctuation).queryByText('可能的慢卡列表')).not.toBeInTheDocument();
    expect(fluctuation.querySelector('.ant-table-row-expand-icon-cell')).toBeNull();
    expect(testing.within(fluctuation).queryByRole('button', { name: '在图中高亮' })).not.toBeInTheDocument();
    const matrixCalls = request.mock.calls.filter(call => call[0] === 'communication/matrix/bandwidthInfo').length;
    await select(screen.getAllByRole('combobox', { name: '算子名称' })[0], 'allReduce_0');
    fireEvent.click(await screen.findByRole('button', { name: 'Align operator' }));
    await waitFor(() => expect(screen.getByLabelText('timeline alignment')).toHaveTextContent('allReduce_0'));
    await select(screen.getAllByRole('combobox', { name: '算子名称' })[0], 'Total Op Info');
    await waitFor(() => expect(screen.getByLabelText('timeline alignment')).toBeEmptyDOMElement());
    expect(request.mock.calls.filter(call => call[0] === 'communication/matrix/bandwidthInfo')).toHaveLength(matrixCalls);
});

it('preserves Timeline navigation from the communication timeline section', async () => {
    const { bridge, request } = setup();
    const send = jest.spyOn(bridge, 'send');
    await screen.findByText('Timeline rendered');
    fireEvent.click(screen.getByRole('button', { name: 'Locate operator' }));
    const selected = { rankId: '0', dbPath: '/fixture/rank-0.db', name: 'allReduce_0', clusterPath: '/fixture/cluster' };
    expect(request).toHaveBeenCalledWith('unit/kernelDetail', selected, expect.any(AbortSignal), 'timeline');
    await waitFor(() => expect(send).toHaveBeenCalledWith('switchModule', {
        switchTo: 'timeline',
        toModuleEvent: 'locateUnit',
        params: { ...selected, pid: '100', processId: '100', startTime: 3065000000, duration: 7000000, showSelectedData: true },
    }));
});

it('scrolls to the thumbnail on each Timeline jump and preserves the scroll position on manual filter changes', async () => {
    const observe = jest.fn();
    const disconnect = jest.fn();
    type NotifyVisibility = (entries: Array<{ isIntersecting: boolean; intersectionRect: { height: number } }>) => void;
    const Observer = jest.fn((_notify: NotifyVisibility) => ({ observe, disconnect }));
    Object.defineProperty(window, 'IntersectionObserver', { configurable: true, value: Observer });
    const { store, request } = setup();
    await screen.findByText('Timeline rendered');
    expect(observe).not.toHaveBeenCalled();
    const jumpFromTimeline = (): void => {
        act(() => window.dispatchEvent(new MessageEvent('message', {
            source: window.parent,
            origin: window.location.origin,
            data: JSON.stringify({ event: 'locateCommunication', body: { iterationId: '2', groupIdHash: '2-1', operatorName: 'allGather_2' } }),
        })));
    };
    const setVisible = (visible: boolean): void => {
        const [notify] = Observer.mock.calls[Observer.mock.calls.length - 1];
        notify([{ isIntersecting: visible, intersectionRect: { height: visible ? 480 : 0 } }]);
    };
    jumpFromTimeline();
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/operatorLists').pop()?.[1]).toMatchObject({
        iterationId: '2', groupIdHash: '2-1', operatorName: 'allGather_2',
    }));
    expect(store.group?.pgName).toBe('tp');
    const section = (await screen.findByText('Timeline rendered')).closest('section') as HTMLElement;
    section.scrollIntoView = jest.fn();
    expect(observe).toHaveBeenCalledWith(document.documentElement);
    setVisible(false);
    expect(section.scrollIntoView).not.toHaveBeenCalled();
    setVisible(true);
    expect(section.scrollIntoView).toHaveBeenCalledWith({ block: 'start', inline: 'nearest' });
    expect(disconnect).toHaveBeenCalled();
    jumpFromTimeline();
    expect(observe).toHaveBeenCalledTimes(2);
    setVisible(true);
    expect(section.scrollIntoView).toHaveBeenCalledTimes(2);
    await select(screen.getByRole('combobox', { name: '迭代ID' }), '1');
    await waitFor(() => expect(request.mock.calls.filter(call => call[0] === 'communication/operatorLists').pop()?.[1]).toMatchObject({
        iterationId: '1', operatorName: 'Total Op Info',
    }));
    expect(store.locationIntent).toBeUndefined();
    expect(observe).toHaveBeenCalledTimes(2);
    Reflect.deleteProperty(window, 'IntersectionObserver');
});

it('does not redraw unrelated charts or refetch data when Timeline availability changes', async () => {
    const { store, request } = setup();
    await screen.findByText('Timeline rendered');
    await screen.findByRole('img', { name: '通信矩阵图' });
    await screen.findByLabelText('耗时（ms）');
    const chartRenders = mockChartRender.mock.calls.length;
    const requests = request.mock.calls.length;
    const scope = store.scope;
    act(() => store.updateHost({ unitCount: 0 }));
    expect(mockChartRender).toHaveBeenCalledTimes(chartRenders);
    expect(request).toHaveBeenCalledTimes(requests);
    expect(store.scope).toBe(scope);
});

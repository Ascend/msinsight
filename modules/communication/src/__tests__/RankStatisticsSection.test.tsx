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
import { ThemeProvider } from '@emotion/react';
import { SharedConfigProvider } from '@insight/lib';
import { themeInstance } from '@insight/lib/theme';
import i18n from '@insight/lib/i18n';
import { CommunicationApi, type CommunicationRequest } from '../api';
import { request as fixture } from '../dev/fixture';
import { RankStatisticsSection } from '../sections/RankStatisticsSection';
import { CommunicationStore } from '../store/CommunicationStore';
import { CommunicationStoreProvider } from '../store/context';

const { screen, waitFor, within } = testing;
jest.mock('../components/Chart', () => ({
    Chart: ({ label }: { label: string }) => <div role="img" aria-label={label} />,
}));
const scope = { clusterPath: '/cluster', iterationId: '013', group: { key: 'hash:', stage: '(0, 1)', groupIdHash: '3985310405473756128', pgName: 'dp', label: 'dp' } };
function setup(request: CommunicationRequest = fixture, store = new CommunicationStore()): jest.Mock {
    const send = jest.fn(request); const api = new CommunicationApi(send);
    void i18n.changeLanguage('zhCN');
    render(<ThemeProvider theme={themeInstance.getTheme().light}><SharedConfigProvider locale="zhCN">
        <CommunicationStoreProvider value={store}><RankStatisticsSection scope={scope} api={api} /></CommunicationStoreProvider>
    </SharedConfigProvider></ThemeProvider>);
    return send;
}
async function ready(): Promise<HTMLElement> {
    await screen.findAllByRole('button', { name: '带宽分析' });
    await waitFor(() => expect(screen.getByRole('table').closest('[aria-busy]')).toHaveAttribute('aria-busy', 'false'));
    return screen.getByRole('table');
}
const lastParams = (request: jest.Mock): Record<string, unknown> => request.mock.calls[request.mock.calls.length - 1][1];

it('requests server pages using total and keeps zero distinct from absent bandwidth', async () => {
    const request = setup(); const table = await ready();
    expect(table.querySelectorAll('tbody tr.ant-table-row')).toHaveLength(20);
    const cells = within(table.querySelector('tr[data-row-key="0"]') as HTMLElement).getAllByRole('cell');
    for (const type of ['RDMA', 'SDMA', 'HCCS', 'PCIE', 'SIO']) {
        expect(within(table).getByRole('columnheader', { name: `${type} 带宽 (GB)` })).toBeInTheDocument();
    }
    expect(cells.map(cell => cell.textContent).slice(0, 6)).toEqual(['0', '18.5000', '21.0000', '15.0000', '-', '0.0000']);
    fireEvent.click(document.querySelector('.ant-pagination-next button') as Element);
    await waitFor(() => expect(lastParams(request)).toMatchObject({ currentPage: 2, pageSize: 20, orderBy: 'rankId', order: 'ascend' }));
    const second = await ready();
    expect(second.querySelectorAll('tbody tr.ant-table-row')).toHaveLength(12);
    expect(second.querySelector('tr[data-row-key="20"]')).toBeInTheDocument();
    expect(second.querySelector('tr[data-row-key="0"]')).toBeNull();
});

it('resets to page one for server sorting, cancellation and page-size changes', async () => {
    const request = setup(); await ready();
    fireEvent.click(document.querySelector('.ant-pagination-next button') as Element);
    await waitFor(() => expect(lastParams(request).currentPage).toBe(2)); await ready();
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(lastParams(request)).toMatchObject({ currentPage: 1, orderBy: 'bandwidth', bandType: 'RDMA', order: 'ascend' })); await ready();
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(lastParams(request).order).toBe('descend'));
    const descending = await ready();
    expect(descending.querySelector('tbody tr.ant-table-row')).toHaveAttribute('data-row-key', '3');
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(lastParams(request)).toMatchObject({ orderBy: 'rankId', order: 'ascend' }));
    expect(lastParams(request)).not.toHaveProperty('bandType'); await ready();
    fireEvent.click(document.querySelector('.ant-pagination-next button') as Element);
    await waitFor(() => expect(lastParams(request).currentPage).toBe(2)); await ready();
    fireEvent.mouseDown(document.querySelector('.ant-pagination-options .ant-select-selector') as Element);
    const options = await screen.findAllByText(/^50/);
    fireEvent.click(options.find(node => node.classList.contains('ant-select-item-option-content')) as Element);
    await waitFor(() => expect(lastParams(request)).toMatchObject({ currentPage: 1, pageSize: 50 }));
    expect((await ready()).querySelectorAll('tbody tr.ant-table-row')).toHaveLength(32);
}, 15000);

it('keeps pagination available when items is empty but total is nonzero', async () => {
    setup(async () => ({ items: [], total: 100, currentPage: 1, pageSize: 20 }));
    await screen.findByRole('table');
    expect(document.querySelector('.ant-pagination-next')).not.toHaveClass('ant-pagination-disabled');
});

it.each([false, true])('opens packet charts directly without operator requests when durationParsed is %s', async durationParsed => {
    const store = new CommunicationStore();
    store.updateHost({ clusterPath: scope.clusterPath, clusters: [{ path: scope.clusterPath, name: 'cluster', parsed: true, durationParsed }] });
    const request = setup(fixture, store);
    const table = await ready();
    fireEvent.click(within(table.querySelector('tr[data-row-key="0"]') as HTMLElement).getByRole('button', { name: '带宽分析' }));
    const dialog = await screen.findByRole('dialog');
    expect(await within(dialog).findByRole('img', { name: 'HCCS 通信包分析' })).toBeInTheDocument();
    expect(await within(dialog).findByRole('img', { name: 'SIO 通信包分析' })).toBeInTheDocument();
    await within(dialog).findByRole('table');
    const packets = request.mock.calls.filter(([command]) => command === 'communication/distribution');
    expect(packets.map(([, params]) => params.transportType)).toEqual(['HCCS', 'PCIE', 'RDMA', 'SIO', 'UB']);
    for (const [, params] of packets) {
        expect(params).toEqual({ clusterPath: '/cluster', iterationId: '013', stage: '(0, 1)', pgName: 'dp', groupIdHash: '3985310405473756128', rankId: '0', operatorName: 'Total Op Info', transportType: expect.any(String) });
    }
    expect(request.mock.calls.filter(([command]) => command === 'communication/bandwidth')).toHaveLength(1);
    expect(request.mock.calls).toHaveLength(7); // One rank page, five packet types and one bandwidth summary.
});

it('formats parent and child transfer measurements to four decimals in the drawer', async () => {
    const measurements = { transitSize: 12, transitTime: 2.5, bandwidth: 1.234567 };
    setup(async (command, params, signal) => command === 'communication/bandwidth' ? { items: [
        { ...measurements, transportType: 'SDMA', largePacketRatio: 1 },
        { ...measurements, transportType: 'HCCS', largePacketRatio: 0 },
        { ...measurements, transportType: 'SIO', largePacketRatio: null },
    ] } : fixture(command, params, signal));
    const table = await ready();
    fireEvent.click(within(table).getAllByRole('button', { name: '带宽分析' })[0]);
    const dialog = await screen.findByRole('dialog');
    const transfers = await within(dialog).findByRole('table');
    for (const [type, ratio] of [['SDMA', '/'], ['HCCS', '0.0000'], ['SIO', '/']]) {
        const row = await within(transfers).findByRole('row', { name: new RegExp(type) });
        expect(within(row).getAllByRole('cell').slice(1).map(cell => cell.textContent))
            .toEqual(['12.0000', '2.5000', '1.2346', ratio]);
    }
});

it('retains the table, rows and pagination while sorting waits for the server', async () => {
    let finish: (() => Promise<void>) | undefined;
    const request = setup(async (command, params, signal) => {
        if (params.orderBy === 'rankId') { return fixture(command, params, signal); }
        return new Promise(resolve => { finish = async () => resolve(await fixture(command, params, signal)); });
    });
    const table = await ready();
    const pagination = document.querySelector('.ant-pagination');
    const originalRows = Array.from(table.querySelectorAll('tbody tr.ant-table-row'));
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(lastParams(request).orderBy).toBe('bandwidth'));
    await waitFor(() => expect(document.querySelector('.ant-spin-spinning')).toBeInTheDocument());
    expect(screen.getByRole('table')).toBe(table);
    expect(table.closest('[aria-busy]')).toHaveAttribute('aria-busy', 'true');
    expect(Array.from(table.querySelectorAll('tbody tr.ant-table-row'))).toEqual(originalRows);
    expect(document.querySelector('.ant-pagination')).toBe(pagination);
    await act(async () => { await finish?.(); });
    expect(await ready()).toBe(table);
    expect(table.querySelectorAll('tbody tr.ant-table-row')[1]).toHaveAttribute('data-row-key', '4');
});

it('keeps the previous table on a sorting failure and retries the requested sort', async () => {
    let rejectSort: ((error: Error) => void) | undefined;
    let failing = true;
    const request = setup(async (command, params, signal) => {
        if (params.orderBy === 'bandwidth' && failing) { return new Promise((resolve, reject) => { rejectSort = reject; }); }
        return fixture(command, params, signal);
    });
    const table = await ready();
    fireEvent.click(screen.getByRole('columnheader', { name: /RDMA/ }));
    await waitFor(() => expect(rejectSort).toBeDefined());
    await act(async () => { rejectSort?.(Object.assign(new Error('query failed'), { code: 3110 })); });
    await screen.findByText('数据加载失败');
    expect(screen.getByRole('table')).toBe(table);
    expect(table.querySelectorAll('tbody tr.ant-table-row')).toHaveLength(20);
    failing = false;
    fireEvent.click(screen.getByRole('button', { name: /重.*试/ }));
    expect(screen.getByRole('table')).toBe(table);
    expect(await ready()).toBe(table);
    expect(lastParams(request)).toMatchObject({ currentPage: 1, orderBy: 'bandwidth', bandType: 'RDMA', order: 'ascend' });
    expect(table.querySelectorAll('tbody tr.ant-table-row')[1]).toHaveAttribute('data-row-key', '4');
    expect(screen.queryByText('数据加载失败')).not.toBeInTheDocument();
});

it.each([[3108, '当前数据不支持该通信统计能力'], [3109, '通信统计数据异常'], [3110, '数据加载失败']])('shows error %s instead of an empty table and allows retry', async (code, message) => {
    let failing = true;
    const request = setup(async (...args) => {
        if (failing) { throw Object.assign(new Error('backend failure'), { code }); }
        return fixture(...args);
    });
    await screen.findByText(message);
    expect(screen.queryByRole('table')).not.toBeInTheDocument();
    failing = false; fireEvent.click(screen.getByRole('button', { name: /重.*试/ }));
    await ready(); expect(request).toHaveBeenCalledTimes(2);
});

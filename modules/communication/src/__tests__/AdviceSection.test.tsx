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
import { themeInstance } from '@insight/lib/theme';
import { CommunicationApi, type CommunicationRequest } from '../api';
import { AdviceSection } from '../sections/AdviceSection';
import { CommunicationStore } from '../store/CommunicationStore';
import { CommunicationStoreProvider } from '../store/context';
import i18n from '@insight/lib/i18n';

const { screen, waitFor } = testing;
const group = { key: 'a', stage: '(0, 1)', groupIdHash: 'a', pgName: 'dp', label: 'dp' };
const diagnostics = [
    { name: 'Bandwidth Contention Analysis', statistics: { name: ['allReduce_0'], rankId: ['0'], 'bandwidth(GB/s)': ['12.5'] } },
    { name: 'Byte Alignment Analysis', statistics: { name: ['<operator>'], rankId: ['1'] } },
    { name: 'Communication Retransmission Analysis', statistics: { name: [], 'group name': [] } },
    { name: 'Packet Analysis', statistics: { Category: ['SDMA', 'RDMA'], 'Small Size Proportion(%)': ['5.2', '0'] } },
];
function setup(request: CommunicationRequest, language: 'zh' | 'en' = 'zh'): CommunicationStore {
    const store = new CommunicationStore();
    store.updateHost({ clusterPath: '/first', clusters: [{ path: '/first', name: 'first', parsed: true, durationParsed: true }] });
    store.selectIteration('1'); store.selectGroup(group);
    void i18n.changeLanguage(language === 'zh' ? 'zhCN' : 'enUS');
    render(<ThemeProvider theme={themeInstance.getTheme().light}>
        <CommunicationStoreProvider value={store}><AdviceSection api={new CommunicationApi(request)} /></CommunicationStoreProvider>
    </ThemeProvider>);
    return store;
}

it('renders only cluster diagnostics without requesting duration statistics', async () => {
    const request = jest.fn(async () => ({ items: diagnostics }));
    setup(request);
    await screen.findByText('带宽抢占分析');
    expect(screen.getByText('字节对齐分析')).toBeInTheDocument();
    expect(screen.getByText('通信重传分析')).toBeInTheDocument();
    expect(screen.getByText('小包分析')).toBeInTheDocument();
    expect(await screen.findByText('12.5')).toBeInTheDocument();
    expect(screen.getByText('<operator>')).toBeInTheDocument();
    expect(screen.getByText('暂未发现问题算子')).toBeInTheDocument();
    expect(screen.getByText(/检查交换机和计算节点服务器的网络配置/)).toBeInTheDocument();
    expect(screen.getByText(/使用亲和优化器或融合算子可以减少通信算子的数量/)).toBeInTheDocument();
    expect(screen.queryByText('当前通信域统计')).not.toBeInTheDocument();
    expect(request).toHaveBeenCalledTimes(1);
    expect(request).toHaveBeenCalledWith('communication/advisor', { clusterPath: '/first' }, expect.any(AbortSignal));
});

it('does not request on iteration/group changes and refreshes diagnostics for a cluster change', async () => {
    const request = jest.fn(async () => ({ items: diagnostics }));
    const store = setup(request);
    await screen.findByText('带宽抢占分析');
    act(() => { store.selectIteration('2'); store.selectGroup({ ...group, key: 'b', groupIdHash: 'b' }); });
    expect(request).toHaveBeenCalledTimes(1);
    act(() => store.updateHost({ clusterPath: '/second' }));
    await waitFor(() => expect(request).toHaveBeenCalledTimes(2));
    expect(request).toHaveBeenLastCalledWith('communication/advisor', { clusterPath: '/second' }, expect.any(AbortSignal));
});

it('keeps retry visible when diagnostics fail and retries only diagnostics', async () => {
    let failing = true;
    const request = jest.fn(async () => {
        if (failing) { throw new Error('Unavailable'); }
        return { items: diagnostics };
    });
    setup(request);
    await screen.findByText('数据加载失败');
    expect(screen.queryByText('当前通信域统计')).not.toBeInTheDocument();
    failing = false;
    fireEvent.click(screen.getByRole('button', { name: /重.*试/ }));
    await screen.findByText('带宽抢占分析');
    expect(request).toHaveBeenCalledTimes(2);
    expect(request).toHaveBeenLastCalledWith('communication/advisor', { clusterPath: '/first' }, expect.any(AbortSignal));
});

it('hides the module when cluster diagnostics are empty', async () => {
    setup(async () => ({ items: [] }));
    await waitFor(() => expect(screen.queryByText('专家建议')).not.toBeInTheDocument());
});

it('preserves cluster diagnostics without a selected group and localizes headings and recommendations', async () => {
    const store = setup(async () => ({ items: diagnostics }), 'en');
    act(() => store.selectIteration('2'));
    await screen.findByText('Byte Alignment Analysis');
    expect(screen.getAllByText('Recommendation:')).toHaveLength(4);
    expect(await screen.findByText('5.2')).toBeInTheDocument();
});

it('ignores diagnostics returned late for the previously selected cluster', async () => {
    let resolveOld: (value: unknown) => void = () => {};
    const request: CommunicationRequest = jest.fn(async (command, params) => {
        return params.clusterPath === '/first' ? new Promise(resolve => { resolveOld = resolve; }) : { items: [] };
    });
    const store = setup(request);
    await waitFor(() => expect(request).toHaveBeenCalledWith('communication/advisor', { clusterPath: '/first' }, expect.any(AbortSignal)));
    act(() => store.updateHost({ clusterPath: '/second' }));
    await waitFor(() => expect(screen.queryByText('专家建议')).not.toBeInTheDocument());
    await act(async () => resolveOld({ items: diagnostics }));
    expect(screen.queryByText('带宽抢占分析')).not.toBeInTheDocument();
});

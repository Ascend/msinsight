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
import i18n from '@insight/lib/i18n';
import { CommunicationApi, type CommunicationRequest } from '../../../api';
import { PacketAnalysis } from '../PacketAnalysis';

const { screen, waitFor } = testing;
jest.mock('../../../components/Chart', () => ({
    Chart: ({ label }: { label: string }) => <div role="img" aria-label={label} />,
}));
const scope = { clusterPath: '/cluster', iterationId: '1', group: { key: 'group', stage: '(0, 1)', groupIdHash: 'hash', pgName: 'dp', label: 'dp' } };
function setup(request: CommunicationRequest, language = 'zhCN'): jest.Mock {
    const send = jest.fn(request);
    void i18n.changeLanguage(language);
    render(<ThemeProvider theme={themeInstance.getTheme().light}>
        <PacketAnalysis api={new CommunicationApi(send)} scope={scope} rankId="0" dark={false} />
    </ThemeProvider>);
    return send;
}

it.each(['', '{}', 'null', null, undefined])('shows one empty state when all packet responses are %s', async distributionData => {
    const request = setup(async () => ({ distributionData }));
    expect(await screen.findByText('暂无数据')).toBeInTheDocument();
    expect(screen.getAllByText('暂无数据')).toHaveLength(1);
    expect(screen.queryByRole('img', { name: /通信包分析/ })).not.toBeInTheDocument();
    expect(request).toHaveBeenCalledTimes(5);
});

it('shows available charts while hiding absent transport types', async () => {
    setup(async (_command, params) => ({ distributionData: params.transportType === 'HCCS' ? '{"2":[3,4]}' : '' }));
    expect(await screen.findByRole('img', { name: 'HCCS 通信包分析' })).toBeInTheDocument();
    await waitFor(() => expect(screen.queryByRole('status')).not.toBeInTheDocument());
    expect(screen.queryByText('暂无数据')).not.toBeInTheDocument();
    expect(screen.queryByRole('img', { name: 'RDMA 通信包分析' })).not.toBeInTheDocument();
});

it('waits for the final transport before showing the overall empty state', async () => {
    let finish: ((value: unknown) => void) | undefined;
    setup(async (_command, params) => params.transportType === 'RDMA'
        ? new Promise(resolve => { finish = resolve; })
        : { distributionData: '' });
    await waitFor(() => expect(finish).toBeDefined());
    expect(screen.queryByText('暂无数据')).not.toBeInTheDocument();
    expect(screen.getByRole('status')).toBeInTheDocument();
    await act(async () => { finish?.({ distributionData: '' }); });
    expect(await screen.findByText('暂无数据')).toBeInTheDocument();
});

it('preserves a failed transport and retries only that request before showing empty', async () => {
    let failing = true;
    const request = setup(async (_command, params) => {
        if (params.transportType === 'RDMA' && failing) { throw new Error('Unavailable'); }
        return { distributionData: '' };
    });
    expect(await screen.findByText('数据加载失败')).toBeInTheDocument();
    expect(screen.queryByText('暂无数据')).not.toBeInTheDocument();
    failing = false;
    fireEvent.click(screen.getByRole('button', { name: /重.*试/ }));
    expect(await screen.findByText('暂无数据')).toBeInTheDocument();
    expect(screen.queryByText('数据加载失败')).not.toBeInTheDocument();
    expect(request).toHaveBeenCalledTimes(6);
    expect(request.mock.calls[5][1].transportType).toBe('RDMA');
});

it('localizes the overall empty state', async () => {
    setup(async () => ({ distributionData: '' }), 'enUS');
    expect(await screen.findByText('No Data')).toBeInTheDocument();
    expect(screen.queryByText('暂无数据')).not.toBeInTheDocument();
});

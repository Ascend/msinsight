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
import connector, { registerEventListeners } from './index';
import { clusterView } from '../cluster/viewVersion';
import { store } from '../store';
import type { Session } from '../entity/session';
import { requestModule } from '../centralServer/server';
import { GLOBAL_HOST } from '../centralServer/websocket/defs';
import i18n from '@insight/lib/i18n';
import { message } from 'antd';

jest.mock('@insight/lib/connection', () => ({ ServerConnector: jest.fn(() => ({ addListener: jest.fn(), registerAwaitFetch: jest.fn() })) }));
jest.mock('./interceptor', () => ({ INTERCEPTOR_HANDLERS: {}, NOTIFICATION_INTERCEPTOR_HANDLERS: {}, NOTIFICATION_STATISTIC_HANDLERS: {} }));
jest.mock('./notification', () => ({ listenerMap: {} }));
jest.mock('@/store', () => ({ store: { sessionStore: {} } }));
jest.mock('@/centralServer/server', () => ({ requestModule: jest.fn() }));
jest.mock('@/cluster/viewVersion', () => ({ ...jest.requireActual('../cluster/viewVersion'), clusterView: { versionFor: jest.fn() } }));
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { t: jest.fn() } }));
jest.mock('antd', () => ({ message: { warning: jest.fn() } }));

it.each([
    ['Summary', true, 'modern', true],
    ['Communication', true, 'modern', true],
    ['Timeline', true, 'modern', true],
    ['Communication', false, 'modern', true],
    ['Communication', true, 'legacy', true],
    ['Communication', true, 'modern', false],
])('routes %s comparison=%s version=%s session=%s', async (from, isCompare, version, hasSession) => {
    const session = hasSession ? { activeDataSource: { projectName: 'fixture' } } : undefined;
    store.sessionStore.activeSession = session as Session;
    (clusterView.versionFor as jest.Mock).mockReturnValue(version);
    (i18n.t as unknown as jest.Mock).mockReturnValue('请切回旧版进行集群对比');
    (requestModule as jest.Mock).mockResolvedValue({ ok: true });
    registerEventListeners();
    const handler = (connector.registerAwaitFetch as jest.Mock).mock.calls[0][0] as (event: unknown) => Promise<unknown>;
    const args = { command: 'communication/test', params: { isCompare } };
    const result = await handler({ data: { from, args, module: 'communication' } });
    const blocked = hasSession && from !== 'Timeline' && isCompare && version === 'modern';
    expect(message.warning).toHaveBeenCalledTimes(blocked ? 1 : 0);
    expect(requestModule).toHaveBeenCalledTimes(blocked ? 0 : 1);
    if (blocked) {
        expect(i18n.t).toHaveBeenCalledWith('clusterView.compareUnavailable', { ns: 'framework' });
        expect(message.warning).toHaveBeenCalledWith('请切回旧版进行集群对比');
        expect(result).toEqual({ dataSource: session?.activeDataSource, body: { error: { code: 'MODERN_CLUSTER_COMPARISON_UNAVAILABLE', message: '请切回旧版进行集群对比' } } });
    } else {
        expect(requestModule).toHaveBeenCalledWith('communication', args);
        expect(result).toEqual({ dataSource: session?.activeDataSource ?? GLOBAL_HOST, body: { ok: true } });
    }
    if (!hasSession) { expect(clusterView.versionFor).not.toHaveBeenCalled(); }
});

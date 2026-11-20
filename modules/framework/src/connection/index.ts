/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import { clusterView, isClusterApplication } from '@/cluster/viewVersion';
import { ServerConnector } from '@insight/lib/connection';
import { INTERCEPTOR_HANDLERS, NOTIFICATION_INTERCEPTOR_HANDLERS, NOTIFICATION_STATISTIC_HANDLERS } from './interceptor';
import { listenerMap } from './notification';
import { store } from '@/store';
import { requestModule } from '@/centralServer/server';
import { GLOBAL_HOST } from '@/centralServer/websocket/defs';
import { message } from 'antd';
import i18n from '@insight/lib/i18n';

type TargetWindow = Window;

const pluginEvents = ['remote/import', 'remote/reset', 'remote/remove', 'setTheme', 'wakeupPlugin', 'language', 'switchLanguage'];

const connector = new ServerConnector({
    getTargetWindow: (): TargetWindow[] => {
        const res: TargetWindow[] = [];
        document?.querySelectorAll('iframe')?.forEach((item) => item.contentWindow && res.push(item.contentWindow));
        return res;
    },
    getInterceptorHandlers: (command: string): (...args: any[]) => void => INTERCEPTOR_HANDLERS[command] as any,
    sendBefore: (origin: any): any => {
        const message = origin;
        if (pluginEvents.includes(message?.event as string)) {
            message.target = 'plugin';
        }
        if (NOTIFICATION_INTERCEPTOR_HANDLERS[message?.event] !== undefined) {
            NOTIFICATION_INTERCEPTOR_HANDLERS[message.event](message.body);
        }
        if (NOTIFICATION_STATISTIC_HANDLERS[message?.event] !== undefined) {
            NOTIFICATION_STATISTIC_HANDLERS[message.event](message.body);
        }
        return message;
    },
});

export default connector;

export function registerEventListeners(): void {
    // 接收模块消息
    Object.entries(listenerMap).forEach(([event, callback]) => {
        connector.addListener(event, callback);
    });

    // 转发ws消息到模块
    connector.registerAwaitFetch(async (e) => {
        const session = store.sessionStore.activeSession;
        const currentRemote = session?.activeDataSource ?? GLOBAL_HOST;
        const { remote = currentRemote, args, module } = e.data;
        if (session && isClusterApplication(e.data.from) && args?.params?.isCompare === true && clusterView.versionFor(session) === 'modern') {
            const warning = i18n.t('clusterView.compareUnavailable', { ns: 'framework' });
            message.warning(warning);
            return { dataSource: remote, body: { error: { code: 'MODERN_CLUSTER_COMPARISON_UNAVAILABLE', message: warning } } };
        }
        const result = await requestModule(module, args);
        return { dataSource: remote, body: result };
    });
}

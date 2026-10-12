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
import { ServerConnector } from '@insight/lib/connection';
import { INTERCEPTOR_HANDLERS, NOTIFICATION_INTERCEPTOR_HANDLERS, NOTIFICATION_STATISTIC_HANDLERS } from './interceptor';
import { listenerMap } from './notification';
import { store } from '@/store';
import { requestModule } from '@/centralServer/server';
import { GLOBAL_HOST } from '@/centralServer/websocket/defs';
import { getTargetWindow } from './targetWindow';

type TargetWindow = Window;

const pluginEvents = ['remote/import', 'remote/reset', 'remote/remove', 'setTheme', 'wakeupPlugin', 'language', 'switchLanguage'];

class FrameworkConnector extends ServerConnector {
    protected getTargetWindows(): TargetWindow[] {
        return getTargetWindow();
    }
}

const connector = new FrameworkConnector({
    getTargetWindow,
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
        const currentRemote = store.sessionStore.activeSession?.activeDataSource ?? GLOBAL_HOST;
        const { remote = currentRemote, args, module } = e.data;
        const result = await requestModule(module, args);
        return { dataSource: remote, body: result };
    });
}

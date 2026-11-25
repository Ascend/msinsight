/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { runInAction } from 'mobx';
import i18n from '@insight/lib/i18n';
import { errorCenter, ErrorCode, WsError } from '@insight/lib';
import type { MemorySession } from '../entity/MemorySession';
import connector from './connector';

export type NotificationHandler = (data: any) => void;
export interface MemoryApplicationAdapter<S extends MemorySession> {
    session: S;
    requests: string[];
    reset: () => void;
    dispose: () => void;
    reconcile?: NotificationHandler;
    events: Record<string, NotificationHandler>;
    isParsing?: () => boolean;
}

export const createNotificationHandlers = <S extends MemorySession>(adapter: MemoryApplicationAdapter<S>): Record<string, NotificationHandler> => {
    const { session } = adapter;
    return {
        setTheme: data => { window.setTheme(Boolean(data.isDark)); },
        switchLanguage: data => {
            runInAction(() => { session.language = data.lang; });
            void i18n.changeLanguage(data.lang);
        },
        updateSession: data => {
            if (data.module !== undefined && data.module !== session.module) return;
            runInAction(() => {
                const keys = session.getUpdateKeys();
                Object.keys(data).forEach(key => {
                    if (keys.includes(key)) (session as unknown as Record<string, unknown>)[key] = data[key];
                });
                adapter.reconcile?.(data);
            });
        },
        'remote/remove': adapter.reset,
        'remote/reset': adapter.reset,
        'parse/fail': data => {
            errorCenter.handleError(new WsError(ErrorCode.PARSE_FAIL, data.error));
            adapter.reset();
        },
        getLeaksParseStatus: () => {
            connector.send({ event: 'leaksParseStatus', body: { complete: !(adapter.isParsing?.() ?? false) } });
        },
        ...adapter.events,
    };
};

export const subscribeNotifications = (handlers: Record<string, NotificationHandler>): (() => void) => {
    const listeners = Object.entries(handlers).map(([event, callback]) => connector.addListener(event, (message: MessageEvent) => {
        const body = message.data?.body;
        if (body !== null && typeof body === 'object') callback(body);
    }));
    return () => { listeners.forEach(listener => { connector.removeListener(listener); }); };
};

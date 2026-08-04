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
import React from 'react';
import { createRoot } from 'react-dom/client';
import App from './App';
import { RootStoreContext } from './context/context';
import '@insight/lib/style';
import { store } from './store';
import { NOTIFICATION_HANDLERS } from './interface';
import connector from './connection';
import '@insight/lib/i18n';
import { disableShortcuts } from '@insight/lib/utils';

interface CefQueryType { request: string; onSuccess: (response: string) => void; onFailure: (errorCode: number, errorMessage: string) => void };

declare global {
    interface Window {
        setTheme: (isDark: boolean) => void;
        request: (params: { command: string; params: Record<string, unknown> }) => Promise<any>;
        _resolve: (value: unknown) => void;
        _reject: (value?: any) => void;
        cefQuery: (obj: CefQueryType) => void;
    }

    interface DataSource {
        remote: string;
        port: number;
        dataPath: string[];
    }
};

// 禁用右键刷新以及F5、Ctrl+R刷新
document.oncontextmenu = (): boolean => false;
disableShortcuts();

// 用于存储最近一次请求的时间戳
const requestCache = new Map<string, number>();

window.request = async (params): Promise<any> => {
    // 根据 command、params生成一个唯一键
    const requestKey = `${params.command}_${JSON.stringify(params.params)}`;

    const now = Date.now();
    // 检查最近的请求时间
    const lastRequestTime = requestCache.get(requestKey);

    // 如果最近请求时间在 10ms 内，则跳过请求
    if (lastRequestTime !== null && lastRequestTime !== undefined && (now - lastRequestTime) < 1) {
        // 返回一个拒绝的 Promise 或自定义行为
        return Promise.reject(new Error('Requests are too frequent'));
    };

    // 更新请求时间
    requestCache.set(requestKey, now);
    const data = await connector.fetch({ args: params });
    // 移除缓存中的请求记录
    requestCache.delete(requestKey);
    return (data as any).body;
};

export function registerEventHandlers(): void {
    Object.entries(NOTIFICATION_HANDLERS).forEach(([event, callback]) => {
        connector.addListener(event, (e: MessageEvent<{ event: string; body: Record<string, unknown> }>) => {
            const res = e.data;
            if (res.body === undefined || typeof res.body !== 'object') {
                return;
            }
            callback(res.body);
        });
    });
}

export function getInitStatus(): void {
    connector.send({
        event: 'getParseStatus',
        body: {
            from: 'Leaks',
            requests: ['language', 'theme', 'fileHash', 'module', 'deviceIds', 'threadIds'],
        },
    });
}

const root = createRoot(document.getElementById('root') as HTMLElement);
root.render(
    (<React.StrictMode>
        <RootStoreContext.Provider value={store}>
            <App />
        </RootStoreContext.Provider>
    </React.StrictMode>));

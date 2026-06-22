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
import '@insight/lib/i18n';
import '@insight/lib/style';
import './index.css';
import connector from './connection';
import { NOTIFICATION_HANDLERS } from './interface';
import React from 'react';
import { createRequest, createDebounceRequest, disableShortcuts } from '@insight/lib/utils';
import { store } from './store';

Object.entries(NOTIFICATION_HANDLERS).forEach(([event, callback]) => {
    const session = store.sessionStore.activeSession;

    connector.addListener(event, (e: MessageEvent<{ event: string; body: Record<string, unknown> }>) => {
        const res = e.data;
        if (res.body === undefined || typeof res.body !== 'object') {
            console.error('[notify]', 'Wrong notify format.');
            return;
        }
        callback(res.body, session);
    });
});

// 禁用右键刷新以及F5、Ctrl+R刷新
document.oncontextmenu = (): boolean => false;
disableShortcuts();

interface CefQueryType {request: string; onSuccess: (response: string) => void; onFailure: (errorCode: number, errorMessage: string) => void};

export const Loading = (<div style={{ textAlign: 'center', top: '50%', position: 'absolute', width: '50px', left: 'calc(50% - 25px)' }}>
    <div className={'loading'} style={{ marginLeft: '15px' }}></div>
</div>);

declare global {
    interface Window {
        // for inspecting some internal state
        debugInspector: {
            store?: unknown;
        };
        setTheme: (isDark: boolean) => void;
        request: (dataSource: DataSource, params: { command: string; params: Record<string, unknown> }) => Promise<any>;
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

declare global {
    interface Window {
        requestData: (method: string, params: any, module?: string) => Promise<any>;
        requestDataDebounced: ((method: string, params: any, module?: string) => Promise<any>) & {
            cancel: (key?: string) => void;
            flush: (key?: string) => void;
            getPendingCount: () => number;
            getPendingKeys: () => string[];
        };
        dataSource: DataSource;

        closeWaiting: () => void;
    }
};

window.requestData = createRequest(connector);
window.requestDataDebounced = createDebounceRequest(connector, { delay: 300, leading: false });

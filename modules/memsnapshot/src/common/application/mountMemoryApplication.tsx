/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import React, { useEffect } from 'react';
import { createRoot } from 'react-dom/client';
import { observer } from 'mobx-react';
import { ThemeProvider } from '@emotion/react';
import { SharedConfigProvider } from '@insight/lib';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import { disableShortcuts } from '@insight/lib/utils';
import '@insight/lib/style';
import '@insight/lib/i18n';
import '../index.css';
import type { MemorySession } from '../entity/MemorySession';
import connector from './connector';
import { createNotificationHandlers, subscribeNotifications, type MemoryApplicationAdapter } from './notifications';

interface RequestParams { command: string; params: Record<string, unknown> }
declare global {
    interface Window { request: (params: RequestParams) => Promise<any> }
}

export const mountMemoryApplication = <S extends MemorySession>(adapter: MemoryApplicationAdapter<S>, Page: React.ComponentType<{ session: S }>): (() => void) => {
    const container = document.getElementById('root');
    if (!container) throw new Error('Memory application root is missing');
    document.oncontextmenu = () => false;
    disableShortcuts();
    const pending = new Map<string, number>();
    const previousRequest = window.request;
    const request: Window['request'] = async params => {
        const key = `${params.command}_${JSON.stringify(params.params)}`;
        const last = pending.get(key);
        if (last !== undefined && Date.now() - last < 1) throw new Error('Requests are too frequent');
        pending.set(key, Date.now());
        try {
            const response = await connector.fetch({ args: params });
            return (response as any).body;
        } finally { pending.delete(key); }
    };
    window.request = request;
    const handlers = createNotificationHandlers(adapter);
    const Application = observer(() => {
        useEffect(() => {
            const unsubscribe = subscribeNotifications(handlers);
            connector.send({ event: 'getParseStatus', body: { requests: adapter.requests } });
            return unsubscribe;
        }, []);
        return <ThemeProvider theme={themeInstance.getThemeType()}>
            <GlobalStyles />
            <SharedConfigProvider locale={adapter.session.language}>
                <Page session={adapter.session} />
            </SharedConfigProvider>
        </ThemeProvider>;
    });
    const root = createRoot(container);
    root.render(<React.StrictMode><Application /></React.StrictMode>);
    let disposed = false;
    const dispose = (): void => {
        if (disposed) return;
        disposed = true;
        root.unmount();
        adapter.dispose();
        pending.clear();
        if (window.request === request) window.request = previousRequest;
        window.removeEventListener('pagehide', dispose);
    };
    window.addEventListener('pagehide', dispose);
    return dispose;
};

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
import { ClientConnector } from '@insight/lib/connection';
import { ACP_MESSAGE_CHANNEL } from '@insight/lib/FrontendAgentCommand';
import { parseAgentReportReference, REPORT_OPEN, REPORT_OPENED, REPORT_READY, REPORT_CLOSE, type AgentReportReference } from '@insight/lib/AgentReport';
import {
    getWindowMessageRouter,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    parentWindowMessageOrigin,
    withWindowMessageChannel,
} from '@insight/lib/WindowMessageRouter';

export const connector = new ClientConnector({
    getTargetWindow: (): Window[] => [window.parent],
    module: 'insight_web_agent',
});

export type HostLocale = 'zhCN' | 'enUS';

export interface HostNotificationHandlers {
    setTheme: (isDark: boolean) => void;
    switchLanguage: (locale: HostLocale) => void;
    updateContext: (context: HostContext) => void;
}

export interface HostContext {
    [key: string]: any;
}

export const registerHostEventHandlers = (handlers: HostNotificationHandlers): (() => void) => {
    const fromHost = (event: MessageEvent): boolean => event.source === window.parent &&
        matchesWindowMessageOrigin(event.origin, parentWindowMessageOrigin());
    connector.addListener('setTheme', (event: MessageEvent<{ body?: { isDark?: unknown } }>) => {
        if (!fromHost(event)) return;
        handlers.setTheme(Boolean(event.data.body?.isDark));
    });
    connector.addListener('switchLanguage', (event: MessageEvent<{ body?: { lang?: unknown } }>) => {
        if (!fromHost(event)) return;
        const locale = event.data.body?.lang;
        if (locale === 'zhCN' || locale === 'enUS') handlers.switchLanguage(locale);
    });
    return getWindowMessageRouter().subscribe((event: MessageEvent<{ event?: unknown; body?: HostContext }>) => {
        if (!fromHost(event)) return;
        if (event.data?.event === 'insightWebAgent/context') handlers.updateContext(event.data.body ?? {});
    }, isWindowMessageChannel(ACP_MESSAGE_CHANNEL));
};

export const requestHostInitStatus = (): void => {
    connector.send({
        event: 'getParseStatus',
        body: {
            from: 'InsightWebAgent',
            requests: ['language', 'theme'],
        },
    });
};

export const notifyHostReady = (): void => {
    window.parent?.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, { event: 'insightWebAgent/ready' }), parentWindowMessageOrigin());
};

export const requestHostClose = (): void => {
    window.parent?.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, { event: 'insightWebAgent/close' }), parentWindowMessageOrigin());
};

export const requestHostOpenReport = (report: AgentReportReference): Promise<boolean> => {
    const reference = parseAgentReportReference(report);
    if (window.parent === window || !reference) return Promise.resolve(false);
    const requestId = crypto.randomUUID();
    const origin = parentWindowMessageOrigin();
    return new Promise((resolve) => {
        const finish = (opened: boolean): void => { clearTimeout(timer); unsubscribe(); resolve(opened); };
        const unsubscribe = getWindowMessageRouter().subscribe((event) => {
            if (event.source === window.parent && matchesWindowMessageOrigin(event.origin, origin) &&
                event.data?.event === REPORT_OPENED && event.data?.requestId === requestId) finish(true);
        }, isWindowMessageChannel(ACP_MESSAGE_CHANNEL));
        const timer = window.setTimeout(() => finish(false), 3000);
        window.parent.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, {
            event: REPORT_OPEN, requestId, body: { report: reference },
        }), origin);
    });
};

export const notifyReportReady = (): void => {
    window.parent.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, { event: REPORT_READY }), parentWindowMessageOrigin());
};

export const requestHostCloseReport = (): void => {
    window.parent.postMessage(withWindowMessageChannel(ACP_MESSAGE_CHANNEL, { event: REPORT_CLOSE }), parentWindowMessageOrigin());
};

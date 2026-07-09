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

export const connector = new ClientConnector({
    getTargetWindow: (): Window[] => [window.parent],
    module: 'insight_web_agent',
});

export type HostLocale = 'zhCN' | 'enUS';

export interface HostNotificationHandlers {
    setTheme: (isDark: boolean) => void;
    switchLanguage: (locale: HostLocale) => void;
}

export const registerHostEventHandlers = (handlers: HostNotificationHandlers): void => {
    connector.addListener('setTheme', (event: MessageEvent<{ body?: { isDark?: unknown } }>) => {
        handlers.setTheme(Boolean(event.data.body?.isDark));
    });
    connector.addListener('switchLanguage', (event: MessageEvent<{ body?: { lang?: unknown } }>) => {
        const locale = event.data.body?.lang;
        if (locale === 'zhCN' || locale === 'enUS') handlers.switchLanguage(locale);
    });
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

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

import i18n from '@insight/lib/i18n';
import type { NumaStore } from '@/features/overview/model/NumaStore';
import type { NotificationHandler } from './types';

export type NumaNotificationHandlers = Record<string, NotificationHandler>;

export function createNumaNotificationHandlers(store: NumaStore): NumaNotificationHandlers {
    const reset: NotificationHandler = () => {
        store.reset();
    };

    return {
        switchDirectory: (body) => store.switchDirectory({
            rankId: String(body.rankId ?? ''),
            selectedFilePath: String(body.selectedFilePath ?? ''),
        }),
        setTheme: (body) => {
            const isDark = Boolean(body.isDark);
            document.documentElement.dataset.theme = isDark ? 'dark' : 'light';
            window.setTheme(isDark);
        },
        switchLanguage: (body) => {
            store.setLocale(body.lang);
            void i18n.changeLanguage(store.locale);
        },
        updateSession: (body) => {
            if (!Object.prototype.hasOwnProperty.call(body, 'timeAnalysisRange')) return;
            return store.setTimeAnalysisRange(body.timeAnalysisRange);
        },
        moduleActive: () => store.activate(),
        'remote/remove': reset,
        'remote/reset': reset,
    };
}

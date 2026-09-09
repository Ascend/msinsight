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

import React from 'react';
import { createRoot } from 'react-dom/client';
import { ThemeProvider } from '@emotion/react';
import { observer } from 'mobx-react';
import '@insight/lib/style';
import '@insight/lib/i18n';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import { numaStore } from '@/features/overview/model/createNumaStore';
import { connector } from '@/infrastructure/connection/client';
import { createNumaNotificationHandlers } from '@/infrastructure/connection/notificationHandlers';
import type { NotificationMessage } from '@/infrastructure/connection/types';
import '@/app/app.css';
import '@/features/overview/styles/metrics.css';
import '@/features/overview/styles/overview.css';
import '@/features/topology/styles/topology.css';
import '@/features/overview/styles/details.css';
import { NumaApp } from './NumaApp';

document.oncontextmenu = (): boolean => false;
document.documentElement.dataset.theme = themeInstance.getCurrentTheme();
document.body.className = themeInstance.getCurrentTheme() === 'dark' ? 'theme_dark' : 'theme_light';

const notificationHandlers = createNumaNotificationHandlers(numaStore);
Object.entries(notificationHandlers).forEach(([event, handler]) => {
    connector.addListener(event, (message: MessageEvent<NotificationMessage>) => {
        void handler(message.data.body ?? {});
    });
});

connector.send({
    event: 'getParseStatus',
    body: { from: 'NUMA', requests: ['language', 'theme', 'timeAnalysisRange', 'directory'] },
});

const root = createRoot(document.getElementById('root') as HTMLElement);
const ThemedApp = observer(() => (
    <ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <NumaApp store={numaStore} />
    </ThemeProvider>
));
root.render(<ThemedApp />);

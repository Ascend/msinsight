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
/// <reference types="react-scripts" />
import React, { useEffect } from 'react';
import { createRoot } from 'react-dom/client';
import { observer } from 'mobx-react';
import { Global, ThemeProvider } from '@emotion/react';
import { SharedConfigProvider } from '@insight/lib';
import { ClientConnector } from '@insight/lib/connection';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import { createRequest, disableShortcuts } from '@insight/lib/utils';
import i18n from '@insight/lib/i18n';
import '@insight/lib/style';
import Summary from './index';
import { OverviewSession, connectOverviewContext } from './session';

declare global {
    interface Window {
        requestData: ReturnType<typeof createRequest>;
    }
}

const session = new OverviewSession();
const connector = new ClientConnector({ module: 'summary' });
window.requestData = createRequest(connector);
document.oncontextmenu = (): boolean => false;
disableShortcuts();
window.setTheme(true);

const App = observer(() => {
    useEffect(() => connectOverviewContext(session, connector, window.setTheme, language => {
        void i18n.changeLanguage(language);
    }), []);
    return <ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <Global styles={{ '#root': { height: '100vh', overflow: 'auto' } }} />
        <SharedConfigProvider locale={session.language}>
            <Summary session={session} onClusterChange={clusterPath => connector.send({
                event: 'clusterPage/selectCluster',
                body: { project: session.selectedProjectName, clusterPath },
            })} />
        </SharedConfigProvider>
    </ThemeProvider>;
});
const root = createRoot(document.getElementById('root') as HTMLElement);
root.render(<App />);
window.addEventListener('pagehide', () => root.unmount(), { once: true });

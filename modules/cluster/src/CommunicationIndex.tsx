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
import React, { useEffect, useLayoutEffect, useState } from 'react';
import { SharedConfigProvider } from '@insight/lib';
import { createRoot } from 'react-dom/client';
import { ThemeProvider } from '@emotion/react';
import { RootStoreContext, useRootStore } from './context/context';
import { store } from './store';
import connector from './connection';
import { observer } from 'mobx-react';
import { themeInstance } from './theme/theme';
import CommunicationAnalysis from './components/communication/CommunicationAnalysis';
import { Loading } from './index';
import { GlobalStyles } from '@insight/lib/theme';
import { setCommunicationAgentSession } from './agent/communicationController';
import { startCommunicationAgentRuntime, stopCommunicationAgentRuntime } from './agent/runtime';

export const App = observer(() => {
    const { sessionStore } = useRootStore();
    let session = sessionStore.activeSession;
    const [locale, setLocale] = useState<'zhCN' | 'enUS'>('zhCN');

    // Bind before child passive effects publish their page state.
    useLayoutEffect(() => {
        setCommunicationAgentSession(session);
        return () => setCommunicationAgentSession(undefined);
    }, [session]);

    useEffect(() => {
        startCommunicationAgentRuntime();
        return stopCommunicationAgentRuntime;
    }, []);

    useEffect(() => {
        if (session) {
            setLocale(session.language);
        }
    }, [session?.language]);

    useEffect(() => {
        session = sessionStore.activeSession;
        getLanguage();
        themeInstance.setCurrentTheme('dark');
        window.setTheme(true);
        connector.send({ event: 'getParseStatus', body: { } });
    }, []);

    const getLanguage = (): void => {
        connector.send({
            event: 'getLanguage',
        });
    };

    return (<ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <SharedConfigProvider locale={locale}>
            {session?.clusterCompleted && <CommunicationAnalysis session={session} />}
            <div className={`fullmask ${session?.clusterCompleted ? 'hide' : ''}`}>{Loading}</div>
        </SharedConfigProvider>
    </ThemeProvider>);
});

const root = createRoot(document.getElementById('root') as HTMLElement);
root.render(
    (<React.StrictMode>
        <RootStoreContext.Provider value={store}>
            <App />
        </RootStoreContext.Provider>
    </React.StrictMode>));

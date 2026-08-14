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
import { observer } from 'mobx-react';
import React, { useEffect, useState } from 'react';
import { ThemeProvider } from '@emotion/react';
import { useRootStore } from './context/context';
import Leaks from './components/leaks/leaks';
import { themeInstance } from './theme/theme';
import { registerEventHandlers, getInitStatus } from './index';
import { GlobalStyles } from '@insight/lib/theme';
import { SharedConfigProvider } from '@insight/lib';
import { startMemScopeAgentRuntime, stopMemScopeAgentRuntime } from './agent/runtime';
import './index.css';
const App = observer(() => {
    const { sessionStore } = useRootStore();
    const [locale, setLocale] = useState<'zhCN' | 'enUS'>('zhCN');
    let session = sessionStore.activeSession;
    useEffect(() => {
        session = sessionStore.activeSession;
        registerEventHandlers();
        getInitStatus();
        startMemScopeAgentRuntime();
        return stopMemScopeAgentRuntime;
    }, []);
    useEffect(() => {
        if (session) {
            setLocale(session.language);
        }
    }, [session?.language]);
    return <ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <SharedConfigProvider locale={locale}>
            {session !== undefined ? <Leaks session={session} /> : <></>}
        </SharedConfigProvider>

    </ThemeProvider>;
});

export default App;

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
import { Global, ThemeProvider } from '@emotion/react';
import i18n from '@insight/lib/i18n';
import { SharedConfigProvider } from '@insight/lib/SharedConfigProvider';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import { useEffect, useState } from 'react';
import { registerHostEventHandlers, requestHostInitStatus } from './connection';
import { ChatStateProvider } from './hooks/useChatState';
import { ChatPage } from './components/ChatPage';

type Locale = 'zhCN' | 'enUS';

const App = (): JSX.Element => {
    const [themeName, setThemeName] = useState(themeInstance.getCurrentTheme());
    const [locale, setLocale] = useState<Locale>((i18n.language as Locale) || 'enUS');

    useEffect(() => {
        const applyTheme = (isDark: boolean): void => {
            const nextTheme = isDark ? 'dark' : 'light';
            themeInstance.setCurrentTheme(nextTheme);
            localStorage.setItem('theme', JSON.stringify(nextTheme));
            setThemeName(nextTheme);
        };
        const applyLanguage = (nextLocale: Locale): void => {
            setLocale(nextLocale);
            void i18n.changeLanguage(nextLocale);
        };

        const handleStorage = (event: StorageEvent): void => {
            if (event.key !== 'theme' || event.newValue === null) return;
            applyTheme(event.newValue === '"dark"' || event.newValue === 'dark');
        };

        registerHostEventHandlers({
            setTheme: applyTheme,
            switchLanguage: applyLanguage,
        });
        requestHostInitStatus();
        window.addEventListener('storage', handleStorage);
        return () => {
            window.removeEventListener('storage', handleStorage);
        };
    }, []);

    return <ThemeProvider theme={themeInstance.getTheme()[themeName]}>
        <GlobalStyles />
        <Global styles={{
            html: { width: '100%', height: '100%' },
            body: { width: '100%', height: '100%', margin: 0 },
        '#root': { width: '100%', height: '100%' },
        '*': { boxSizing: 'border-box' },
    }} />
        <SharedConfigProvider locale={locale}>
            <ChatStateProvider>
                <ChatPage />
            </ChatStateProvider>
        </SharedConfigProvider>
    </ThemeProvider>;
};

export default App;

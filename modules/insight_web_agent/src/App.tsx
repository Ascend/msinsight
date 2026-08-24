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
import { isBackendUnavailableError, updateContext } from './api';
import { notifyHostReady, registerHostEventHandlers, requestHostInitStatus } from './connection';
import { ChatStateProvider } from './hooks/useChatState';
import { ChatPage } from './components/ChatPage';
import { BackendUnavailableDialog } from './components/BackendUnavailableDialog';

type Locale = 'zhCN' | 'enUS';

const App = (): JSX.Element => {
    const [themeName, setThemeName] = useState(themeInstance.getCurrentTheme());
    const [locale, setLocale] = useState<Locale>((i18n.language as Locale) || 'enUS');

    useEffect(() => {
        const synchronizeContext = async (context: Parameters<typeof updateContext>[0]): Promise<void> => {
            try {
                await updateContext(context);
            } catch (error) {
                if (!isBackendUnavailableError(error)) {
                    console.error('Failed to update Insight Bot context:', error);
                }
            }
        };
        const applyTheme = (isDark: boolean): void => {
            const nextTheme = isDark ? 'dark' : 'light';
            themeInstance.setCurrentTheme(nextTheme);
            localStorage.setItem('theme', JSON.stringify(nextTheme));
            setThemeName(nextTheme);
        };
        const applyLanguage = (nextLocale: Locale): void => {
            setLocale(nextLocale);
            i18n.changeLanguage(nextLocale);
        };

        const handleStorage = (event: StorageEvent): void => {
            if (event.key !== 'theme' || event.newValue === null) {
                return;
            }
            applyTheme(event.newValue === '"dark"' || event.newValue === 'dark');
        };

        const unregisterAgentContext = registerHostEventHandlers({
            setTheme: applyTheme,
            switchLanguage: applyLanguage,
            updateContext: (context): void => {
                synchronizeContext(context);
            },
        });
        requestHostInitStatus();
        notifyHostReady();
        window.addEventListener('storage', handleStorage);
        return () => {
            unregisterAgentContext();
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
                <BackendUnavailableDialog />
            </ChatStateProvider>
        </SharedConfigProvider>
    </ThemeProvider>;
};

export default App;

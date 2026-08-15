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
import React, { useEffect, useState } from 'react';
import { runInAction } from 'mobx';
import { SharedConfigProvider } from '@insight/lib';
import { createRoot } from 'react-dom/client';
import { ThemeProvider } from '@emotion/react';
import { RootStoreContext, useRootStore } from './context/context';
import { store } from './store';
import connector from './connection';
import { observer } from 'mobx-react';
import { themeInstance, GlobalStyles } from '@insight/lib/theme';
import AnalysisSummary from './pages/AnalysisSummary';
import { Loading } from './index';

export const App = observer(() => {
    const { sessionStore } = useRootStore();
    let session = sessionStore.activeSession;
    const [locale, setLocale] = useState<'zhCN' | 'enUS'>('zhCN');

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

    /* 添加 selectedClusterPath 属性事件监听，保证切换 selectedClusterPath 时页面能整体更新 */
    useEffect(() => {
        if (!session) { return; }
        runInAction(() => {
            session.resetForClusterChange();
            session.renderId = ++session.renderId % 1000;
        });
    }, [session?.selectedClusterPath]);

    // 当前数据无 cluster 维度（不在 clusterList 中）或 cluster 已解析完成时，显示真实内容
    const showContent = React.useMemo(() => {
        return !session?.isClusterPathInList || !!session?.clusterCompleted;
    }, [session?.isClusterPathInList, session?.clusterCompleted]);

    return (<ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <SharedConfigProvider locale={locale}>
            {showContent && <AnalysisSummary session={session} />}
            <div className={`fullmask ${showContent ? 'hide' : ''}`}>{Loading}</div>
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

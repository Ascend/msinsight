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

import { ThemeProvider } from '@emotion/react';
import { SharedConfigProvider } from '@insight/lib';
import styled from '@emotion/styled';
import { observer } from 'mobx-react';
import React, { useEffect, useState } from 'react';
import { AppErrorBoundary } from './components/error/AppErrorBoundary';
import { SessionPageErrorBoundary } from './components/error/SessionPageErrorBoundary';
import { ActionManagerContext, useRootStore } from './context/context';
import { SessionPage } from './pages/SessionPage';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import connector from './connection';
import { disableShortcuts } from '@insight/lib/utils';
import { ActionManager } from './actions/manager';

const Window = styled.div`
    height: 100vh;
    overflow: hidden;
    display: flex;
    width: 100vw;
    color: ${(props): string => props.theme.textColor};
    font-family: ${(props): string => props.theme.fontFamily};
`;

disableShortcuts();

const ImgWithFallback = ({
    className = '',
}): JSX.Element => {
    const PictureContainer = styled.picture`
        img {
            height: 48px;
            width: 48px;
        }
    `;
    return (
        <PictureContainer>
            <div className={className}></div>
        </PictureContainer>
    );
};

const StatePopover = observer(() => {
    const Mask = styled.div`
        position: absolute;
        display: flex;
        z-index: 4;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        width: 100%;
        height: 100%;
        justify-content: center;
        align-items: center;
        background-color: ${(props): string => props.theme.maskColor};
    `;
    const Info = styled.div`
        border-radius: 4px;
        position: relative;
        display: flex;
        align-items: center;
        width: 16rem;
        height: 3rem;
        font-size: 1.12rem;
        .img {
            margin-right: 10px;
        }
    `;
    return <Mask>
        <Info className={'info'}><ImgWithFallback className={'loading'}/>waiting...</Info>
    </Mask>;
});

export const App = observer(() => {
    const { insightStore, sessionStore } = useRootStore();
    let session = sessionStore.activeSession;
    const [locale, setLocale] = useState<'zhCN' | 'enUS'>('zhCN');
    const theme = themeInstance.getThemeType();

    useEffect(() => {
        if (session) {
            setLocale(session.language);
        }
    }, [session?.language]);

    useEffect(() => {
        insightStore.loadTemplates().then(() => {
            session = sessionStore.activeSession;
        });
        getLanguage();
        getTheme();
    }, []);

    const getLanguage = (): void => {
        connector.send({
            event: 'getLanguage',
        });
    };
    const getTheme = (): void => {
        connector.send({
            event: 'getTheme',
        });
    };
    return (
        <ThemeProvider theme={theme}>
            <GlobalStyles />
            <Window>
                <AppErrorBoundary>
                    <SessionPageErrorBoundary>
                        <SharedConfigProvider locale={locale}>
                            {
                                session !== undefined
                                    ? <ActionManagerContext.Provider value={new ActionManager(session)}>
                                        <SessionPage session={session} />
                                    </ActionManagerContext.Provider>
                                    : <StatePopover/>
                            }
                        </SharedConfigProvider>
                    </SessionPageErrorBoundary>
                </AppErrorBoundary>
            </Window>
        </ThemeProvider>
    );
});

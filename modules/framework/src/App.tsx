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
import { ThemeProvider } from '@emotion/react';
import { observer } from 'mobx-react';
import { GlobalStyles, themeInstance } from '@insight/lib/theme';
import { DragDirection, SizeMethod, useDraggableContainer, SharedConfigProvider } from '@insight/lib';
import { useRootStore } from './context/context';
import RemoteManager from './components/RemoteManager/Index';
import Main from './components/Main';
import { Session } from '@/entity/session';
import { connectRemote } from '@/centralServer/server';
import { JUPYTERLABPROXY, GLOBAL_HOST } from '@/centralServer/websocket/defs';
import './index.css';
import { registerEventListeners } from '@/connection';
import { registerDragAndDropFile } from '@/utils';
import { Spin } from 'antd';
import { LoadingOutlined } from '@ant-design/icons';
import i18n from '@insight/lib/i18n';
import { runInAction } from 'mobx';
import { isVscodePluginEnvironment, pluginModeInit, isVscodeEnv } from '@/vscode-adapter';

const init = async (session: Session): Promise<void> => {
    // 注册文件拖拽
    registerDragAndDropFile();
    // 连接与模块的通信
    registerEventListeners();

    // 连接ws（启动后第一次）
    if (isVscodeEnv()) {
        pluginModeInit();
        if (isVscodePluginEnvironment()) {
            return;
        }
    }

    // ws连接
    console.log('[App] ========== connectRemote ==========', { ...GLOBAL_HOST, jupyterlabProxy: JUPYTERLABPROXY });
    const isSuccess = await connectRemote({ ...GLOBAL_HOST, jupyterlabProxy: JUPYTERLABPROXY });
    if (isSuccess) {
        runInAction(() => {
            session.defaultConnected = true;
        });
    }
};

const LEFT_WIDTH = 300;
const App = observer(() => {
    const session = useRootStore().sessionStore.activeSession;
    const [locale] = useState<'zhCN' | 'enUS'>('zhCN');

    const [view] = useDraggableContainer({
        draggableWH: LEFT_WIDTH,
        foldWH: 0,
        dragDirection: DragDirection.LEFT,
        open: true,
        sizeMethod: SizeMethod.NUMBER,
    });

    useEffect(() => {
        // 启动
        init(session);
    }, []);

    const isVscode = isVscodeEnv() && isVscodePluginEnvironment();

    return (isVscode || session?.defaultConnected)
        ? <ThemeProvider theme={themeInstance.getThemeType()}>
            <GlobalStyles />
            <SharedConfigProvider locale={locale}>
                <Spin
                    wrapperClassName={`spin-global ${isVscode ? 'no-remote-manager' : ''}`}
                    spinning={session.loading}
                    tip={i18n.t('Loading', { ns: 'framework' })}
                    indicator={<LoadingOutlined style={{ fontSize: 24 }} spin />}>
                    {
                        view({
                            mainContainer: <Main session={session} />,
                            draggableContainer: isVscode ? undefined : <RemoteManager session={session} />,
                            id: 'framework',
                            padding: 16,
                        })
                    }
                </Spin>
            </SharedConfigProvider>
        </ThemeProvider>
        : <></>;
});

export default App;

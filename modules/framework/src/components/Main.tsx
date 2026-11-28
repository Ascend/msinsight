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
import React, { useState } from 'react';
import { observer } from 'mobx-react';
import { LocalStorageKey, localStorageService } from '@insight/lib';
import { Session } from '@/entity/session';
import TabPane from './TabPane/Index';
import ToolBox from './ToolBox/Index';

interface IProps {
    session: Session;
}
const Main = observer(({ session }: IProps) => {
    const [showSessionPanel, setShowSessionPanel] = useState(
        () => localStorageService.getItem(LocalStorageKey.INSIGHT_BOT_PANEL_OPEN) !== false,
    );
    const updateSessionPanel = (open: boolean): void => {
        setShowSessionPanel(open);
        localStorageService.setItem(LocalStorageKey.INSIGHT_BOT_PANEL_OPEN, open);
    };

    return <div style={{ height: '100%' }}>
        <ToolBox
            insightBotActive={showSessionPanel}
            onInsightBotClick={() => updateSessionPanel(!showSessionPanel)}
        />
        <TabPane
            onCloseSessionPanel={() => updateSessionPanel(false)}
            session={session}
            showSessionPanel={showSessionPanel}
        />
    </div>;
});

export default Main;

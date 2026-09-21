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
import { Tooltip } from '@insight/lib/components';
import { themeInstance, GlobalStyles } from '@insight/lib/theme';
import '@insight/lib/style';
import '@insight/lib/i18n';
import i18n from 'i18next';
import { LifecycleZoomModeTooltip } from '../../src/components/leaks/LifecycleGraphToolbar';
import { GraphToolbarTooltipStyle, graphToolbarTooltipClassName } from '../../src/components/leaks/tools';

// Exercise the real shared component, translations, styles and tooltip positioning.
// No backend connection or profiling data is needed for this presentation test.
const query = new URLSearchParams(location.search);
const mode = query.get('mode') === 'horizontal' ? 'horizontal' : 'proportional';
const placement = query.get('placement') === 'left' ? 'left' : 'topRight';
window.setTheme(false);
await i18n.changeLanguage(query.get('lang') || 'enUS');
createRoot(document.getElementById('root')!).render(
    <ThemeProvider theme={themeInstance.getThemeType()}>
        <GlobalStyles />
        <GraphToolbarTooltipStyle />
        <div style={{ position: 'absolute', right: 16, top: 300 }}>
            <Tooltip title={<LifecycleZoomModeTooltip zoomMode={mode} />} placement={placement}
                overlayClassName={graphToolbarTooltipClassName} mouseEnterDelay={0} mouseLeaveDelay={0}>
                <button>Zoom mode</button>
            </Tooltip>
        </div>
    </ThemeProvider>,
);

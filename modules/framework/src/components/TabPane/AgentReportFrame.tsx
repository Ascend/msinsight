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
import React, { useEffect, useMemo, useRef } from 'react';
import { useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import { ACP_MESSAGE_CHANNEL } from '@insight/lib/FrontendAgentCommand';
import { REPORT_CLOSE, REPORT_READY, type AgentReportReference } from '@insight/lib/AgentReport';
import { frameWindowMessageOrigin, getWindowMessageRouter, isWindowMessageChannel, matchesWindowMessageOrigin } from '@insight/lib/WindowMessageRouter';
import { agentPageUrl } from './agentPageUrl';

export const AgentReportFrame = ({ report, active, onClose }: {
    report: AgentReportReference; active: boolean; onClose: () => void;
}): JSX.Element => {
    const frame = useRef<HTMLIFrameElement>(null);
    const src = useMemo(() => agentPageUrl(report), [report.artifactId, report.version]);
    const origin = useMemo(() => frameWindowMessageOrigin(src), [src]);
    const theme = useTheme();
    const { i18n } = useTranslation();
    useEffect(() => {
        const syncAppearance = (): void => {
            frame.current?.contentWindow?.postMessage({ event: 'setTheme', body: { isDark: theme.mode === 'dark' } }, origin);
            frame.current?.contentWindow?.postMessage({ event: 'switchLanguage', body: { lang: i18n.language } }, origin);
        };
        syncAppearance();
        return getWindowMessageRouter().subscribe((event) => {
            if (event.source !== frame.current?.contentWindow || !matchesWindowMessageOrigin(event.origin, origin)) return;
            if (event.data?.event === REPORT_READY) syncAppearance();
            if (event.data?.event === REPORT_CLOSE) onClose();
        }, isWindowMessageChannel(ACP_MESSAGE_CHANNEL));
    }, [origin, theme.mode, i18n.language, onClose]);
    return <iframe ref={frame} data-agent-report="true" src={src} title={report.title}
        style={{ display: active ? 'block' : 'none' }} />;
};

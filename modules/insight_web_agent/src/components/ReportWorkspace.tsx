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
import React, { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { reportFromTool, reportKey, type ReportArtifact } from '../artifacts';
import { requestHostOpenReport } from '../connection';
import { ReportPreview } from './ReportPreview';
import { useChatState } from '../hooks/useChatState';
import type { ChatMessage, ToolCallItem } from '../types';

const ReportContext = createContext<(report: ReportArtifact) => void>(() => {});
const EMPTY_MESSAGES: ChatMessage[] = [];

const Workspace = styled.div`
    flex: 1; min-height: 0; display: flex; position: relative; overflow: hidden;
    > .chat-column { display: flex; flex: 1; min-width: 0; }
    &[data-preview='true'] > .chat-column { display: none; }
`;

const OpenError = styled.div`
    padding: 8px 16px; color: ${(p): string => p.theme.textColorPrimary};
    button { margin-left: 8px; cursor: pointer; }
`;

const Card = styled.button`
    display: flex; align-items: center; gap: 14px; width: 100%; text-align: left;
    margin: 12px 0; padding: 16px; border: 1px solid ${(p): string => p.theme.borderColor};
    border-radius: 10px; color: ${(p): string => p.theme.textColorPrimary};
    background: ${(p): string => p.theme.bgColorLight}; cursor: pointer;
    &:hover:not(:disabled), &:focus-visible { border-color: #547de8; outline: 2px solid #547de833; }
    &:disabled { cursor: default; }
    .report-icon { display: grid; place-items: center; width: 40px; height: 44px; flex-shrink: 0;
        border: 1px solid #547de844; border-radius: 6px; color: #547de8; background: #547de80d; }
    .report-details { flex: 1; min-width: 0; }
    strong { display: block; font-size: 14px; line-height: 22px; overflow-wrap: anywhere; }
    small { display: block; margin-top: 4px; opacity: .65; line-height: 18px; }
    .report-open { font-size: 12px; white-space: nowrap; color: #547de8; }
`;

export const ReportCard = ({ tool }: { tool: ToolCallItem }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const open = useContext(ReportContext);
    const report = reportFromTool(tool);
    return <Card type="button" disabled={!report} onClick={() => report && open(report)}>
        <span className="report-icon" aria-hidden="true"><svg width="24" height="24" viewBox="0 0 24 24" fill="none">
            <path d="M5 3h10l4 4v14H5zM9 16v-4m4 4V9m4 7v-5" stroke="currentColor" strokeWidth="1.5" strokeLinejoin="round" />
        </svg></span>
        <span className="report-details"><strong>{report?.title ?? t('reportTitle')}</strong>
            <small>{report ? (report.description || t('reportReady')) : t(tool.status === 'failed' ? 'reportFailed' : tool.status === 'completed' ? 'reportUnavailable' : 'reportGenerating')}</small>
        </span>
        {report && <span className="report-open">V{report.version} · {t('reportOpen')} ↗</span>}
    </Card>;
};

export const ReportWorkspace = ({ children }: { children: ReactNode }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const embedded = window.parent !== window;
    const { currentSessionId, isDraftSession, messages = EMPTY_MESSAGES, pendingPrompt = false } = useChatState();
    const sessionKey = `${isDraftSession ? 'draft' : 'session'}:${currentSessionId ?? ''}`;
    const [selection, setSelection] = useState<{ sessionKey: string; report: ReportArtifact }>();
    const [failedOpen, setFailedOpen] = useState<{ sessionKey: string; report: ReportArtifact }>();
    const openRequest = useRef(0);
    const open = useCallback((report: ReportArtifact): void => {
        if (!embedded) { setSelection({ sessionKey, report }); return; }
        const request = ++openRequest.current;
        setFailedOpen(undefined);
        const fail = (): void => {
            if (openRequest.current === request) setFailedOpen({ sessionKey, report });
        };
        void requestHostOpenReport(report).then((opened) => {
            if (!opened) fail();
        }).catch(fail);
    }, [embedded, sessionKey]);
    const seen = useRef<{ sessionKey: string; keys: Set<string>; pending: boolean }>({ sessionKey: '', keys: new Set(), pending: false });
    const reports = useMemo(() => messages.filter((message) => message.role === 'assistant')
        .flatMap((message) => message.content.flatMap((block) => {
            const report = block.type === 'tool' ? reportFromTool(block.toolCall) : undefined;
            return report ? [report] : [];
        })), [messages]);
    const selected = !embedded && selection?.sessionKey === sessionKey ? selection.report : undefined;
    useEffect(() => {
        const keys = new Set(reports.map(reportKey));
        if (seen.current.sessionKey !== sessionKey) {
            seen.current = { sessionKey, keys, pending: pendingPrompt };
            setSelection(undefined);
            return;
        }
        const added = reports.filter((report) => !seen.current.keys.has(reportKey(report)));
        const wasPending = seen.current.pending;
        seen.current = { sessionKey, keys, pending: pendingPrompt };
        // Restoring history must not open a report. Only new live results do.
        if ((pendingPrompt || wasPending) && added.length) open(added[added.length - 1]);
    }, [reports, sessionKey, pendingPrompt, open]);
    return <ReportContext.Provider value={open}>
        {failedOpen?.sessionKey === sessionKey && <OpenError role="alert">
            {t('reportOpenMainFailed')}<button type="button" onClick={() => open(failedOpen.report)}>{t('reportRetry')}</button>
        </OpenError>}
        <Workspace data-preview={Boolean(selected)}>
            <div className="chat-column">{children}</div>
            {selected && <ReportPreview key={reportKey(selected)} report={selected} onClose={() => setSelection(undefined)} />}
        </Workspace>
    </ReportContext.Provider>;
};

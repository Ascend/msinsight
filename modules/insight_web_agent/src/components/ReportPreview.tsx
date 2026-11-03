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
import React, { useEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import { fetchReport } from '../api';
import { reportSrcDoc, type ReportArtifact, type ReportDocument } from '../artifacts';

const Preview = styled.section`
    min-width: 0; flex: 1; display: flex; flex-direction: column;
    background: ${(p): string => p.theme.bgColorLight};
    .report-toolbar { display: flex; align-items: center; gap: 8px; padding: 14px 16px; flex-wrap: wrap; }
    .report-heading { flex: 1; min-width: 80px; }
    .report-heading small { font-size: 10px; letter-spacing: 1px; color: #547de8; }
    h2 { font-size: 14px; line-height: 22px; margin: 3px 0 0; overflow-wrap: anywhere;
        color: ${(p): string => p.theme.textColorPrimary}; }
    button, select { padding: 6px 9px; border: 1px solid ${(p): string => p.theme.borderColor};
        border-radius: 5px; background: ${(p): string => p.theme.bgColor}; color: inherit; cursor: pointer; font-size: 12px; }
    button:hover { border-color: #547de8; }
    button:focus-visible, select:focus-visible { outline: 2px solid #547de8; outline-offset: 2px; }
    .report-version-select { position: relative; display: inline-flex; }
    .report-version-select select { appearance: none; min-width: 72px; padding: 6px 32px 6px 12px; }
    .report-version-select::after { content: ''; position: absolute; right: 12px; top: 50%;
        width: 6px; height: 6px; border-right: 1.5px solid currentColor; border-bottom: 1.5px solid currentColor;
        transform: translateY(-70%) rotate(45deg); pointer-events: none; }
    .report-canvas { position: relative; flex: 1; min-height: 0; margin: 0 12px 12px; border-radius: 8px;
        overflow: hidden; border: 1px solid ${(p): string => p.theme.borderColor}; background: white; }
    iframe { display: block; width: 100%; height: 100%; border: 0; background: white; }
    .report-state { display: flex; height: 100%; padding: 24px; flex-direction: column; align-items: center;
        justify-content: center; gap: 16px; color: #687182; text-align: center; font-size: 13px; }
    .report-footer { padding: 0 16px 12px; font-size: 11px; opacity: .6; }
`;

export const ReportPreview = ({ report, onClose }: {
    report: ReportArtifact; onClose: () => void;
}): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const [version, setVersion] = useState(report.version);
    const [document, setDocument] = useState<ReportDocument>();
    const [error, setError] = useState(false);
    const [attempt, setAttempt] = useState(0);
    const closeRef = useRef<HTMLButtonElement>(null);
    useEffect(() => {
        const previousFocus = window.document.activeElement as HTMLElement | null;
        closeRef.current?.focus();
        return () => { if (previousFocus?.isConnected) previousFocus.focus(); };
    }, []);
    useEffect(() => {
        const controller = new AbortController();
        setError(false);
        void fetchReport(report.artifactId, version, controller.signal).then((value) => {
            if (!controller.signal.aborted) setDocument(value);
        }).catch(() => { if (!controller.signal.aborted) setError(true); });
        return () => controller.abort();
    }, [report.artifactId, version, attempt]);
    const ready = document?.version === version && !error;
    const srcDoc = useMemo(() => ready && document ? reportSrcDoc(document.html) : undefined, [ready, document]);
    return <Preview aria-label={t('reportPreview')} onKeyDown={(event) => { if (event.key === 'Escape') onClose(); }}>
        <div className="report-toolbar">
            <div className="report-heading"><small>{t('reportPreview')}</small><h2>{ready ? document?.title : report.title}</h2></div>
            <span className="report-version-select">
                <select aria-label={t('reportVersion')} value={version} onChange={(event) => setVersion(Number(event.target.value))}>
                    {(document?.versions ?? [report]).map((item) => <option value={item.version} key={item.version}>V{item.version}</option>)}
                </select>
            </span>
            <button type="button" ref={closeRef} onClick={onClose} aria-label={t('reportClose')}>✕</button>
        </div>
        <div className="report-canvas">
            {error
                ? <div className="report-state" role="alert">{t('reportLoadFailed')}
                    <button type="button" onClick={() => setAttempt((value) => value + 1)}>{t('reportRetry')}</button></div>
                : srcDoc
                    ? <iframe key={version} title={document?.title ?? report.title} sandbox="allow-scripts" referrerPolicy="no-referrer" srcDoc={srcDoc} />
                    : <div className="report-state" role="status">{t('reportLoading')}</div>}
        </div>
        <div className="report-footer">{t('reportSnapshot')}</div>
    </Preview>;
};

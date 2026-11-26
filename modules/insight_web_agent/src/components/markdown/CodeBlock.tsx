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
import styled from '@emotion/styled';
import { useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import checkIcon from '../../icons/check.svg';
import copyIcon from '../../icons/copy.svg';
import { displayLanguage, escapeHtml, highlightCode } from './highlight';

const COLLAPSE_LINE_LIMIT = 30;
const MAX_HIGHLIGHT_LENGTH = 200000;

const copyToClipboard = async (value: string): Promise<boolean> => {
    if (navigator.clipboard?.writeText) {
        try {
            await navigator.clipboard.writeText(value);
            return true;
        } catch {
            // Falls back to the execCommand path below for restricted renderers.
        }
    }
    const textarea = document.createElement('textarea');
    textarea.value = value;
    textarea.setAttribute('readonly', '');
    textarea.style.position = 'fixed';
    textarea.style.top = '-9999px';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    try {
        textarea.select();
        return document.execCommand?.('copy') === true;
    } catch {
        return false;
    } finally {
        document.body.removeChild(textarea);
    }
};

const Shell = styled.div`
    min-width: 0;
    max-width: 100%;
    margin: 0;
    border: 1px solid ${(props): string => props.theme.borderColor};
    border-radius: ${(props): string => props.theme.borderRadiusSmall};
    background: ${(props): string => props.theme.bgColorDark};
    overflow: hidden;

    .md-code-toolbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
        padding: 3px 8px;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        background: ${(props): string => props.theme.bgColorLight};
    }

    .md-code-language {
        min-width: 0;
        overflow: hidden;
        color: ${(props): string => props.theme.textColorSecondary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 11px;
        line-height: 20px;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .md-code-actions {
        display: flex;
        align-items: center;
        flex: 0 0 auto;
        gap: 2px;
    }

    .md-code-button {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 2px 6px;
        background: transparent;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 11px;
        line-height: 16px;
        cursor: pointer;
    }

    .md-code-button:hover {
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.primaryColor};
    }

    .md-code-button:focus-visible {
        outline: 2px solid ${(props): string => props.theme.primaryColor};
        outline-offset: 1px;
    }

    .md-code-button-icon {
        width: 12px;
        height: 12px;
    }

    .rich-text & .md-code-pre,
    & .md-code-pre {
        max-width: 100%;
        margin: 0;
        border: 0;
        border-radius: 0;
        padding: 10px 12px;
        background: transparent;
        overflow-x: auto;
        overflow-y: hidden;
    }

    & .md-code-pre code {
        border: 0;
        padding: 0;
        background: transparent;
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
        line-height: 1.55;
        white-space: pre;
        tab-size: 4;
    }

    &.wrapped .md-code-pre {
        overflow-x: hidden;
    }

    &.wrapped .md-code-pre code {
        overflow-wrap: anywhere;
        white-space: pre-wrap;
        word-break: break-word;
    }

    &.clamped .md-code-pre {
        max-height: 420px;
        overflow-y: hidden;
    }

    .hljs-comment,
    .hljs-quote {
        color: ${(props): string => props.theme.textColorSecondary};
        font-style: italic;
    }

    .hljs-keyword,
    .hljs-selector-tag,
    .hljs-literal,
    .hljs-doctag {
        color: ${(props): string => props.theme.primaryColor};
    }

    .hljs-string,
    .hljs-regexp,
    .hljs-addition,
    .hljs-attribute,
    .hljs-meta .hljs-string {
        color: ${(props): string => props.theme.successColor};
    }

    .hljs-number,
    .hljs-symbol,
    .hljs-bullet,
    .hljs-variable,
    .hljs-template-variable,
    .hljs-link {
        color: ${(props): string => props.theme.warningColor};
    }

    .hljs-title,
    .hljs-section,
    .hljs-title.class_,
    .hljs-title.function_ {
        color: ${(props): string => props.theme.primaryColorDark};
        font-weight: 600;
    }

    .hljs-type,
    .hljs-built_in,
    .hljs-class .hljs-title {
        color: ${(props): string => props.theme.primaryColorHover};
    }

    .hljs-attr,
    .hljs-name,
    .hljs-selector-id,
    .hljs-selector-class,
    .hljs-template-tag {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .hljs-meta,
    .hljs-tag {
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .hljs-deletion {
        color: ${(props): string => props.theme.dangerColor};
    }

    .hljs-emphasis {
        font-style: italic;
    }

    .hljs-strong {
        font-weight: 600;
    }
`;

export const CodeBlock = ({
    code,
    language,
    streaming = false,
}: {
    code: string;
    language?: string;
    streaming?: boolean;
}): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const [copyResult, setCopyResult] = useState<'idle' | 'copied' | 'failed'>('idle');
    const [wrapped, setWrapped] = useState(false);
    const [expanded, setExpanded] = useState(false);
    const timerRef = useRef<number | undefined>(undefined);

    useEffect(() => () => {
        if (timerRef.current !== undefined) window.clearTimeout(timerRef.current);
    }, []);

    const collapsible = useMemo(() => code.split('\n').length > COLLAPSE_LINE_LIMIT, [code]);
    const highlighted = useMemo(() => {
        if (streaming || code.length > MAX_HIGHLIGHT_LENGTH) return escapeHtml(code);
        return highlightCode(code, language);
    }, [code, language, streaming]);

    const handleCopy = async (): Promise<void> => {
        setCopyResult(await copyToClipboard(code) ? 'copied' : 'failed');
        if (timerRef.current !== undefined) window.clearTimeout(timerRef.current);
        timerRef.current = window.setTimeout(() => setCopyResult('idle'), 2000);
    };

    const copied = copyResult === 'copied';
    const copyLabel = t(copied ? 'codeCopied' : copyResult === 'failed' ? 'codeCopyFailed' : 'codeCopy');
    const clamped = collapsible && !expanded;
    const className = `md-code-block${wrapped ? ' wrapped' : ''}${clamped ? ' clamped' : ''}`;

    return (
        <Shell className={className}>
            <div className="md-code-toolbar">
                <span className="md-code-language">{displayLanguage(language)}</span>
                <div className="md-code-actions">
                    {collapsible ? (
                        <button
                            aria-expanded={expanded}
                            className="md-code-button"
                            onClick={() => setExpanded(current => !current)}
                            type="button"
                        >
                            {t(expanded ? 'codeCollapse' : 'codeExpand')}
                        </button>
                    ) : null}
                    <button
                        aria-pressed={wrapped}
                        className="md-code-button"
                        onClick={() => setWrapped(current => !current)}
                        title={t(wrapped ? 'codeWrapOff' : 'codeWrapOn')}
                        type="button"
                    >
                        {t(wrapped ? 'codeWrapOff' : 'codeWrapOn')}
                    </button>
                    <button
                        aria-label={copyLabel}
                        className="md-code-button"
                        onClick={handleCopy}
                        title={copyLabel}
                        type="button"
                    >
                        <img alt="" aria-hidden="true" className="md-code-button-icon" src={copied ? checkIcon : copyIcon} />
                        <span aria-live="polite">{copyLabel}</span>
                    </button>
                </div>
            </div>
            <pre className="md-code-pre">
                <code dangerouslySetInnerHTML={{ __html: highlighted }} />
            </pre>
        </Shell>
    );
};

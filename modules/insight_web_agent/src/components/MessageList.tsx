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
import type { TFunction } from 'i18next';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { useEffect, useRef, useState } from 'react';
import type React from 'react';
import { useTranslation } from 'react-i18next';
import type { ChatMessage, MessageContentBlock, PermissionDecision, ToolCallItem } from '../types';
import arrowDownIcon from '../icons/arrow-down.svg';
import { ActionBlock } from './ActionBlock';
import { parseActionMarkup } from './actionMarkup';

const markdownComponents = {
    a: ({ href, children, ...props }: React.AnchorHTMLAttributes<HTMLAnchorElement>): JSX.Element => {
        return (
            <a
                {...props}
                href={href}
                rel="noopener noreferrer"
                target="_blank"
            >
                {children}
            </a>
        );
    },
};

const Container = styled.div`
    display: flex;
    flex-direction: column;
    gap: 14px;

    .message-turn {
        min-width: 0;
        display: flex;
        flex-direction: column;
        gap: 14px;
    }

    .empty {
        margin: auto;
        max-width: 260px;
        color: ${(props): string => props.theme.textColorSecondary};
        line-height: 1.5;
        text-align: center;
    }

    .message {
        box-sizing: border-box;
        width: 100%;
        min-width: 0;
        max-width: 100%;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        padding: 12px 14px;
        line-height: 1.55;
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .message.user {
        position: sticky;
        top: 0;
        z-index: 4;
        display: grid;
        grid-template-columns: 24px minmax(0, 1fr);
        align-items: start;
        gap: 8px;
        overflow: hidden;
        border: 0;
        border-radius: 16px;
        padding: 12px 16px;
        background: ${(props): string => props.theme.mode === 'dark' ? props.theme.primaryColorLight3 : props.theme.primaryColorLight4};
    }

    .message.user:not(.overflowing) {
        grid-template-columns: minmax(0, 1fr);
    }

    .user-prompt-toggle {
        width: 24px;
        height: 24px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 0;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .user-prompt-toggle:disabled {
        cursor: default;
    }

    .user-prompt-chevron {
        width: 16px;
        height: 16px;
        background: currentColor;
        -webkit-mask: url(${arrowDownIcon}) center / contain no-repeat;
        mask: url(${arrowDownIcon}) center / contain no-repeat;
        transform: rotate(-90deg);
        transition: transform 0.18s ease;
    }

    .message.user.expanded .user-prompt-chevron {
        transform: rotate(0deg);
    }

    .user-prompt-shell {
        position: relative;
        min-width: 0;
    }

    .user-prompt-content {
        max-height: 65px;
        overflow: hidden;
        font-size: 14px;
        line-height: 1.55;
        transition: max-height 0.2s ease;
    }

    .message.user.expanded .user-prompt-content {
        max-height: 300px;
        overflow-y: auto;
        scrollbar-gutter: stable;
    }

    .message.user.overflowing:not(.expanded) .user-prompt-shell::after {
        position: absolute;
        right: 0;
        bottom: 0;
        left: 0;
        height: 28px;
        pointer-events: none;
        background: linear-gradient(
            to bottom,
            transparent,
            ${(props): string => props.theme.mode === 'dark' ? props.theme.primaryColorLight3 : props.theme.primaryColorLight4}
        );
        content: "";
    }

    .message.assistant {
        border: 0;
        padding: 0;
        background: transparent;
    }

    .thinking-details {
        min-width: 0;
        max-width: 100%;
        margin-bottom: 10px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        overflow-wrap: anywhere;
    }

    .thinking-details summary {
        width: fit-content;
        display: flex;
        align-items: center;
        gap: 4px;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        padding: 2px 4px;
        cursor: pointer;
        list-style: none;
    }

    .thinking-details summary::-webkit-details-marker {
        display: none;
    }

    .thinking-details summary:hover {
        background: ${(props): string => props.theme.bgColorLight};
    }

    .thinking-chevron {
        width: 14px;
        height: 14px;
        flex: 0 0 auto;
        background: currentColor;
        -webkit-mask: url(${arrowDownIcon}) center / contain no-repeat;
        mask: url(${arrowDownIcon}) center / contain no-repeat;
        transform: rotate(-90deg);
        transition: transform 0.18s ease;
    }

    .thinking-details[open] .thinking-chevron {
        transform: rotate(0deg);
    }

    .thinking-content {
        margin-top: 6px;
        padding-left: 18px;
        border-left: 2px solid ${(props): string => props.theme.borderColor};
    }

    .thinking-indicator {
        display: inline-flex;
        align-items: center;
        gap: 8px;
        margin-top: 10px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }

    .thinking-indicator::before {
        width: 8px;
        height: 8px;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: ${(props): string => props.theme.primaryColor};
        content: "";
        animation: pulse 1s ease-in-out infinite;
    }

    .answer-duration {
        margin-bottom: 16px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }

    .rich-text {
        min-width: 0;
        max-width: 100%;
        display: grid;
        gap: 8px;
        color: inherit;
        overflow-wrap: anywhere;
    }

    .rich-text :where(p, ul, ol, li, h1, h2, h3, h4, h5, h6, blockquote, table, th, td, code, pre) {
        color: inherit;
    }

    .rich-text a {
        color: inherit;
    }

    .rich-text.muted {
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .rich-text p,
    .rich-text ul,
    .rich-text ol,
    .rich-text li,
    .rich-text pre,
    .rich-text h3,
    .rich-text h4,
    .rich-text h5 {
        min-width: 0;
        max-width: 100%;
        margin: 0;
        overflow-wrap: anywhere;
    }

    .rich-text ul {
        padding-left: 18px;
    }

    .rich-text pre {
        max-width: 100%;
        overflow-x: auto;
        overflow-y: hidden;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 10px;
        background: ${(props): string => props.theme.bgColorDark};
    }

    .rich-text table {
        display: block;
        max-width: 100%;
        overflow-x: auto;
        overflow-y: hidden;
        border-collapse: collapse;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
    }

    .rich-text th,
    .rich-text td {
        padding: 6px 10px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        text-align: left;
        vertical-align: top;
    }

    .rich-text th {
        background: ${(props): string => props.theme.bgColorDark};
        font-weight: 700;
    }

    .rich-text code {
        overflow-wrap: anywhere;
        white-space: normal;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        padding: 2px 8px;
        background: ${(props): string => props.theme.bgColorLight};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 0.92em;
    }

    .rich-text pre code {
        overflow-wrap: normal;
        white-space: pre;
        border: 0;
        padding: 0;
        background: transparent;
    }

    .action-literal {
        max-width: 100%;
        margin: 0;
        overflow-x: auto;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 8px;
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
        white-space: pre-wrap;
    }

    .tool-calls {
        display: grid;
        gap: 6px;
        margin: 8px 0;
    }

    .tool-call {
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: ${(props): string => props.theme.bgColorLight};
        font-size: 12px;
    }

    .tool-call summary {
        display: grid;
        min-width: 0;
        grid-template-columns: auto minmax(0, 1fr) auto;
        align-items: center;
        gap: 6px 8px;
        cursor: pointer;
        padding: 7px 9px;
        list-style: none;
    }

    .tool-call summary::-webkit-details-marker {
        display: none;
    }

    .tool-status {
        width: 8px;
        height: 8px;
        flex: 0 0 auto;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: ${(props): string => props.theme.textColorSecondary};
    }

    .tool-call.in_progress .tool-status {
        background: ${(props): string => props.theme.primaryColor};
        animation: pulse 1s ease-in-out infinite;
    }

    .tool-call.completed .tool-status {
        background: ${(props): string => props.theme.successColor};
    }

    .tool-call.failed .tool-status {
        background: ${(props): string => props.theme.dangerColor};
    }

    .tool-summary {
        min-width: 0;
        display: grid;
        gap: 2px;
    }

    .tool-name {
        min-width: 0;
        font-weight: 700;
        overflow-wrap: anywhere;
    }

    .tool-target {
        min-width: 0;
        color: ${(props): string => props.theme.textColorSecondary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        overflow-wrap: anywhere;
        white-space: pre-wrap;
        word-break: break-word;
    }

    .tool-state {
        align-self: start;
        color: ${(props): string => props.theme.textColorSecondary};
        white-space: nowrap;
    }

    .tool-details {
        display: grid;
        gap: 8px;
        padding: 0 9px 9px 25px;
    }

    .tool-section {
        min-width: 0;
    }

    .tool-label {
        margin-bottom: 3px;
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .tool-value {
        max-height: 180px;
        margin: 0;
        overflow: auto;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 7px;
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.textColorPrimary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 11px;
        overflow-wrap: anywhere;
        white-space: pre-wrap;
        word-break: break-word;
    }

    .permission-card {
        display: grid;
        gap: 10px;
    }

    .permission-title {
        font-weight: 700;
    }

    .permission-path {
        overflow-wrap: anywhere;
        border: 1px solid ${(props): string => props.theme.borderColorLight};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 8px;
        background: ${(props): string => props.theme.bgColorDark};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
    }

    .permission-actions {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
    }

    .permission-actions button {
        cursor: pointer;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 5px 10px;
        background: ${(props): string => props.theme.bgColor};
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .permission-actions button.primary {
        border-color: ${(props): string => props.theme.primaryColor};
        color: ${(props): string => props.theme.primaryColor};
    }

    .permission-actions button:disabled {
        cursor: not-allowed;
        opacity: 0.6;
    }

    .permission-state,
    .permission-error {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
    }

    .permission-error {
        color: ${(props): string => props.theme.dangerColor};
    }

    .attachments {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
        margin-top: 8px;
    }

    .attachment {
        min-width: 0;
        display: grid;
        gap: 6px;
        padding: 8px;
        border: 1px solid ${(props): string => props.theme.borderColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: ${(props): string => props.theme.bgColor};
    }

    .attachment img {
        width: 72px;
        height: 72px;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        object-fit: cover;
    }

    @keyframes pulse {
        0%, 100% {
            opacity: 0.35;
            transform: scale(0.82);
        }

        50% {
            opacity: 1;
            transform: scale(1);
        }
    }
`;

interface MessageListProps {
    messages: ChatMessage[];
    pendingPrompt: boolean;
    onPermissionDecision: (sessionId: string, requestId: string, decision: PermissionDecision) => Promise<void>;
}

export const MessageList = ({ messages, pendingPrompt, onPermissionDecision }: MessageListProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const now = useToolClock(pendingPrompt);
    if (!messages.length) {
        return <Container><div className="empty">{t('noLocalMessages')}</div></Container>;
    }

    return (
        <Container>
            {groupMessagesIntoTurns(messages).map((turn) => (
                <section className="message-turn" key={turn[0].message.id}>
                    {turn.map(({ message, index }) => {
                        if (isHiddenPermissionMessage(message)) return null;
                        if (message.role === 'user') {
                            return <UserPromptCard key={message.id} message={message} />;
                        }
                        return (
                            <article className={`message ${message.role}`} key={message.id}>
                                {message.startedAt !== undefined
                                    ? <div className="answer-duration">{t('answerDuration', {
                                        duration: formatDuration(message.durationMs ?? now - message.startedAt),
                                    })}</div>
                                    : null}
                                {message.content.map((block) => <ContentBlock
                                    allowActions={message.role === 'assistant'}
                                    block={block}
                                    key={block.id}
                                    streaming={isStreamingAssistantMessage(messages, index, pendingPrompt)}
                                />)}
                                {message.permission
                                    ? <PermissionCard message={message} onDecision={onPermissionDecision} />
                                    : null}
                                {message.activity === 'analyzing_tool_results'
                                    ? <div className="thinking-indicator">{t('analyzingToolResults')}</div>
                                    : null}
                                {typeof message.activity === 'object' && message.activity.type === 'model_retry'
                                    ? <div className="thinking-indicator">{t('modelRetrying', {
                                        attempt: message.activity.attempt,
                                        maxAttempts: message.activity.maxAttempts,
                                        wait: formatRetryWait(message.activity.retryAfterSeconds, t),
                                    })}</div>
                                    : null}
                                {isThinkingMessage(messages, index, pendingPrompt)
                                    ? <div className="thinking-indicator">{t('thinking')}</div>
                                    : null}
                            </article>
                        );
                    })}
                </section>
            ))}
        </Container>
    );
};

const groupMessagesIntoTurns = (messages: ChatMessage[]): Array<Array<{ message: ChatMessage; index: number }>> => {
    return messages.reduce<Array<Array<{ message: ChatMessage; index: number }>>>((turns, message, index) => {
        if (message.role === 'user' || !turns.length) turns.push([]);
        turns[turns.length - 1].push({ message, index });
        return turns;
    }, []);
};

const COLLAPSED_PROMPT_HEIGHT = 65;

const UserPromptCard = ({ message }: { message: ChatMessage }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const contentRef = useRef<HTMLDivElement>(null);
    const [expanded, setExpanded] = useState(false);
    const [overflowing, setOverflowing] = useState(false);

    useEffect(() => {
        const content = contentRef.current;
        if (!content) return undefined;
        const measure = (): void => setOverflowing(content.scrollHeight > COLLAPSED_PROMPT_HEIGHT + 1);
        measure();
        const observer = typeof ResizeObserver === 'undefined' ? undefined : new ResizeObserver(measure);
        observer?.observe(content);
        window.addEventListener('resize', measure);
        return () => {
            observer?.disconnect();
            window.removeEventListener('resize', measure);
        };
    }, [message.content]);

    const handleWheel = (event: React.WheelEvent<HTMLDivElement>): void => {
        if (!expanded) return;
        const content = event.currentTarget;
        const canScrollUp = event.deltaY < 0 && content.scrollTop > 0;
        const canScrollDown = event.deltaY > 0 && content.scrollTop + content.clientHeight < content.scrollHeight - 1;
        if (canScrollUp || canScrollDown) event.stopPropagation();
    };

    const toggleExpanded = (): void => {
        if (expanded && contentRef.current) contentRef.current.scrollTop = 0;
        setExpanded((current) => !current);
    };

    const className = `message user${overflowing ? ' overflowing' : ''}${expanded ? ' expanded' : ''}`;
    return (
        <article className={className}>
            {overflowing ? (
                <button
                    aria-label={expanded ? t('collapse') : t('expand')}
                    className="user-prompt-toggle"
                    onClick={toggleExpanded}
                    type="button"
                >
                    <span aria-hidden="true" className="user-prompt-chevron" />
                </button>
            ) : null}
            <div className="user-prompt-shell">
                <div className="user-prompt-content" onWheel={handleWheel} ref={contentRef}>
                    {message.content.map((block) => <ContentBlock allowActions={false} block={block} key={block.id} streaming={false} />)}
                </div>
            </div>
        </article>
    );
};

const ContentBlock = ({ allowActions, block, streaming }: { allowActions: boolean; block: MessageContentBlock; streaming: boolean }): JSX.Element => {
    if (block.type === 'thinking') {
        return <ThinkingBlock text={block.text} />;
    }
    if (block.type === 'text') {
        const segments = allowActions
            ? parseActionMarkup(block.text, { streaming, keyPrefix: block.id })
            : [{ key: block.id, type: 'markdown' as const, text: block.text }];
        return <>{segments.map((segment) => {
            if (segment.type === 'action') return <ActionBlock action={segment.action} key={segment.key} />;
            if (segment.type === 'literal') return <pre className="action-literal" key={segment.key}>{segment.text}</pre>;
            return <div className="rich-text" key={segment.key}><ReactMarkdown components={markdownComponents} remarkPlugins={[remarkGfm]}>{segment.text}</ReactMarkdown></div>;
        })}</>;
    }
    return <ToolCalls toolCalls={[block.toolCall]} />;
};

const ThinkingBlock = ({ text }: { text: string }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    return (
        <details className="thinking-details">
            <summary>
                <span aria-hidden="true" className="thinking-chevron" />
                <span>{t('thinkingProcess')}</span>
            </summary>
            <div className="thinking-content rich-text">
                <ReactMarkdown components={markdownComponents} remarkPlugins={[remarkGfm]}>{text}</ReactMarkdown>
            </div>
        </details>
    );
};

const isHiddenPermissionMessage = (message: ChatMessage): boolean => {
    const state = message.permission?.state;
    return state === 'allowed_once' || state === 'allowed_always';
};

const ToolCalls = ({ toolCalls }: { toolCalls: ToolCallItem[] }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const hasRunningTool = toolCalls.some((toolCall) => toolCall.status === 'in_progress');
    const now = useToolClock(hasRunningTool);
    return (
        <div className="tool-calls">
            {toolCalls.map((toolCall) => (
                <details className={`tool-call ${toolCall.status}`} key={toolCall.toolCallId}>
                    <summary>
                        <span className="tool-status" />
                        <span className="tool-summary">
                            <span className="tool-name">{toolCallSummary(toolCall, t)}</span>
                            {toolCallTarget(toolCall.input) ? <span className="tool-target">⎿ {toolCallTarget(toolCall.input)}</span> : null}
                        </span>
                        <span className="tool-state">{toolCallState(toolCall, t)} · {formatToolDuration(toolCall, now)}</span>
                    </summary>
                    {toolCall.input || toolCall.output ? (
                        <div className="tool-details">
                            {toolCall.input ? <ToolSection label={t('toolInput')} value={toolCall.input} /> : null}
                            {toolCall.output ? <ToolSection label={toolCall.status === 'failed' ? t('toolError') : t('toolOutput')} value={toolCall.output} /> : null}
                        </div>
                    ) : null}
                </details>
            ))}
        </div>
    );
};

const ToolSection = ({ label, value }: { label: string; value: string }): JSX.Element => (
    <div className="tool-section">
        <div className="tool-label">{label}</div>
        <pre className="tool-value">{value}</pre>
    </div>
);

const useToolClock = (enabled: boolean): number => {
    const [now, setNow] = useState(Date.now());
    useEffect(() => {
        if (!enabled) return undefined;
        setNow(Date.now());
        const timer = window.setInterval(() => setNow(Date.now()), 1000);
        return () => window.clearInterval(timer);
    }, [enabled]);
    return now;
};

const formatRetryWait = (seconds: number | undefined, t: TFunction): string => {
    if (seconds === undefined) return t('modelRetryWaitUnknown');
    if (seconds < 60) return t('modelRetryWaitSeconds', { count: seconds });
    const minutes = Math.floor(seconds / 60);
    const remainder = seconds % 60;
    return remainder
        ? t('modelRetryWaitMinutesSeconds', { minutes, seconds: remainder })
        : t('modelRetryWaitMinutes', { count: minutes });
};

const formatToolDuration = (toolCall: ToolCallItem, now: number): string => formatDuration(
    toolCall.status === 'in_progress' ? now - (toolCall.startedAt ?? now) : toolCall.durationMs ?? 0,
);

const formatDuration = (value: number): string => {
    const durationMs = Math.max(0, value);
    if (durationMs < 1000) return `${durationMs}ms`;
    if (durationMs < 10000) return `${(durationMs / 1000).toFixed(1)}s`;
    return `${Math.round(durationMs / 1000)}s`;
};

const toolCallState = (toolCall: ToolCallItem, t: TFunction): string => {
    if (toolCall.status === 'completed') return t('toolCompleted');
    if (toolCall.status === 'failed') return t('toolFailed');
    return t('toolRunning');
};

const toolCallSummary = (toolCall: ToolCallItem, t: TFunction): string => {
    if (toolCall.status === 'in_progress' && toolCall.progress) return toolCall.progress;
    const displayName = toolCallDisplayName(toolCall);
    if (toolCall.status === 'completed') return t('toolCallCompleted', { name: displayName });
    if (toolCall.status === 'failed') return t('toolCallFailed', { name: displayName });
    return t('toolCallRunning', { name: displayName });
};

export const toolCallDisplayName = (toolCall: ToolCallItem): string => {
    if (toolCall.name !== 'msinsight' || !toolCall.input) return toolCall.name;
    try {
        const command = (JSON.parse(toolCall.input) as Record<string, unknown>).command;
        return typeof command === 'string' && command.trim() ? command.trim() : toolCall.name;
    } catch (_error) {
        return toolCall.name;
    }
};

const TOOL_TARGET_MAX_LENGTH = 50;

const toolCallTarget = (input?: string): string | undefined => {
    if (!input) return undefined;
    try {
        const value = JSON.parse(input) as Record<string, unknown>;
        const path = value.file_path ?? value.path;
        if (typeof path === 'string' && path.trim()) return compactPath(path.trim(), TOOL_TARGET_MAX_LENGTH);
        const target = value.pattern ?? value.query ?? value.command;
        if (typeof target === 'string' && target.trim()) return truncateTarget(target.trim(), TOOL_TARGET_MAX_LENGTH);
    } catch (_error) {
        return truncateTarget(input.trim(), TOOL_TARGET_MAX_LENGTH) || undefined;
    }
    return undefined;
};

const compactPath = (path: string, maxLength: number): string => {
    if (path.length <= maxLength) return path;
    const separator = path.includes('\\') ? '\\' : '/';
    const segments = path.split(/[\\/]+/).filter(Boolean);
    const fileName = segments[segments.length - 1] ?? path;
    const shortSuffix = `…${separator}${fileName}`;
    if (shortSuffix.length >= maxLength) return `…${fileName.slice(-(maxLength - 1))}`;
    const firstSegment = segments.length > 1 ? segments[0] : '';
    const prefix = path.startsWith(separator) ? separator : firstSegment;
    const suffix = prefix.endsWith(separator)
        ? `…${separator}${fileName}`
        : `${separator}…${separator}${fileName}`;
    const compactPrefix = prefix.slice(0, maxLength - suffix.length);
    return compactPrefix ? `${compactPrefix}${suffix}` : shortSuffix;
};

const truncateTarget = (target: string, maxLength: number): string => (
    target.length <= maxLength ? target : `${target.slice(0, maxLength - 1)}…`
);

const PermissionCard = ({
    message,
    onDecision,
}: {
    message: ChatMessage;
    onDecision: (sessionId: string, requestId: string, decision: PermissionDecision) => Promise<void>;
}): JSX.Element | null => {
    const { t } = useTranslation('insightWebAgent');
    const permission = message.permission;
    if (!permission) return null;
    const pending = permission.state === 'pending';
    return (
        <div className="permission-card">
            <div className="permission-title">{permission.title ?? (permission.kind === 'bash' ? t('allowBashCommand') : t('allowFileRead'))}</div>
            <div className="permission-path" title={permission.target}>{truncatePermissionTarget(permission.target)}</div>
            {permission.kind === 'bash' && permission.details?.cwd ? <div className="permission-state">{t('workingDirectory')}: {String(permission.details.cwd)}</div> : null}
            {pending ? (
                <div className="permission-actions">
                    <button
                        className="primary"
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { onDecision(permission.sessionId, permission.requestId, 'allow_once'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'allow_once' ? t('allowing') : t('allowOnce')}
                    </button>
                    <button
                        className="primary"
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { onDecision(permission.sessionId, permission.requestId, 'allow_always'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'allow_always' ? t('allowing') : t('allowAlways')}
                    </button>
                    <button
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { onDecision(permission.sessionId, permission.requestId, 'deny'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'deny' ? t('denying') : t('deny')}
                    </button>
                </div>
            ) : <div className="permission-state">{permissionStateText(permission.state, t)}</div>}
            {permission.error ? <div className="permission-error">{permission.error}</div> : null}
        </div>
    );
};

const truncatePermissionTarget = (target: string): string => target.length <= 2000 ? target : `${target.slice(0, 1999)}…`;

const permissionStateText = (state: NonNullable<ChatMessage['permission']>['state'], t: TFunction): string => {
    if (state === 'allowed_once') return t('allowedOnce');
    if (state === 'allowed_always') return t('allowedAlways');
    if (state === 'denied') return t('denied');
    if (state === 'expired') return t('expired');
    if (state === 'invalidated') return t('invalidated');
    return t('pending');
};

const isStreamingAssistantMessage = (messages: ChatMessage[], index: number, pendingPrompt: boolean): boolean => {
    const message = messages[index];
    if (!pendingPrompt || message.role !== 'assistant' || message.permission) return false;
    return !messages.slice(index + 1).some(item => item.role === 'assistant' && !item.permission);
};

const isThinkingMessage = (messages: ChatMessage[], index: number, pendingPrompt: boolean): boolean => {
    if (!pendingPrompt) return false;
    const message = messages[index];
    const hasAnswerContent = message.content.some((block) => block.type === 'text' && block.text.trim());
    return message.role === 'assistant' &&
        index === messages.length - 1 &&
        !message.permission &&
        !message.activity &&
        !hasAnswerContent;
};

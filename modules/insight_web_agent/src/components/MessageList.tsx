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
import { Fragment, useEffect, useRef, useState } from 'react';
import type React from 'react';
import { useTranslation } from 'react-i18next';
import type { ChatMessage, ConversationNotice, MessageContentBlock, PermissionDecision, ToolCallItem } from '../types';
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

    .user-prompt-sticky {
        position: sticky;
        top: 0;
        z-index: 4;
        min-width: 0;
        background: ${(props): string => props.theme.bgColor};
    }

    .user-prompt-fade {
        visibility: hidden;
        position: absolute;
        top: 100%;
        right: 0;
        left: 0;
        height: 46px;
        background: linear-gradient(180deg, ${(props): string => props.theme.bgColor} -1.275%, transparent 148.726%);
        pointer-events: none;
    }

    .user-prompt-sticky.stuck .user-prompt-fade {
        visibility: visible;
    }

    .message.user {
        position: relative;
        z-index: 1;
        display: grid;
        grid-template-columns: 24px minmax(0, 1fr);
        align-items: start;
        gap: 8px;
        overflow: hidden;
        border: 0;
        border-radius: 16px 0 16px 16px;
        padding: 12px 16px;
        background: ${(props): string => props.theme.agentUserMessageBackgroundColor};
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
            ${(props): string => props.theme.agentUserMessageBackgroundColor}
        );
        content: "";
    }

    .message.assistant {
        border: 0;
        padding: 0;
        background: transparent;
    }

    .thinking-summary,
    .thinking-timeline {
        min-width: 0;
        max-width: 100%;
        margin-bottom: 16px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 14px;
        overflow-wrap: anywhere;
    }

    .thinking-summary {
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .thinking-timeline > summary {
        width: fit-content;
        display: flex;
        align-items: center;
        gap: 8px;
        cursor: pointer;
        list-style: none;
        user-select: none;
    }

    .thinking-sparkle {
        display: inline-flex;
        align-items: center;
        flex: 0 0 auto;
    }

    .thinking-sparkle::before {
        width: 8px;
        height: 8px;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: rgba(75, 112, 247, 1);
        content: "";
        animation: pulse 1s ease-in-out infinite;
    }

    .thinking-timeline > summary::-webkit-details-marker {
        display: none;
    }

    .thinking-chevron,
    .timeline-tool-chevron {
        width: 16px;
        height: 16px;
        flex: 0 0 auto;
        background: currentColor;
        -webkit-mask: url(${arrowDownIcon}) center / contain no-repeat;
        mask: url(${arrowDownIcon}) center / contain no-repeat;
        transform: rotate(-90deg);
        transition: transform 0.18s ease;
    }

    .timeline-tool-chevron {
        width: 12px;
        height: 12px;
        margin-top: 3px;
    }

    .thinking-timeline[open] > summary .thinking-chevron,
    .timeline-tool-details[open] > summary .timeline-tool-chevron {
        transform: rotate(0deg);
    }

    .thinking-summary-label,
    .timeline-title {
        color: ${(props): string => props.theme.textColorPrimary};
        font-weight: 500;
    }

    .thinking-summary-duration,
    .timeline-duration {
        color: ${(props): string => props.theme.textColorSecondary};
        white-space: nowrap;
    }

    .thinking-timeline[open] > .timeline-list {
        margin-top: 14px;
    }

    .timeline-list {
        position: relative;
        display: grid;
        gap: 0;
        padding-left: 30px;
    }

    .timeline-item {
        position: relative;
        min-width: 0;
        padding: 0 0 20px;
    }

    .timeline-item:last-child {
        padding-bottom: 0;
    }

    .timeline-item:not(:last-child)::before {
        position: absolute;
        top: calc((1.5em + 8px) / 2 + 3px);
        bottom: calc((8px - 1.5em) / 2 + 3px);
        left: -23px;
        width: 1px;
        background: ${(props): string => props.theme.borderColor};
        content: "";
    }

    .timeline-marker {
        position: absolute;
        z-index: 1;
        top: calc((1.5em - 8px) / 2);
        left: -27px;
        width: 8px;
        height: 8px;
        border-radius: ${(props): string => props.theme.borderRadiusCircle};
        background: rgba(191, 191, 191, 1);
    }

    .timeline-item.failed .timeline-marker {
        background: ${(props): string => props.theme.dangerColor};
    }

    .timeline-list.processing > .timeline-item:last-child > .timeline-marker {
        background: ${(props): string => props.theme.primaryColor};
        animation: pulse 1.6s ease-in-out infinite;
    }

    @media (prefers-reduced-motion: reduce) {
        .timeline-list.processing > .timeline-item:last-child > .timeline-marker {
            animation: none;
        }
    }

    .timeline-heading {
        min-width: 0;
        display: flex;
        align-items: baseline;
        flex-wrap: wrap;
        gap: 0 12px;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        line-height: 1.5;
    }

    .timeline-body {
        margin-top: 6px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 14px;
        line-height: 1.7;
    }

    .timeline-body.rich-text {
        display: grid;
        gap: 6px;
    }

    .thinking-content {
        color: rgba(119, 119, 119, 1);
        font-size: 14px;
        font-weight: 400;
    }

    .thinking-content :where(strong, b) {
        font-weight: 400;
    }

    .timeline-item.thinking .timeline-title {
        color: rgba(119, 119, 119, 1);
        font-size: 14px;
        font-weight: 400;
    }

    .timeline-item.tool .timeline-heading {
        display: grid;
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: start;
    }

    .timeline-item.tool .timeline-title {
        min-width: 0;
        font-weight: 400;
        overflow-wrap: anywhere;
    }

    .timeline-item.tool .tool-state {
        font-size: 12px;
        line-height: 21px;
    }

    .timeline-tool-details {
        margin-top: 2px;
    }

    .timeline-tool-details > summary {
        width: fit-content;
        max-width: 100%;
        display: flex;
        align-items: flex-start;
        gap: 4px;
        cursor: pointer;
        list-style: none;
        user-select: none;
        color: ${(props): string => props.theme.textColorSecondary};
    }

    .timeline-tool-details > summary::-webkit-details-marker {
        display: none;
    }

    .timeline-tool-details > summary:hover,
    .timeline-tool-details > summary:hover .timeline-tool-target {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .timeline-tool-target {
        min-width: 0;
        margin-top: 2px;
        color: ${(props): string => props.theme.textColorSecondary};
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace;
        font-size: 12px;
        line-height: 18px;
        overflow-wrap: anywhere;
        white-space: pre-wrap;
    }

    .timeline-tool-details > summary .timeline-tool-target {
        margin-top: 0;
    }

    .timeline-tool-sections {
        display: grid;
        gap: 8px;
        margin: 6px 0 0 16px;
    }

    .timeline-answer .timeline-title,
    .timeline-item.analyzing .timeline-title {
        font-weight: 400;
    }

    .model-retry-alert {
        margin-top: 10px;
        border: 1px solid ${(props): string => props.theme.warningColor};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 8px 12px;
        background: ${(props): string => props.theme.warningColorLight5};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        line-height: 1.5;
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
        font-weight: 400;
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
    notices?: ConversationNotice[];
    pendingPrompt: boolean;
    onPermissionDecision: (sessionId: string, requestId: string, decision: PermissionDecision) => Promise<void>;
}

export const MessageList = ({ messages, notices = [], pendingPrompt, onPermissionDecision }: MessageListProps): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const now = useToolClock(pendingPrompt);
    if (!messages.length) {
        return <Container>
            <div className="empty">{t('noLocalMessages')}</div>
            {notices.map((notice) => <ModelSwitchNotice key={notice.id} notice={notice} />)}
        </Container>;
    }

    const messageIds = new Set(messages.map((message) => message.id));
    const leadingNotices = notices.filter((notice) => !notice.afterMessageId);
    const trailingNotices = notices.filter((notice) => Boolean(notice.afterMessageId) && !messageIds.has(notice.afterMessageId as string));

    return (
        <Container>
            {leadingNotices.map((notice) => <ModelSwitchNotice key={notice.id} notice={notice} />)}
            {groupMessagesIntoTurns(messages).map((turn) => (
                <section className="message-turn" key={turn[0].message.id}>
                    {turn.map(({ message, index }) => {
                        const attachedNotices = notices.filter((notice) => notice.afterMessageId === message.id).map((notice) => (
                            <ModelSwitchNotice key={notice.id} notice={notice} />
                        ));
                        if (isHiddenPermissionMessage(message)) {
                            return attachedNotices.length ? <Fragment key={message.id}>{attachedNotices}</Fragment> : null;
                        }
                        if (message.role === 'user') {
                            return (
                                <Fragment key={message.id}>
                                    <UserPromptCard message={message} />
                                    {attachedNotices}
                                </Fragment>
                            );
                        }
                        return (
                            <Fragment key={message.id}>
                            <article className={`message ${message.role}`}>
                                <AssistantContent
                                    index={index}
                                    message={message}
                                    messages={messages}
                                    now={now}
                                    pendingPrompt={pendingPrompt}
                                />
                                {message.permission
                                    ? <PermissionCard message={message} onDecision={onPermissionDecision} />
                                    : null}
                                {typeof message.activity === 'object' && message.activity.type === 'model_retry'
                                    ? <div className="model-retry-alert" role="status">
                                        {t('modelRetrying', {
                                            attempt: message.activity.attempt,
                                            maxAttempts: message.activity.maxAttempts,
                                            wait: formatRetryWait(message.activity.retryAfterSeconds, t),
                                        })}
                                    </div>
                                    : null}
                            </article>
                            {attachedNotices}
                            </Fragment>
                        );
                    })}
                </section>
            ))}
            {trailingNotices.map((notice) => <ModelSwitchNotice key={notice.id} notice={notice} />)}
        </Container>
    );
};

const ModelSwitchNoticeShell = styled.div`
    width: 100%;
    min-width: 0;
    display: grid;
    grid-template-columns: minmax(16px, 1fr) auto minmax(16px, 1fr);
    align-items: center;
    gap: 12px;
    padding: 8px 0;
    color: ${(props): string => props.theme.textColorSecondary};
    font-size: 12px;
    line-height: 18px;

    .model-switch-wave {
        height: 6px;
        background: currentColor;
        opacity: 0.4;
        -webkit-mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='6' viewBox='0 0 12 6'%3E%3Cpath fill='none' stroke='black' stroke-width='1' d='M0 3 Q 3 0 6 3 T 12 3'/%3E%3C/svg%3E") center / 12px 6px repeat-x;
        mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='6' viewBox='0 0 12 6'%3E%3Cpath fill='none' stroke='black' stroke-width='1' d='M0 3 Q 3 0 6 3 T 12 3'/%3E%3C/svg%3E") center / 12px 6px repeat-x;
    }
`;

export const ModelSwitchNotice = ({ notice }: { notice: ConversationNotice }): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    return (
        <ModelSwitchNoticeShell className="model-switch-notice" role="status">
            <span aria-hidden="true" className="model-switch-wave" />
            <span>{t('modelSwitched', { model: notice.model })}</span>
            <span aria-hidden="true" className="model-switch-wave" />
        </ModelSwitchNoticeShell>
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
    const stickyRef = useRef<HTMLDivElement>(null);
    const contentRef = useRef<HTMLDivElement>(null);
    const [stuck, setStuck] = useState(false);
    const [expanded, setExpanded] = useState(false);
    const [overflowing, setOverflowing] = useState(false);

    useEffect(() => {
        const sticky = stickyRef.current;
        const turn = sticky?.closest('.message-turn');
        const scroller = sticky?.closest('.messages');
        if (!sticky || !turn || !scroller) return undefined;
        const measure = (): void => {
            const top = scroller.getBoundingClientRect().top + scroller.clientTop;
            setStuck(turn.getBoundingClientRect().top < top - 0.5 && sticky.getBoundingClientRect().bottom > top);
        };
        measure();
        scroller.addEventListener('scroll', measure, { passive: true });
        window.addEventListener('resize', measure);
        const observer = typeof ResizeObserver === 'undefined' ? undefined : new ResizeObserver(measure);
        observer?.observe(turn);
        observer?.observe(scroller);
        return () => {
            scroller.removeEventListener('scroll', measure);
            window.removeEventListener('resize', measure);
            observer?.disconnect();
        };
    }, []);

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
        <div className={`user-prompt-sticky${stuck ? ' stuck' : ''}`} ref={stickyRef}>
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
            <span aria-hidden="true" className="user-prompt-fade" />
        </div>
    );
};

const ContentBlock = ({ allowActions, block, streaming }: { allowActions: boolean; block: MessageContentBlock; streaming: boolean }): JSX.Element | null => {
    if (block.type === 'thinking') return null;
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

const AssistantContent = ({
    index,
    message,
    messages,
    now,
    pendingPrompt,
}: {
    index: number;
    message: ChatMessage;
    messages: ChatMessage[];
    now: number;
    pendingPrompt: boolean;
}): JSX.Element => {
    const { t } = useTranslation('insightWebAgent');
    const streaming = isStreamingAssistantMessage(messages, index, pendingPrompt);
    const lastProcessIndex = message.content.reduce((last, block, blockIndex) => block.type !== 'text' ? blockIndex : last, -1);
    // Text followed by more reasoning or tools is progress narration, not the final answer.
    const timelineEntries = message.content.slice(0, lastProcessIndex + 1);
    const answerBlocks = message.content.slice(lastProcessIndex + 1);

    return <>
        {timelineEntries.length ? <ThinkingTimeline
            analyzing={message.activity === 'analyzing_tool_results'}
            entries={timelineEntries}
            message={message}
            now={now}
            open={streaming}
            hasTrailingText={answerBlocks.some((block) => block.type === 'text' && block.text.trim())}
            t={t}
        /> : <AnswerMeta inProgress={streaming} message={message} now={now} />}
        {answerBlocks.map((block) => <ContentBlock
            allowActions={message.role === 'assistant'}
            block={block}
            key={block.id}
            streaming={streaming}
        />)}
    </>;
};

type TimelineEntry = MessageContentBlock;

const ThinkingTimeline = ({
    analyzing,
    entries,
    message,
    now,
    open: expanded,
    hasTrailingText,
    t,
}: {
    analyzing: boolean;
    entries: TimelineEntry[];
    message: ChatMessage;
    now: number;
    open: boolean;
    hasTrailingText: boolean;
    t: TFunction;
}): JSX.Element => {
    const [open, setOpen] = useState(expanded);
    useEffect(() => setOpen(expanded), [expanded]);
    const thinking = expanded;
    const totalDuration = processingDuration(message, now, expanded);
    const showAnalyzing = expanded && analyzing && !hasTrailingText;
    const activeIndex = showAnalyzing ? -1 : entries.reduce((last, entry, entryIndex) => {
        if (expanded && entry.type === 'tool' && entry.toolCall.status === 'in_progress') return entryIndex;
        if (thinking && entry.type === 'thinking' && entryIndex === entries.length - 1 && !hasTrailingText) return entryIndex;
        return last;
    }, -1);

    return (
        <details className="thinking-timeline answer-meta-details" onToggle={(event) => setOpen(event.currentTarget.open)} open={open}>
            <summary>
                {thinking ? <ThinkingSparkle /> : null}
                <span>{analysisStatusLabel(expanded, message, totalDuration, t)}</span>
                <span aria-hidden="true" className="thinking-chevron" />
            </summary>
            <div className={`timeline-list${expanded ? ' processing' : ''}`}>
                {entries.map((entry, entryIndex) => (
                    <TimelineNode
                        active={entryIndex === activeIndex}
                        duration={timelineEntryDuration(entries, entryIndex, message, now, entryIndex === activeIndex)}
                        entry={entry}
                        key={entry.id}
                        t={t}
                    />
                ))}
                {showAnalyzing
                    ? (
                        <div className="timeline-item analyzing active">
                            <span aria-hidden="true" className="timeline-marker" />
                            <div className="timeline-heading"><span className="timeline-title">{t('analyzingToolResults')}</span></div>
                        </div>
                    )
                    : null}
            </div>
        </details>
    );
};

const TimelineNode = ({
    active,
    duration,
    entry,
    t,
}: {
    active: boolean;
    duration?: string;
    entry: TimelineEntry;
    t: TFunction;
}): JSX.Element => {
    if (entry.type === 'text') return <div className="timeline-item progress completed">
        <span aria-hidden="true" className="timeline-marker" />
        <ContentBlock allowActions block={entry} streaming={false} />
    </div>;
    if (entry.type === 'thinking') {
        return (
            <div className={`timeline-item thinking ${active ? 'active' : 'completed'}`}>
                <span aria-hidden="true" className="timeline-marker" />
                <div className="timeline-heading">
                    <span className="timeline-title">{t(active ? 'thinking' : 'thinkingCompleted')}</span>
                    {duration ? <span className="timeline-duration">{duration}</span> : null}
                </div>
                <div className="timeline-body rich-text thinking-content">
                    <ReactMarkdown components={markdownComponents} remarkPlugins={[remarkGfm]}>{entry.text}</ReactMarkdown>
                </div>
            </div>
        );
    }

    const toolCall = entry.toolCall;
    const target = toolCallTarget(toolCall.input);
    const outputLabel = t(toolCall.status === 'failed' ? 'toolError' : 'toolOutput');
    const hasDetails = Boolean(toolCall.input || toolCall.output);
    return (
        <div className={`timeline-item tool ${toolCall.status === 'failed' ? 'failed' : active ? 'active' : 'completed'}`}>
            <span aria-hidden="true" className="timeline-marker" />
            <div className="timeline-heading">
                <span className="timeline-title">{toolCallSummary(toolCall, t)}</span>
                <span className="tool-state">
                    {toolCallState(toolCall, t)}
                    {duration ? <> · <span className="timeline-duration">{duration}</span></> : null}
                </span>
            </div>
            {hasDetails
                ? (
                    <details className="timeline-tool-details">
                        <summary>
                            <span aria-hidden="true" className="timeline-tool-chevron" />
                            <span className="timeline-tool-target">⎿ {target ?? (toolCall.input ? t('toolInput') : outputLabel)}</span>
                        </summary>
                        <div className="timeline-tool-sections">
                            {toolCall.input ? <ToolSection label={t('toolInput')} value={toolCall.input} /> : null}
                            {toolCall.output ? <ToolSection label={outputLabel} value={toolCall.output} /> : null}
                        </div>
                    </details>
                )
                : target
                    ? <div className="timeline-tool-target">⎿ {target}</div>
                    : null}
        </div>
    );
};

const thinkingBlockDuration = (entries: TimelineEntry[], index: number, message: ChatMessage, now: number, active: boolean): number | undefined => {
    const entry = entries[index];
    if (entry.type !== 'thinking') return undefined;
    if (entry.durationMs !== undefined) return entry.durationMs;
    if (entry.startedAt === undefined) return undefined;
    const nextStartedAt = entries.slice(index + 1).map(getTimelineEntryStartedAt).find((startedAt): startedAt is number => startedAt !== undefined);
    const end = nextStartedAt ?? (active ? now : message.completedAt ??
        (message.startedAt !== undefined && message.durationMs !== undefined ? message.startedAt + message.durationMs : undefined));
    return end !== undefined && end >= entry.startedAt ? end - entry.startedAt : undefined;
};

const timelineEntryDuration = (entries: TimelineEntry[], index: number, message: ChatMessage, now: number, active: boolean): string | undefined => {
    const entry = entries[index];
    if (entry.type === 'tool') return formatToolDuration(entry.toolCall, now);
    const duration = thinkingBlockDuration(message.content, message.content.findIndex((block) => block.id === entry.id), message, now, active);
    return duration === undefined ? undefined : formatDuration(duration);
};

const getTimelineEntryStartedAt = (entry: TimelineEntry): number | undefined => (
    entry.type === 'tool' ? entry.toolCall.startedAt : entry.startedAt
);

const AnswerMeta = ({ inProgress = false, message, now }: { inProgress?: boolean; message: ChatMessage; now: number }): JSX.Element | null => {
    const { t } = useTranslation('insightWebAgent');
    const thinking = inProgress;
    const label = analysisStatusLabel(inProgress, message, processingDuration(message, now, inProgress), t);
    return message.startedAt === undefined ? null : (
        <div className="thinking-summary">
            {thinking ? <ThinkingSparkle /> : null}
            <span>{label}</span>
        </div>
    );
};

const ThinkingSparkle = (): JSX.Element => <span aria-hidden="true" className="thinking-sparkle" />;

const analysisStatusLabel = (inProgress: boolean, message: ChatMessage, duration: string | undefined, t: TFunction): string => {
    if (inProgress) return t('analysisInProgress', { duration: duration ?? formatDuration(0) });
    const elapsed = duration ? t('thinkingDuration', { duration }) : undefined;
    if (message.completionStatus === 'failed' || message.completionStatus === 'cancelled') {
        const status = t(message.completionStatus === 'failed' ? 'analysisFailed' : 'analysisCancelled');
        return elapsed ? `${status} · ${elapsed}` : status;
    }
    return elapsed ?? t('analysisCompleted');
};

const processingDuration = (message: ChatMessage, now: number, inProgress: boolean): string | undefined => {
    const end = message.completedAt ?? (inProgress ? now : undefined);
    if (message.startedAt !== undefined && end !== undefined) {
        return formatDuration(end - message.startedAt);
    }
    // Older messages may only have the recorded first-output duration.
    return message.durationMs === undefined ? undefined : formatDuration(message.durationMs);
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
            {toolCalls.map((toolCall) => {
                const duration = formatToolDuration(toolCall, now);
                return <details className={`tool-call ${toolCall.status}`} key={toolCall.toolCallId}>
                    <summary>
                        <span className="tool-status" />
                        <span className="tool-summary">
                            <span className="tool-name">{toolCallSummary(toolCall, t)}</span>
                            {toolCallTarget(toolCall.input) ? <span className="tool-target">⎿ {toolCallTarget(toolCall.input)}</span> : null}
                        </span>
                        <span className="tool-state">
                            {toolCallState(toolCall, t)}
                            {duration ? ` · ${duration}` : null}
                        </span>
                    </summary>
                    {toolCall.input || toolCall.output ? (
                        <div className="tool-details">
                            {toolCall.input ? <ToolSection label={t('toolInput')} value={toolCall.input} /> : null}
                            {toolCall.output ? <ToolSection label={toolCall.status === 'failed' ? t('toolError') : t('toolOutput')} value={toolCall.output} /> : null}
                        </div>
                    ) : null}
                </details>;
            })}
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
    return enabled ? Date.now() : now;
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

const formatToolDuration = (toolCall: ToolCallItem, now: number): string | undefined => {
    const duration = toolCall.status === 'in_progress'
        ? now - (toolCall.startedAt ?? now)
        : toolCall.durationMs;
    return duration === undefined ? undefined : formatDuration(duration);
};

const formatDuration = (value: number): string => {
    const durationMs = Math.max(0, value);
    if (durationMs < 1000) return `${durationMs}ms`;
    if (durationMs < 10000) return `${(durationMs / 1000).toFixed(1)}s`;
    const totalSeconds = Math.round(durationMs / 1000);
    if (totalSeconds < 60) return `${totalSeconds}s`;
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${minutes}m${String(seconds).padStart(2, '0')}s`;
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
            <div className="permission-title">{permission.title ?? permissionTitle(permission.kind, t)}</div>
            <div className="permission-path" title={permission.target}>{truncatePermissionTarget(permission.target)}</div>
            {permission.kind === 'bash' && permission.details?.cwd ? <div className="permission-state">{t('workingDirectory')}: {String(permission.details.cwd)}</div> : null}
            {permission.kind === 'tool' && permission.details?.input ? <ToolSection label={t('toolInput')} value={formatPermissionInput(permission.details.input)} /> : null}
            {pending ? (
                <div className="permission-actions">
                    {permission.actions.includes('allow_once') ? (
                        <button
                            className="primary"
                            disabled={Boolean(permission.loadingDecision)}
                            onClick={() => { onDecision(permission.sessionId, permission.requestId, 'allow_once'); }}
                            type="button"
                        >
                            {permission.loadingDecision === 'allow_once' ? t('allowing') : t('allowOnce')}
                        </button>
                    ) : null}
                    {permission.actions.includes('allow_always') ? (
                        <button
                            className="primary"
                            disabled={Boolean(permission.loadingDecision)}
                            onClick={() => { onDecision(permission.sessionId, permission.requestId, 'allow_always'); }}
                            type="button"
                        >
                            {permission.loadingDecision === 'allow_always' ? t('allowing') : t('allowAlways')}
                        </button>
                    ) : null}
                    {permission.actions.includes('deny') ? (
                        <button
                            disabled={Boolean(permission.loadingDecision)}
                            onClick={() => { onDecision(permission.sessionId, permission.requestId, 'deny'); }}
                            type="button"
                        >
                            {permission.loadingDecision === 'deny' ? t('denying') : t('deny')}
                        </button>
                    ) : null}
                </div>
            ) : <div className="permission-state">{permissionStateText(permission.state, t)}</div>}
            {permission.error ? <div className="permission-error">{permission.error}</div> : null}
        </div>
    );
};

const permissionTitle = (kind: NonNullable<ChatMessage['permission']>['kind'], t: TFunction): string => {
    if (kind === 'bash') return t('allowBashCommand');
    if (kind === 'tool') return t('allowToolUse');
    return t('allowFileRead');
};

const formatPermissionInput = (input: unknown): string => {
    try {
        const value = JSON.stringify(input, null, 2);
        return value.length <= 4000 ? value : `${value.slice(0, 3999)}…`;
    } catch (_error) {
        return String(input);
    }
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

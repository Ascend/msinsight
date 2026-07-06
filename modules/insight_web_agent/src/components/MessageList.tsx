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
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import type React from 'react';
import type { ChatMessage, PermissionDecision } from '../types';

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
        border: 1px solid ${(props): string => props.theme.borderColorLight};
        background: ${(props): string => props.theme.bgColorLight};
    }

    .message.assistant {
        border: 1px solid ${(props): string => props.theme.borderColor};
        background: ${(props): string => props.theme.bgColor};
    }

    .thinking {
        margin-bottom: 10px;
        padding-left: 10px;
        border-left: 2px solid ${(props): string => props.theme.borderColor};
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
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

    .rich-text {
        min-width: 0;
        max-width: 100%;
        display: grid;
        gap: 8px;
        overflow-wrap: anywhere;
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
        border: 1px solid ${(props): string => props.theme.borderColorLight};
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        padding: 1px 5px;
        background: ${(props): string => props.theme.bgColorDark};
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
    if (!messages.length) {
        return <Container><div className="empty">No local messages loaded for this session. Send a message to continue.</div></Container>;
    }

    return (
        <Container>
            {messages.map((message, index) => (
                <article className={`message ${message.role}`} key={message.id}>
                    {message.thinking ? <div className="thinking">{message.thinking}</div> : null}
                    {message.permission ? <PermissionCard message={message} onDecision={onPermissionDecision} /> : null}
                    {message.text || !message.permission ? (
                        <div className={`rich-text ${message.text ? '' : 'muted'}`}>
                            {message.text ? <ReactMarkdown components={markdownComponents} remarkPlugins={[remarkGfm]}>{message.text}</ReactMarkdown> : '...'}
                        </div>
                    ) : null}
                    {message.images?.length ? (
                        <div className="attachments">
                            {message.images.map((image) => (
                                <div className="attachment" key={image.id}>
                                    <img alt={image.name} src={`data:${image.mimeType};base64,${image.data}`} />
                                </div>
                            ))}
                        </div>
                    ) : null}
                    {isThinkingMessage(messages, index, pendingPrompt) ? <div className="thinking-indicator">Thinking...</div> : null}
                </article>
            ))}
        </Container>
    );
};

const PermissionCard = ({
    message,
    onDecision,
}: {
    message: ChatMessage;
    onDecision: (sessionId: string, requestId: string, decision: PermissionDecision) => Promise<void>;
}): JSX.Element | null => {
    const permission = message.permission;
    if (!permission) return null;
    const pending = permission.state === 'pending';
    return (
        <div className="permission-card">
            <div className="permission-title">Allow file read?</div>
            <div className="permission-path">{permission.path}</div>
            {pending ? (
                <div className="permission-actions">
                    <button
                        className="primary"
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { void onDecision(permission.sessionId, permission.requestId, 'allow_once'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'allow_once' ? 'Allowing...' : 'Allow once'}
                    </button>
                    <button
                        className="primary"
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { void onDecision(permission.sessionId, permission.requestId, 'allow_always'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'allow_always' ? 'Allowing...' : 'Allow always'}
                    </button>
                    <button
                        disabled={Boolean(permission.loadingDecision)}
                        onClick={() => { void onDecision(permission.sessionId, permission.requestId, 'deny'); }}
                        type="button"
                    >
                        {permission.loadingDecision === 'deny' ? 'Denying...' : 'Deny'}
                    </button>
                </div>
            ) : <div className="permission-state">{permissionStateText(permission.state)}</div>}
            {permission.error ? <div className="permission-error">{permission.error}</div> : null}
        </div>
    );
};

const permissionStateText = (state: NonNullable<ChatMessage['permission']>['state']): string => {
    if (state === 'allowed_once') return 'Allowed once';
    if (state === 'allowed_always') return 'Allowed for this app run';
    if (state === 'denied') return 'Denied';
    if (state === 'expired') return 'Expired';
    if (state === 'invalidated') return 'No longer actionable';
    return 'Pending';
};

const isThinkingMessage = (messages: ChatMessage[], index: number, pendingPrompt: boolean): boolean => {
    if (!pendingPrompt) return false;
    const message = messages[index];
    return message.role === 'assistant' && index === messages.length - 1 && !message.permission;
};

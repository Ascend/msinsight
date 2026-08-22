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
import { useChatState } from '../hooks/useChatState';
import { Composer } from './Composer';
import { MessageList } from './MessageList';
import { WelcomePanel } from './WelcomePanel';

const Container = styled.section`
    flex: 1 1 auto;
    min-height: 0;
    width: 100%;
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    grid-template-rows: auto minmax(0, 1fr) auto;
    background: ${(props): string => props.theme.bgColor};
    overflow: hidden;

    .session-title-slot {
        grid-row: 1;
        min-width: 0;
        overflow: hidden;
        padding: 8px 16px 12px;
    }

    .session-title-slot:empty {
        display: none;
    }

    .session-title {
        width: 100%;
        max-width: 100%;
        min-width: 0;
        display: block;
        overflow: hidden;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 14px;
        font-weight: 600;
        line-height: 22px;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .messages {
        height: 100%;
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 14px;
        overflow: auto;
        scrollbar-gutter: stable;
        padding: 0 16px;
    }

    .conversation-content {
        grid-row: 2;
        min-height: 0;
        overflow: hidden;
    }

    .composer-slot {
        grid-row: 3;
        min-width: 0;
    }

`;

export const ChatPanel = (): JSX.Element => {
    const { currentSessionId, isDraftSession, messages, messagesRef, pendingPrompt, respondToPermission, sessions } = useChatState();
    const currentTitle = isDraftSession
        ? undefined
        : sessions.find((session) => session.sessionId === currentSessionId)?.title?.trim();

    return (
        <Container>
            <div className="session-title-slot">
                {currentTitle ? <div className="session-title" title={currentTitle}>{currentTitle}</div> : null}
            </div>
            <div className="conversation-content">
                {messages.length
                    ? <section className="messages" ref={messagesRef}>
                        <MessageList messages={messages} pendingPrompt={pendingPrompt} onPermissionDecision={respondToPermission} />
                    </section>
                    : <WelcomePanel />}
            </div>
            <div className="composer-slot"><Composer /></div>
        </Container>
    );
};

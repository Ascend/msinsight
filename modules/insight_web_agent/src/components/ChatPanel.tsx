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
    grid-template-rows: 1fr auto;
    background: ${(props): string => props.theme.bgColor};
    overflow: hidden;

    .messages {
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 14px;
        overflow: auto;
        scrollbar-gutter: stable;
        padding: 0 16px;
    }

`;

export const ChatPanel = (): JSX.Element => {
    const { messages, messagesRef, pendingPrompt, respondToPermission } = useChatState();

    return (
        <Container>
            {messages.length
                ? <section className="messages" ref={messagesRef}>
                    <MessageList messages={messages} pendingPrompt={pendingPrompt} onPermissionDecision={respondToPermission} />
                </section>
                : <WelcomePanel />}

            <Composer />
        </Container>
    );
};

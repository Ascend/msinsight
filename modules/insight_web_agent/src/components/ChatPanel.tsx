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
import { SetIcon } from '@insight/lib/icon/Icon';
import { useTranslation } from 'react-i18next';
import { useChatState } from '../hooks/useChatState';
import { AgentSettingsDialog } from './AgentSettingsDialog';
import { Composer } from './Composer';
import { MessageList } from './MessageList';

const Container = styled.section`
    flex: 1 1 auto;
    min-height: 0;
    width: 100%;
    display: grid;
    grid-template-rows: auto 1fr auto;
    background: ${(props): string => props.theme.bgColorLight};
    overflow: hidden;

    .toolbar {
        display: flex;
        justify-content: flex-end;
        padding: 8px 14px 0;
    }

    .settings-button {
        width: 26px;
        height: 26px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .settings-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }

    .messages {
        min-height: 0;
        display: flex;
        flex-direction: column;
        gap: 14px;
        overflow: auto;
        scrollbar-gutter: stable;
        padding: 12px 14px;
    }

`;

export const ChatPanel = (): JSX.Element => {
    const { messages, messagesRef, pendingPrompt, respondToPermission } = useChatState();
    const { t } = useTranslation('insightWebAgent');

    return (
        <Container>
            <div className="toolbar">
                <AgentSettingsDialog
                    trigger={(
                        <button aria-label={t('agentSettings')} className="settings-button" type="button">
                            <SetIcon />
                        </button>
                    )}
                />
            </div>
            <section className="messages" ref={messagesRef}>
                <MessageList messages={messages} pendingPrompt={pendingPrompt} onPermissionDecision={respondToPermission} />
            </section>

            <Composer />
        </Container>
    );
};

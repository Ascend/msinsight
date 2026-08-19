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
import { useTranslation } from 'react-i18next';
import { useChatState } from '../hooks/useChatState';
import { ChatPanel } from './ChatPanel';
import { SessionSidebar } from './SessionSidebar';

const Container = styled.main`
    width: 100%;
    height: 100vh;
    min-height: 0;
    display: flex;
    flex-direction: column;
    background: ${(props): string => props.theme.bgColor};
    color: ${(props): string => props.theme.textColorPrimary};
    position: relative;

    .agent-switch-mask,
    .agent-discovery-mask {
        position: absolute;
        inset: 0;
        z-index: 10;
        display: flex;
        align-items: center;
        justify-content: center;
        background: ${(props): string => props.theme.bgColorLight}cc;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 13px;
        font-weight: 700;
    }

    .agent-discovery-mask {
        z-index: 20;
        flex-direction: column;
        gap: 12px;
        background: ${(props): string => props.theme.bgColorDark};
    }

    .agent-discovery-spinner {
        width: 28px;
        height: 28px;
        border: 3px solid ${(props): string => props.theme.borderColor};
        border-top-color: ${(props): string => props.theme.primaryColor};
        border-radius: 50%;
        animation: agent-discovery-spin 0.8s linear infinite;
    }

    @keyframes agent-discovery-spin {
        to { transform: rotate(360deg); }
    }
`;

export const ChatPage = (): JSX.Element => {
    const { agentDiscoveryLoading, switchingAgent } = useChatState();
    const { t } = useTranslation('insightWebAgent');

    return (
        <Container>
            <SessionSidebar />
            <ChatPanel />
            {switchingAgent && <div className="agent-switch-mask">{t('switchingAgent')}</div>}
            {agentDiscoveryLoading && (
                <div className="agent-discovery-mask" role="status">
                    <div className="agent-discovery-spinner" />
                    <span>{t('loading')}</span>
                </div>
            )}
        </Container>
    );
};

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
import { ChatPanel } from './ChatPanel';
import { SessionSidebar } from './SessionSidebar';

const Container = styled.main`
    width: 100%;
    height: 100vh;
    min-height: 0;
    display: flex;
    flex-direction: column;
    background: ${(props): string => props.theme.bgColorDark};
    color: ${(props): string => props.theme.textColorPrimary};
    position: relative;

    .agent-switch-mask {
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
`;

export const ChatPage = (): JSX.Element => {
    const { switchingAgent } = useChatState();

    return (
        <Container>
            <SessionSidebar />
            <ChatPanel />
            {switchingAgent && <div className="agent-switch-mask">Switching agent...</div>}
        </Container>
    );
};

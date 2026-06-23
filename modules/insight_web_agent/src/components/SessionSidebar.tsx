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
import { Drawer } from 'antd';
import { useState } from 'react';
import { Button, Select } from '@insight/lib/components';
import { AddIcon, CaretRightIcon, DeleteIcon } from '@insight/lib/icon/Icon';
import { useChatState } from '../hooks/useChatState';

const Container = styled.div`
    position: relative;
    flex: 0 0 auto;
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 8px 10px 10px;
    border-bottom: 1px solid ${(props): string => props.theme.borderColor};
    background: ${(props): string => props.theme.bgColorLight};

    .session-title-bar {
        flex: 1 1 auto;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        color: ${(props): string => props.theme.textColorPrimary};
        font-size: 12px;
        font-weight: 700;
    }

    .agent-picker {
        flex: 0 0 130px;
        min-width: 0;
    }

    .agent-error {
        color: ${(props): string => props.theme.dangerColor};
        font-size: 12px;
        line-height: 1;
    }

    .icon-button {
        width: 26px;
        height: 26px;
        flex: 0 0 26px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .icon-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }

    .icon-button:disabled {
        color: ${(props): string => props.theme.textColorDisabled};
        cursor: not-allowed;
    }

    .session-drawer .ant-drawer-content {
        background: ${(props): string => props.theme.bgColorLight};
    }

    .session-drawer.ant-drawer-right {
        position: fixed;
        inset: 0;
    }

    .session-drawer .ant-drawer-header {
        padding: 14px 16px;
        border-bottom: 1px solid ${(props): string => props.theme.borderColor};
        background: ${(props): string => props.theme.bgColorLight};
    }

    .session-drawer .ant-drawer-title,
    .session-drawer .ant-drawer-close {
        color: ${(props): string => props.theme.textColorPrimary};
    }

    .session-drawer .ant-drawer-body {
        padding: 12px;
        background: ${(props): string => props.theme.bgColorLight};
    }

    .session-list {
        display: grid;
        gap: 6px;
    }

    .drawer-title {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
    }

    .session-item {
        width: 100%;
        min-width: 0;
        display: grid;
        grid-template-columns: minmax(0, 1fr) auto;
        align-items: center;
        gap: 8px;
        padding: 10px;
        border: 1px solid transparent;
        border-radius: ${(props): string => props.theme.borderRadiusBase};
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        text-align: left;
        cursor: pointer;
    }

    .session-content {
        min-width: 0;
        display: grid;
        gap: 4px;
    }

    .session-delete {
        width: 24px;
        height: 24px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-radius: ${(props): string => props.theme.borderRadiusSmall};
        background: transparent;
        color: ${(props): string => props.theme.textColorSecondary};
        cursor: pointer;
    }

    .session-delete:hover {
        background: ${(props): string => props.theme.bgColorDark};
        color: ${(props): string => props.theme.dangerColor};
    }

    .session-item:hover,
    .session-item.active {
        border-color: ${(props): string => props.theme.primaryColor};
        background: ${(props): string => props.theme.primaryColorLight2};
    }

    .session-item:disabled {
        color: ${(props): string => props.theme.textColorDisabled};
        cursor: not-allowed;
    }

    .session-title,
    .session-meta {
        display: block;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
    }

    .session-meta {
        color: ${(props): string => props.theme.textColorSecondary};
        font-size: 12px;
        font-weight: 500;
    }
`;

export const SessionSidebar = (): JSX.Element => {
    const {
        createSession,
        activeAgentName,
        agentError,
        agentServers,
        currentSessionId,
        deleteSession,
        sessions,
        selectSession,
        setAgent,
    } = useChatState();
    const [open, setOpen] = useState(false);
    const activeSession = sessions.find((session) => session.sessionId === currentSessionId);
    const handleCreateSession = (): void => {
        void createSession();
        setOpen(false);
    };

    return (
        <Container>
            <Select
                className="agent-picker"
                onChange={(value) => void setAgent(String(value))}
                options={agentServers.map((agent) => ({ value: agent.name, label: agent.name }))}
                value={activeAgentName}
                width="130px"
            />
            {agentError && <span className="agent-error" title={agentError}>Agent error</span>}
            <span className="session-title-bar" title={activeSession?.title || activeSession?.sessionId || 'New session'}>
                {activeSession?.title || activeSession?.sessionId || 'New session'}
            </span>
            <button className="icon-button" onClick={() => void createSession()} title="New chat" type="button">
                <AddIcon />
            </button>
            <button className="icon-button drawer-toggle" disabled={!sessions.length} onClick={() => setOpen(true)} title="Open sessions" type="button">
                <CaretRightIcon style={{ transform: 'rotate(180deg)' }} />
            </button>
            <Drawer
                className="session-drawer"
                getContainer={false}
                mask
                maskClosable
                onClose={() => setOpen(false)}
                open={open}
                placement="right"
                title={<div className="drawer-title"><span>Sessions</span><Button onClick={handleCreateSession} size="small" type="primary">New Chat</Button></div>}
                width={280}
            >
                <div className="session-list">
                    {sessions.map((session) => (
                        <button
                            className={`session-item ${session.sessionId === currentSessionId ? 'active' : ''}`}
                            disabled={session.isPending}
                            key={session.sessionId}
                            onClick={() => {
                                void selectSession(session);
                                setOpen(false);
                            }}
                            type="button"
                        >
                            <span className="session-content">
                                <span className="session-title">{session.title || session.sessionId}</span>
                                <span className="session-meta">{getSessionMeta(session)}</span>
                            </span>
                            <span
                                className="session-delete"
                                onClick={(event) => {
                                    event.stopPropagation();
                                    void deleteSession(session);
                                }}
                                role="button"
                                tabIndex={0}
                            >
                                <DeleteIcon />
                            </span>
                        </button>
                    ))}
                </div>
            </Drawer>
        </Container>
    );
};

const getSessionMeta = (session: { pendingPrompt?: boolean; status?: string; updatedAt?: string; sessionId: string }): string => {
    if (session.pendingPrompt || session.status === 'working') return 'Working...';
    if (session.status === 'completed') return 'Completed';
    if (session.status === 'loading') return 'Loading...';
    if (session.status === 'error') return 'Load failed';
    return session.updatedAt || session.sessionId;
};

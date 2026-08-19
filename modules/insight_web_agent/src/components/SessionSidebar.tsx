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
import type { TFunction } from 'i18next';
import { useState } from 'react';
import { Button } from '@insight/lib/components';
import { DeleteIcon } from '@insight/lib/icon/Icon';
import { useTranslation } from 'react-i18next';
import { requestHostClose } from '../connection';
import { useChatState } from '../hooks/useChatState';
import closeIcon from '../icons/close.svg';
import historyIcon from '../icons/history-session.svg';
import logo from '../icons/logo.png';
import newSessionIcon from '../icons/new-session.svg';
import settingsIcon from '../icons/settings.svg';
import { AgentSelect } from './AgentSelect';
import { AgentSettingsDialog } from './AgentSettingsDialog';

const Container = styled.div`
    position: relative;
    flex: 0 0 auto;
    display: flex;
    align-items: center;
    min-height: 54px;
    gap: 0;
    padding: 10px 16px;
    background: ${(props): string => props.theme.bgColor};

    .agent-brand {
        flex: 1 1 auto;
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 0;
    }

    .toolbar-actions {
        display: flex;
        align-items: center;
        gap: 2px;
    }

    .agent-picker {
        width: fit-content;
        max-width: calc(100vw - 260px);
        flex: 0 1 auto;
        min-width: 0;
    }

    .agent-error {
        color: ${(props): string => props.theme.dangerColor};
        font-size: 12px;
        line-height: 1;
    }

    .icon-button {
        width: 28px;
        height: 28px;
        flex: 0 0 28px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        padding: 6px;
        border-radius: 6px;
        background: transparent;
        color: ${(props): string => props.theme.textColorPrimary};
        cursor: pointer;
    }

    .icon-button:hover {
        background: ${(props): string => props.theme.bgColorDark};
    }

    .icon-button img {
        width: 16px;
        height: 16px;
        filter: ${(props): string => props.theme.mode === 'dark' ? 'invert(1)' : 'none'};
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

const AgentAvatar = styled.span`
    width: 100%;
    height: 100%;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border-radius: 50%;
    background: ${(props): string => props.theme.primaryColorLight5};
    color: ${(props): string => props.theme.primaryColor};
    font-size: 10px;
    font-weight: 600;
    text-transform: uppercase;

    &.native {
        border-radius: 0;
        background: transparent;
    }

    img {
        width: 100%;
        height: 100%;
        object-fit: contain;
    }
`;

const AddAgentButton = styled.button`
    width: 100%;
    height: 32px;
    border: 1px solid ${(props): string => props.theme.borderColorLighter};
    border-radius: 8px;
    background: transparent;
    color: ${(props): string => props.theme.textColorPrimary};
    font-size: 14px;
    cursor: pointer;

    &:hover {
        border-color: ${(props): string => props.theme.primaryColor};
        color: ${(props): string => props.theme.primaryColor};
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
    const { t } = useTranslation('insightWebAgent');
    const handleCreateSession = (): void => {
        createSession();
        setOpen(false);
    };

    return (
        <Container>
            <div className="agent-brand">
                <AgentSelect
                    className="agent-picker"
                    footer={(
                        <AgentSettingsDialog trigger={(
                            <AddAgentButton type="button">{t('addAgent')}</AddAgentButton>
                        )} />
                    )}
                    onChange={(value) => { void setAgent(value); }}
                    options={agentServers.map((agent) => ({
                        value: agent.name,
                        label: agent.name,
                        icon: getAgentIcon(agent.name),
                    }))}
                    placeholder={t('defaultAgent')}
                    title={t('switchAgent')}
                    value={activeAgentName}
                />
            </div>
            {agentError && <span className="agent-error" title={agentError}>{t('agentError')}</span>}
            <div className="toolbar-actions">
                <button className="icon-button" onClick={() => createSession()} title={t('newChat')} type="button">
                    <img src={newSessionIcon} alt="" />
                </button>
                <button className="icon-button drawer-toggle" disabled={!sessions.length} onClick={() => setOpen(true)} title={t('openSessions')} type="button">
                    <img src={historyIcon} alt="" />
                </button>
                <AgentSettingsDialog trigger={(
                    <button aria-label={t('agentSettings')} className="icon-button" title={t('agentSettings')} type="button">
                        <img src={settingsIcon} alt="" />
                    </button>
                )} />
                <button className="icon-button" onClick={requestHostClose} title={t('close')} type="button">
                    <img src={closeIcon} alt="" />
                </button>
            </div>
            <Drawer
                className="session-drawer"
                getContainer={false}
                mask
                maskClosable
                onClose={() => setOpen(false)}
                open={open}
                placement="right"
                title={<div className="drawer-title"><span>{t('sessions')}</span><Button onClick={handleCreateSession} size="small" type="primary">{t('newChat')}</Button></div>}
                width={280}
            >
                <div className="session-list">
                    {sessions.map((session) => (
                        <button
                            className={`session-item ${session.sessionId === currentSessionId ? 'active' : ''}`}
                            disabled={session.isPending}
                            key={session.sessionId}
                            onClick={() => {
                                selectSession(session);
                                setOpen(false);
                            }}
                            type="button"
                        >
                            <span className="session-content">
                                <span className="session-title">{session.title || session.sessionId}</span>
                                <span className="session-meta">{getSessionMeta(session, t)}</span>
                            </span>
                            <span
                                className="session-delete"
                                onClick={(event) => {
                                    event.stopPropagation();
                                    deleteSession(session);
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

const getAgentIcon = (agentName: string): JSX.Element => {
    const normalizedName = agentName.toLowerCase();
    if (normalizedName === 'msinsight-native' || normalizedName.includes('insight')) {
        return <AgentAvatar className="native"><img alt="" src={logo} /></AgentAvatar>;
    }
    return <AgentAvatar>{agentName.slice(0, 1)}</AgentAvatar>;
};

const getSessionMeta = (session: { pendingPrompt?: boolean; status?: string; updatedAt?: string; sessionId: string }, t: TFunction): string => {
    if (session.pendingPrompt || session.status === 'working') return t('working');
    if (session.status === 'completed') return t('completed');
    if (session.status === 'loading') return t('loading');
    if (session.status === 'error') return t('loadFailed');
    return session.updatedAt || session.sessionId;
};

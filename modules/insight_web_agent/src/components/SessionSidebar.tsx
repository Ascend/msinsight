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
import React, { useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { requestHostClose } from '../connection';
import { useChatState } from '../hooks/useChatState';
import closeIcon from '../icons/close.svg';
import historyIcon from '../icons/history-session.svg';
import logo from '../icons/logo.png';
import newSessionIcon from '../icons/new-session.svg';
import settingsIcon from '../icons/settings.svg';
import statusDotIcon from '../icons/status-dot.svg';
import { AgentSelect } from './AgentSelect';
import { AgentSettingsDialog } from './AgentSettingsDialog';
import { SessionHistoryPopover } from './SessionHistoryPopover';

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
        position: relative;
        width: 28px;
        height: 28px;
        flex: 0 0 28px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        border: 0;
        padding: 6px;
        border-radius: ${(props): string => props.theme.borderRadiusLarge};
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

    .icon-button .history-attention {
        width: 8px;
        height: 8px;
        position: absolute;
        top: 4px;
        right: 4px;
        filter: none;
        pointer-events: none;
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
    border-radius: ${(props): string => props.theme.borderRadiusLarge};
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
    const [settingsOpen, setSettingsOpen] = useState(false);
    const [createAgentOnOpen, setCreateAgentOnOpen] = useState(false);
    const [historyAttention, setHistoryAttention] = useState(false);
    const historyButtonRef = useRef<HTMLButtonElement>(null);
    const { t } = useTranslation('insightWebAgent');
    const openSettings = (createAgent: boolean): void => {
        setCreateAgentOnOpen(createAgent);
        setSettingsOpen(true);
    };

    return (
        <Container>
            <div className="agent-brand">
                <AgentSelect
                    className="agent-picker"
                    footer={(
                        <AddAgentButton onClick={() => openSettings(true)} type="button">{t('addAgent')}</AddAgentButton>
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
                <button
                    aria-expanded={open}
                    aria-haspopup="dialog"
                    className="icon-button drawer-toggle"
                    disabled={!sessions.length}
                    onClick={() => setOpen((current) => !current)}
                    ref={historyButtonRef}
                    title={t('openSessions')}
                    type="button"
                >
                    <img src={historyIcon} alt="" />
                    {historyAttention ? <img alt="" aria-hidden="true" className="history-attention" src={statusDotIcon} /> : null}
                </button>
                <button aria-label={t('agentSettings')} className="icon-button" onClick={() => openSettings(false)} title={t('agentSettings')} type="button">
                    <img src={settingsIcon} alt="" />
                </button>
                <button className="icon-button" onClick={requestHostClose} title={t('close')} type="button">
                    <img src={closeIcon} alt="" />
                </button>
            </div>
            <AgentSettingsDialog
                createOnOpen={createAgentOnOpen}
                onOpenChange={setSettingsOpen}
                open={settingsOpen}
            />
            <SessionHistoryPopover
                anchorRef={historyButtonRef}
                currentSessionId={currentSessionId}
                onClose={() => setOpen(false)}
                onDelete={deleteSession}
                onAttentionChange={setHistoryAttention}
                onSelect={selectSession}
                open={open}
                sessions={sessions}
            />
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

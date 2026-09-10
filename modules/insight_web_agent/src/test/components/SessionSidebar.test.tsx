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
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { SessionSidebar } from '../../components/SessionSidebar';

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

jest.mock('../../connection', () => ({
    requestHostClose: jest.fn(),
}));

jest.mock('../../components/AgentSettingsDialog', () => ({
    AgentSettingsDialog: () => null,
}));

jest.mock('../../components/SessionHistoryPopover', () => ({
    SessionHistoryPopover: () => null,
}));

const mockUseChatState = useChatState as jest.Mock;

const renderSidebar = (overrides: Record<string, unknown> = {}) => {
    const state = {
        createSession: jest.fn(),
        activeAgentName: 'DeepSeek',
        agentError: undefined,
        agentServers: [{ name: 'DeepSeek' }, { name: 'Claude' }],
        availableCapabilities: [],
        currentSessionId: 'session-1',
        deleteSession: jest.fn(),
        sessions: [{ sessionId: 'session-1', title: 'Chat' }],
        selectSession: jest.fn(),
        setAgent: jest.fn(),
        refreshAgents: jest.fn(),
        agentDiscoveryLoading: false,
        ...overrides,
    };
    mockUseChatState.mockReturnValue(state);
    render(<SessionSidebar />);
    return state;
};

test('refreshes agents from the top-left agent picker footer', async () => {
    const state = renderSidebar();

    fireEvent.click(screen.getByRole('button', { name: /DeepSeek/i }));
    await waitFor(() => expect(screen.getByRole('button', { name: 'Refresh agents' })).toBeVisible());

    const refreshButton = screen.getByRole('button', { name: 'Refresh agents' });
    expect(getComputedStyle(refreshButton.querySelector('.refresh-icon') as HTMLElement).backgroundColor).toBe('currentColor');

    fireEvent.click(refreshButton);

    expect(state.refreshAgents).toHaveBeenCalledTimes(1);
});

test('disables agent refresh while discovery is running', async () => {
    renderSidebar({ agentDiscoveryLoading: true });

    fireEvent.click(screen.getByRole('button', { name: /DeepSeek/i }));
    await waitFor(() => expect(screen.getByRole('button', { name: 'Refresh agents' })).toBeDisabled());
});

test('renders theme-following logos for catalog agents', async () => {
    renderSidebar({
        activeAgentName: 'OpenCode(auto)',
        agentServers: [
            { name: 'OpenCode(auto)' },
            { name: 'Claude Code(auto)' },
            { name: 'Codex(auto)' },
            { name: 'Trae(auto)' },
        ],
    });

    const trigger = screen.getByRole('button', { name: /OpenCode\(auto\)/i });
    expect(trigger.querySelector('[data-agent-icon="themed"]')).not.toBeNull();

    fireEvent.click(trigger);
    expect((await screen.findByRole('option', { name: 'OpenCode(auto)' })).querySelector('[data-agent-icon="themed"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Codex(auto)' })).querySelector('[data-agent-icon="themed"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Claude Code(auto)' })).querySelector('[data-agent-icon="static"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Trae(auto)' })).querySelector('[data-agent-icon="static"]')).not.toBeNull();
});

test('shows undetected catalog agents as unavailable options', async () => {
    renderSidebar({
        agentServers: [
            { name: 'DeepSeek' },
            { name: 'OpenCode(auto)', available: false },
        ],
    });

    fireEvent.click(screen.getByRole('button', { name: /DeepSeek/i }));
    const unavailable = await screen.findByRole('option', { name: /OpenCode\(auto\) \(Unavailable\)/i });
    expect(unavailable).toBeDisabled();
});

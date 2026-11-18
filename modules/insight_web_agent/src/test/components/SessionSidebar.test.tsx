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
import { SessionHistoryPopover } from '../../components/SessionHistoryPopover';

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
    SessionHistoryPopover: jest.fn(() => null),
}));

const mockUseChatState = useChatState as jest.Mock;

const renderSidebar = (overrides: Record<string, unknown> = {}) => {
    const state = {
        createSession: jest.fn(),
        activeAgentName: 'DeepSeek',
        agentError: undefined,
        agentServers: [{ name: 'DeepSeek', available: true }, { name: 'Claude', available: true }],
        availableCapabilities: [],
        currentSessionId: 'session-1',
        deleteSession: jest.fn(),
        exportSession: jest.fn(),
        renameSession: jest.fn(),
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

test('routes current-session export through one accessible toolbar submenu', async () => {
    const state = renderSidebar();
    const trigger = screen.getByRole('button', { name: 'Export sessions' });
    expect(screen.getAllByRole('button', { name: /Export sessions/ })).toHaveLength(1);
    expect(screen.queryByRole('button', { name: 'Export current conversation' })).not.toBeInTheDocument();
    expect(trigger).toHaveAttribute('aria-haspopup', 'menu');
    expect(trigger).toHaveAttribute('aria-expanded', 'false');
    fireEvent.click(trigger);
    expect(await screen.findByRole('menuitem', { name: 'Historical sessions' })).toBeInTheDocument();
    expect(trigger).toHaveAttribute('aria-expanded', 'true');
    fireEvent.click(screen.getByRole('menuitem', { name: 'Current session' }));
    expect(state.exportSession).toHaveBeenCalledTimes(1);
    expect(state.exportSession).toHaveBeenCalledWith(state.sessions[0]);
    expect(trigger).toHaveAttribute('aria-expanded', 'false');
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
});

test('opens historical export with zero active sessions without changing chat state', async () => {
    const loadSession = jest.fn();
    const state = renderSidebar({ currentSessionId: undefined, sessions: [], loadSession });
    const entry = screen.getByRole('button', { name: 'Export sessions' });
    expect(entry).toBeEnabled();
    fireEvent.click(entry);
    const current = await screen.findByRole('menuitem', { name: 'Current session' });
    expect(current).toHaveAttribute('aria-disabled', 'true');
    fireEvent.click(current);
    expect(state.exportSession).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole('menuitem', { name: 'Historical sessions' }));
    expect(await screen.findByRole('dialog', { name: 'Export historical sessions' })).toBeVisible();
    expect(screen.getByRole('group', { name: 'Agents' })).toBeInTheDocument();
    expect(screen.getByText('2 of 2 selected')).toBeInTheDocument();
    expect(entry).toHaveAttribute('aria-expanded', 'false');
    fireEvent.click(screen.getByRole('button', { name: 'Close' }));
    await waitFor(() => expect(screen.queryByRole('dialog')).not.toBeInTheDocument());
    expect(loadSession).not.toHaveBeenCalled();
    expect(state.selectSession).not.toHaveBeenCalled();
    expect(state.setAgent).not.toHaveBeenCalled();
    expect(state.createSession).not.toHaveBeenCalled();
    expect(state.exportSession).not.toHaveBeenCalled();
});

test('opens the export menu from the keyboard and offers history when current ID is stale', async () => {
    renderSidebar({ currentSessionId: 'missing' });
    const entry = screen.getByRole('button', { name: 'Export sessions' });
    fireEvent.keyDown(entry, { key: 'ArrowDown' });
    expect(await screen.findByRole('menuitem', { name: 'Current session' })).toHaveAttribute('aria-disabled', 'true');
    fireEvent.click(screen.getByRole('menuitem', { name: 'Historical sessions' }));
    expect(await screen.findByRole('dialog', { name: 'Export historical sessions' })).toBeVisible();
});

test('dismisses the export menu with Escape and returns focus to its trigger', async () => {
    renderSidebar();
    const entry = screen.getByRole('button', { name: 'Export sessions' });
    entry.focus();
    fireEvent.keyDown(entry, { key: 'ArrowDown' });
    expect(await screen.findByRole('menuitem', { name: 'Historical sessions' })).toBeInTheDocument();
    fireEvent.keyDown(window, { key: 'Escape', keyCode: 27 });
    await waitFor(() => expect(entry).toHaveAttribute('aria-expanded', 'false'));
    expect(entry).toHaveFocus();
});

test('passes the rename hook action through to the history popover', () => {
    const state = renderSidebar();
    expect(SessionHistoryPopover).toHaveBeenLastCalledWith(expect.objectContaining({ onRename: state.renameSession }), expect.anything());
});

test('disables agent refresh while discovery is running', async () => {
    renderSidebar({ agentDiscoveryLoading: true });

    fireEvent.click(screen.getByRole('button', { name: /DeepSeek/i }));
    await waitFor(() => expect(screen.getByRole('button', { name: 'Refresh agents' })).toBeDisabled());
});

test('renders theme-following logos for catalog agents', async () => {
    renderSidebar({
        activeAgentName: 'OpenCode',
        agentServers: [
            { name: 'OpenCode', available: true },
            { name: 'Claude Code', available: true },
            { name: 'Codex', available: true },
            { name: 'Trae', available: true },
        ],
    });

    const trigger = screen.getByRole('button', { name: /^OpenCode$/i });
    expect(trigger.querySelector('[data-agent-icon="themed"]')).not.toBeNull();

    fireEvent.click(trigger);
    expect((await screen.findByRole('option', { name: 'OpenCode' })).querySelector('[data-agent-icon="themed"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Codex' })).querySelector('[data-agent-icon="themed"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Claude Code' })).querySelector('[data-agent-icon="static"]')).not.toBeNull();
    expect((await screen.findByRole('option', { name: 'Trae' })).querySelector('[data-agent-icon="static"]')).not.toBeNull();
});

test('hides undetected catalog agents from the picker', async () => {
    renderSidebar({
        agentServers: [
            { name: 'DeepSeek', available: true },
            { name: 'OpenCode', available: false },
        ],
    });

    fireEvent.click(screen.getByRole('button', { name: /DeepSeek/i }));
    expect(await screen.findByRole('option', { name: 'DeepSeek' })).toBeVisible();
    expect(screen.queryByRole('option', { name: /OpenCode/i })).not.toBeInTheDocument();
});

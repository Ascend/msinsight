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
import { act, fireEvent, render, screen, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import * as api from '../../api';
import { ChatStateProvider, useChatState } from '../../hooks/useChatState';
import { ChatPanel } from '../../components/ChatPanel';
import { SessionSidebar } from '../../components/SessionSidebar';

jest.mock('antd', () => ({
    Drawer: ({ children, open, title }: any) => open ? <section aria-label="Agent Settings"><h2>{title}</h2>{children}</section> : null,
    message: {
        error: jest.fn(),
        success: jest.fn(),
        warning: jest.fn(),
    },
}));

jest.mock('@insight/lib/components', () => ({
    Button: ({ children, size: _size, type: _buttonType, ...props }: any) => <button {...props} type="button">{children}</button>,
    Select: ({ onChange, options, value, width: _width, ...props }: any) => (
        <select
            aria-label={props['aria-label'] ?? 'select'}
            disabled={props.disabled}
            id={props.id}
            onChange={(event) => onChange(event.target.value)}
            value={value}
        >
            {options.map((option: any) => <option key={option.value} value={option.value}>{option.label}</option>)}
        </select>
    ),
}), { virtual: true });

jest.mock('@insight/lib/icon/Icon', () => ({
    DeleteIcon: () => <span>delete-icon</span>,
}), { virtual: true });

jest.mock('react-markdown', () => {
    const React = require('react');
    return {
        __esModule: true,
        default: ({ children }: { children: string }) => React.createElement('span', null, children),
    };
});

jest.mock('remark-gfm', () => ({
    __esModule: true,
    default: jest.fn(),
}));

jest.mock('../../env', () => ({
    apiUrl: (path: string) => path,
}));

jest.mock('../../api', () => ({
    cancelPrompt: jest.fn(),
    createSession: jest.fn(),
    deleteSession: jest.fn(),
    fetchAgentConfig: jest.fn(),
    fetchAgents: jest.fn(),
    fetchSessions: jest.fn(),
    fetchState: jest.fn(),
    loadSession: jest.fn(),
    respondPermission: jest.fn(),
    refreshAgents: jest.fn(),
    saveAgentServersConfig: jest.fn(),
    saveAgentSessionConfig: jest.fn(),
    saveBuiltinAgentConfig: jest.fn(),
    sendPrompt: jest.fn(),
    setSessionMode: jest.fn(),
    setSessionModel: jest.fn(),
    switchAgent: jest.fn(),
}));

const mockFetchState = api.fetchState as jest.Mock;
const mockFetchSessions = api.fetchSessions as jest.Mock;
const mockLoadSession = api.loadSession as jest.Mock;
const mockFetchAgentConfig = api.fetchAgentConfig as jest.Mock;
const mockFetchAgents = api.fetchAgents as jest.Mock;
const mockSaveAgentServersConfig = api.saveAgentServersConfig as jest.Mock;
const mockSendPrompt = api.sendPrompt as jest.Mock;

const previousAssistantReply = 'previous assistant reply that must stay visible';

const snapshot = {
    activeAgentName: 'OpenCode',
    agentServers: [
        { name: 'OpenCode', command: 'opencode', args: ['acp'], env: {} },
        { name: 'Claude', command: 'claude', args: ['--print'], env: {} },
    ],
    builtinAgent: { schemaVersion: 1, name: 'msinsight-native' as const, provider: 'openai', model: 'cx/gpt-5.5', baseUrl: 'http://127.0.0.1:19099/v1', apiKey: '' },
    sessionConfig: {
        requestTimeoutMs: 30000,
        promptRequestTimeoutMs: 300000,
        permissionRequestTimeoutMs: 300000,
        defaultAllowlist: {
            includeDocsRoot: true,
            includeAgentWorkspaceRoot: true,
            includeProjectRoot: true,
            extraPaths: [],
        },
    },
};

interface FakeEventSourceInstance {
    url: string;
    onmessage: ((event: MessageEvent) => void) | null;
    close: jest.Mock;
    emit: (data: unknown) => void;
}

const fakeEventSourceInstances: FakeEventSourceInstance[] = [];

function FakeEventSource(this: FakeEventSourceInstance, url: string): void {
    this.url = url;
    this.onmessage = null;
    this.close = jest.fn();
    this.emit = (data: unknown): void => {
        this.onmessage?.({ data: JSON.stringify(data) } as MessageEvent);
    };
    fakeEventSourceInstances.push(this);
}

const ChatStateProbe = (): JSX.Element => {
    const { activeAgentName, agentServers, currentSessionId } = useChatState();
    return (
        <div>
            <output aria-label="active agent">{activeAgentName ?? ''}</output>
            <output aria-label="agent servers">{agentServers.map((agent) => agent.name).join(',')}</output>
            <output aria-label="current session">{currentSessionId ?? ''}</output>
        </div>
    );
};

beforeEach(() => {
    fakeEventSourceInstances.length = 0;
    (globalThis as any).EventSource = FakeEventSource;
    (HTMLElement.prototype as any).scrollTo = jest.fn();

    mockFetchState.mockResolvedValue({
        initialized: true,
        activeAgentName: 'OpenCode',
        agentServers: [{ name: 'OpenCode' }],
        availableCommands: [],
        availableSkills: [],
        configOptions: [],
    });
    mockFetchSessions.mockResolvedValue([{ sessionId: 'session-open', title: 'Open session', updatedAt: 'Earlier' }]);
    mockLoadSession.mockResolvedValue({
        messages: [{ id: 'assistant-1', role: 'assistant', text: previousAssistantReply }],
        configOptions: [],
        pendingPrompt: false,
    });
    mockFetchAgentConfig.mockResolvedValue(snapshot);
    mockFetchAgents.mockResolvedValue({
        activeAgentName: 'Claude',
        agentServers: [{ name: 'OpenCode' }, { name: 'Claude' }],
        discoveryLoading: false,
    });
    mockSaveAgentServersConfig.mockResolvedValue({ ok: true, snapshot: { ...snapshot, activeAgentName: 'Claude' } });
    mockSendPrompt.mockResolvedValue({ ok: true, sessionId: 'session-claude' });
});

afterEach(() => {
    jest.clearAllMocks();
});

test('settings save success preserves visible messages, applies agent state from a backend event, and avoids the stale session', async () => {
    mockSaveAgentServersConfig.mockResolvedValue({ ok: true });

    render(
        <ChatStateProvider>
            <SessionSidebar />
            <ChatPanel />
            <ChatStateProbe />
        </ChatStateProvider>,
    );

    expect(await screen.findByText(previousAssistantReply)).toBeVisible();

    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    expect(await screen.findByText('Agent Configuration')).toBeVisible();
    fireEvent.change(await screen.findByLabelText('Command'), { target: { value: 'opencode-updated' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    act(() => {
        fakeEventSourceInstances[0].emit({
            type: 'state',
            state: {
                activeAgentName: 'Claude',
                agentServers: [{ name: 'OpenCode' }, { name: 'Claude' }],
            },
        });
    });

    expect(screen.getByLabelText('active agent')).toHaveTextContent('Claude');
    expect(screen.getByLabelText('agent servers')).toHaveTextContent('OpenCode,Claude');
    expect(screen.getByText(previousAssistantReply)).toBeVisible();

    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'prompt after event reload' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));

    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt).toHaveBeenCalledWith('prompt after event reload', true, undefined, [], undefined);
});

test('settings save-and-switch preserves visible messages, refreshes agent state, and does not send the next prompt to the stale session', async () => {
    render(
        <ChatStateProvider>
            <SessionSidebar />
            <ChatPanel />
            <ChatStateProbe />
        </ChatStateProvider>,
    );

    expect(await screen.findByText(previousAssistantReply)).toBeVisible();
    expect(screen.getByLabelText('active agent')).toHaveTextContent('OpenCode');
    expect(screen.getByLabelText('current session')).toHaveTextContent('session-open');

    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    expect(await screen.findByText('Agent Configuration')).toBeVisible();
    fireEvent.change(await screen.findByLabelText('Agent to edit'), { target: { value: 'Claude' } });
    fireEvent.click(screen.getByLabelText('Save and switch to selected agent'));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(screen.getByLabelText('active agent')).toHaveTextContent('Claude'));
    expect(screen.getByLabelText('agent servers')).toHaveTextContent('OpenCode,Claude');
    expect(screen.getByText(previousAssistantReply)).toBeVisible();

    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'prompt after reload' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));

    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt).toHaveBeenCalledWith('prompt after reload', true, undefined, [], undefined);
});

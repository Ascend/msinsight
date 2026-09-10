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
import { webcrypto } from 'crypto';
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
    Input: (props: any) => <input {...props} />,
    InputNumber: ({ onChange, value, ...props }: any) => <input {...props} onChange={(event) => onChange(Number(event.target.value))} type="number" value={value} />,
    PasswordInput: (props: any) => <input {...props} type="password" />,
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
    Tooltip: ({ children, title }: any) => (
        <>
            {children}
            {title ? <span role="tooltip">{title}</span> : null}
        </>
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
const originalCrypto = Object.getOwnPropertyDescriptor(globalThis, 'crypto');

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
    Object.defineProperty(globalThis, 'crypto', { configurable: true, value: webcrypto });
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
        messages: [{ id: 'assistant-1', role: 'assistant', content: [{ id: 'text-1', type: 'text', text: previousAssistantReply }] }],
        configOptions: [],
        pendingPrompt: false,
    });
    mockFetchAgentConfig.mockResolvedValue(JSON.parse(JSON.stringify(snapshot)));
    mockFetchAgents.mockResolvedValue({
        activeAgentName: 'Claude',
        agentServers: [{ name: 'OpenCode' }, { name: 'Claude' }],
        discoveryLoading: false,
    });
    mockSaveAgentServersConfig.mockResolvedValue({ ok: true, snapshot: { ...snapshot, activeAgentName: 'Claude' } });
    mockSendPrompt.mockResolvedValue({ ok: true, sessionId: 'session-claude' });
});

afterEach(() => {
    if (originalCrypto) Object.defineProperty(globalThis, 'crypto', originalCrypto);
    else Reflect.deleteProperty(globalThis, 'crypto');
    jest.clearAllMocks();
    jest.restoreAllMocks();
});

test('loads welcome pickers before the first prompt and preserves draft mode when changing models', async () => {
    const configOptions = [
        { id: 'mode', category: 'mode', type: 'select', currentValue: 'default', options: [
            { value: 'default', name: 'Agent' }, { value: 'plan', name: 'Plan' },
        ] },
        { id: 'model', category: 'model', type: 'select', currentValue: 'model-a', options: [
            { value: 'model-a', name: 'Model A' }, { value: 'model-b', name: 'Model B' },
        ] },
    ];
    mockFetchSessions.mockResolvedValue([]);
    mockFetchAgents.mockResolvedValue({ activeAgentName: 'OpenCode', agentServers: [{ name: 'OpenCode' }], discoveryLoading: false });
    mockFetchState.mockResolvedValue({ initialized: false, activeAgentName: 'OpenCode', configOptions: [] });
    render(<ChatStateProvider><ChatPanel /></ChatStateProvider>);

    await waitFor(() => expect(mockFetchAgents).toHaveBeenCalled());
    expect(screen.getByRole('button', { name: 'Loading...' })).toBeDisabled();

    const readyState = { initialized: true, activeAgentName: 'OpenCode', configOptions };
    mockFetchState.mockResolvedValue(readyState);
    act(() => fakeEventSourceInstances[0].emit({ type: 'state', state: readyState }));

    await waitFor(() => expect(screen.getByRole('button', { name: 'Agent' })).toBeEnabled());
    expect(screen.getByRole('button', { name: 'Model A' })).toBeEnabled();
    expect(mockSendPrompt).not.toHaveBeenCalled();
    expect(api.createSession).not.toHaveBeenCalled();

    fireEvent.click(screen.getByRole('button', { name: 'Agent' }));
    fireEvent.click(await screen.findByRole('option', { name: 'Plan' }));
    expect(api.setSessionMode).not.toHaveBeenCalled();

    const modelResponse = { configOptions: configOptions.map((option) => option.id === 'model' ? { ...option, currentValue: 'model-b' } : option) };
    (api.setSessionModel as jest.Mock).mockResolvedValue(modelResponse);
    fireEvent.click(screen.getByRole('button', { name: 'Model A' }));
    fireEvent.click(await screen.findByRole('option', { name: 'Model B' }));
    await waitFor(() => expect(api.setSessionModel).toHaveBeenCalledWith('model-b', undefined));
    await waitFor(() => expect(screen.getByRole('button', { name: 'Model B' })).toBeEnabled());
    act(() => fakeEventSourceInstances[0].emit({ type: 'state', state: { ...readyState, ...modelResponse } }));
    expect(screen.getByRole('button', { name: 'Plan' })).toBeEnabled();

    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'hello' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledWith('hello', true, undefined, [], 'plan', undefined, undefined));

    act(() => fakeEventSourceInstances[0].emit({ type: 'state', state: { ...readyState, activeAgentName: 'Claude' } }));
    expect(screen.getByRole('button', { name: 'Agent' })).toBeEnabled();
    expect(screen.queryByRole('button', { name: 'Plan' })).not.toBeInTheDocument();
});

test.each([
    [false, false],
    [true, false],
    [true, true],
])('freezes thinking time at the first answer while streaming continues (timeline: %s, first text via delta: %s)', async (withTimeline, firstTextViaDelta) => {
    const sessionId = 'session-open';
    mockSendPrompt.mockResolvedValueOnce({ ok: true, sessionId });
    render(<ChatStateProvider><ChatPanel /></ChatStateProvider>);
    await screen.findByText(previousAssistantReply);
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'analyze data' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));

    let now = 1000;
    jest.spyOn(Date, 'now').mockImplementation(() => now);
    const events = fakeEventSourceInstances[0];
    act(() => {
        events.emit({ type: 'message_added', sessionId, message: { id: 'timed-reply', role: 'assistant', content: [] } });
        events.emit({ type: 'prompt_status', sessionId, pendingPrompt: true });
    });
    now = 4000;
    act(() => {
        if (withTimeline) {
            events.emit({ type: 'message_content_added', sessionId, id: 'timed-reply', block: { id: 'thought', type: 'thinking', text: 'Inspecting data', startedAt: 1000 } });
        }
        if (firstTextViaDelta) {
            events.emit({ type: 'message_content_added', sessionId, id: 'timed-reply', block: { id: 'answer', type: 'text', text: ' ' } });
        }
    });
    expect(document.querySelector('.thinking-sparkle')).toBeInTheDocument();

    now = 6000;
    act(() => {
        events.emit(firstTextViaDelta
            ? { type: 'message_content_delta', sessionId, id: 'timed-reply', blockId: 'answer', blockType: 'text', delta: 'First answer' }
            : { type: 'message_content_added', sessionId, id: 'timed-reply', block: { id: 'answer', type: 'text', text: 'First answer' } });
    });
    expect(screen.getByText('Thought completed 5.0s')).toBeVisible();
    expect(document.querySelector('.thinking-sparkle')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Cancel' })).toBeVisible();

    now = 9000;
    act(() => events.emit({ type: 'message_content_delta', sessionId, id: 'timed-reply', blockId: 'answer', blockType: 'text', delta: ' continues streaming' }));
    expect(screen.getByText(/First answer continues streaming/)).toBeVisible();
    expect(screen.getByText('Thought completed 5.0s')).toBeVisible();
    expect(screen.getByRole('button', { name: 'Cancel' })).toBeVisible();

    now = 12000;
    act(() => events.emit({ type: 'prompt_status', sessionId, pendingPrompt: false }));
    expect(screen.getByText('Thought completed 5.0s')).toBeVisible();
    expect(screen.queryByRole('button', { name: 'Cancel' })).not.toBeInTheDocument();
});

test.each([
    [true, true],
    [true, false],
    [false, true],
    [false, false],
])('ends thinking after a configuration error (new session: %s, completion before HTTP response: %s)', async (newSession, completionFirst) => {
    const sessionId = newSession ? 'new-session' : 'session-open';
    if (newSession) mockFetchSessions.mockResolvedValue([]);
    let resolvePrompt!: (value: { ok: boolean; sessionId: string }) => void;
    mockSendPrompt.mockImplementationOnce(() => new Promise((resolve) => { resolvePrompt = resolve; }));
    render(<ChatStateProvider><ChatPanel /><ChatStateProbe /></ChatStateProvider>);
    if (newSession) {
        await waitFor(() => expect(mockFetchSessions).toHaveBeenCalled());
    } else {
        await screen.findByText(previousAssistantReply);
    }
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'analyze data' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));
    expect(screen.getByRole('button', { name: 'Cancel' })).toBeVisible();

    const error = 'Error: AI SDK runtime is not configured. Set MSINSIGHT_NATIVE_API_KEY.';
    const finishPrompt = (): void => {
        const events = fakeEventSourceInstances[0];
        events.emit({ type: 'message_added', sessionId, message: { id: 'request-1', role: 'user', content: [{ id: 'request-text', type: 'text', text: 'analyze data' }] } });
        events.emit({ type: 'message_added', sessionId, message: { id: 'reply-1', role: 'assistant', content: [] } });
        events.emit({ type: 'prompt_status', sessionId, pendingPrompt: true });
        events.emit({ type: 'message_content_added', sessionId, id: 'reply-1', block: { id: 'error-1', type: 'text', text: error } });
        events.emit({ type: 'prompt_status', sessionId, pendingPrompt: false });
    };
    if (completionFirst) act(finishPrompt);
    await act(async () => { resolvePrompt({ ok: true, sessionId }); });
    if (!completionFirst) {
        expect(screen.getByRole('button', { name: 'Cancel' })).toBeVisible();
        act(finishPrompt);
    }

    expect(await screen.findByText(error)).toBeVisible();
    expect(screen.queryByText(/^Thinking(?:\s|$)/)).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Cancel' })).not.toBeInTheDocument();
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'retry after configuring' } });
    expect(screen.getByRole('button', { name: 'Send' })).toBeEnabled();
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
    expect(mockSendPrompt).toHaveBeenCalledWith('prompt after event reload', true, undefined, [], undefined, undefined, undefined);
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
    fireEvent.click(await screen.findByRole('button', { name: /Claude/ }));
    fireEvent.click(screen.getByLabelText('Save and switch to selected agent'));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(screen.getByLabelText('active agent')).toHaveTextContent('Claude'));
    expect(screen.getByLabelText('agent servers')).toHaveTextContent('OpenCode,Claude');
    expect(screen.getByText(previousAssistantReply)).toBeVisible();

    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'prompt after reload' } });
    fireEvent.click(screen.getByRole('button', { name: 'Send' }));

    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt).toHaveBeenCalledWith('prompt after reload', true, undefined, [], undefined, undefined, undefined);
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import * as testingLibrary from '@testing-library/react';
import * as api from '../../api';
import { ChatStateProvider, useChatState } from '../../hooks/useChatState';
import type { AgentConfigSnapshot, ConfigOption, ServerEvent } from '../../types';
import { executeFrontendCommand } from '../../bridge/frontendAgentCommandTransport';
import { message } from 'antd';

const { act, render, waitFor } = testingLibrary;

jest.mock('antd', () => ({ message: { error: jest.fn(), warning: jest.fn() } }));
jest.mock('../../bridge/frontendAgentCommandTransport', () => ({
    executeFrontendCommand: jest.fn(), cancelFrontendCommand: jest.fn(),
}));
jest.mock('../../env', () => ({
    apiUrl: (path: string) => path,
    eventsUrl: () => '/api/events',
    capabilityAuthHeaders: () => ({}),
}));
jest.mock('../../api', () => ({
    fetchState: jest.fn(),
    fetchAgents: jest.fn(),
    fetchSessions: jest.fn(),
    refreshAgents: jest.fn(),
    switchAgent: jest.fn(),
    loadSession: jest.fn(),
    sendPrompt: jest.fn(),
    cancelPrompt: jest.fn(),
    setSessionMode: jest.fn(),
    isBackendUnavailableError: jest.fn(() => false),
    claimFrontendCommand: jest.fn(),
    respondFrontendCommand: jest.fn(),
}));

let chat: ReturnType<typeof useChatState>;
let eventSource: { onmessage?: (event: { data: string }) => void };
const originalEventSource = globalThis.EventSource;
const mockSendPrompt = api.sendPrompt as jest.Mock;

const Probe = (): null => {
    chat = useChatState();
    return null;
};

const deferred = (): {
    promise: Promise<{ sessionId: string }>;
    resolve: (response: { sessionId: string }) => void;
    reject: (error: Error) => void;
} => {
    let resolveResponse!: (response: { sessionId: string }) => void;
    let rejectResponse!: (error: Error) => void;
    const promise = new Promise<{ sessionId: string }>((resolve, reject) => {
        resolveResponse = resolve;
        rejectResponse = reject;
    });
    return { promise, resolve: resolveResponse, reject: rejectResponse };
};

const send = async (text: string): Promise<void> => {
    act(() => chat.setInput(text));
    await act(async () => { void chat.sendMessage(); });
};

const emit = (event: ServerEvent): void => {
    act(() => eventSource.onmessage?.({ data: JSON.stringify(event) }));
};

const select = async (sessionId: string): Promise<void> => {
    const session = chat.sessions.find((item) => item.sessionId === sessionId);
    if (!session) throw new Error(`Missing session: ${sessionId}`);
    await act(async () => { await chat.selectSession(session); });
};

beforeEach(async () => {
    jest.resetAllMocks();
    (api.fetchState as jest.Mock).mockResolvedValue({ initialized: true, activeAgentName: 'agent-a', configOptions: [] });
    (api.fetchAgents as jest.Mock).mockResolvedValue({ agentServers: [], discoveryLoading: false });
    (api.fetchSessions as jest.Mock).mockResolvedValue([]);
    (api.cancelPrompt as jest.Mock).mockResolvedValue({ ok: true });
    mockSendPrompt.mockResolvedValue({ sessionId: 'session-a' });
    globalThis.EventSource = jest.fn(() => {
        const source = { onmessage: undefined, close: jest.fn() };
        eventSource = source;
        return source;
    }) as unknown as typeof EventSource;
    render(<ChatStateProvider><Probe /></ChatStateProvider>);
    await waitFor(() => expect(chat.isDraftSession).toBe(true));
});

afterEach(() => {
    globalThis.EventSource = originalEventSource;
});

const modelOptions = (currentValue: string, ids: string[]): ConfigOption[] => [{
    id: 'model', name: 'Model', category: 'model', type: 'select', currentValue,
    options: ids.map((value) => ({ value, name: value })),
}];
const savedConfig = { activeAgentName: 'agent-a' } as AgentConfigSnapshot;

test('a state snapshot clears discovery loading after a missed completion event', () => {
    emit({ type: 'agent_discovery_started' });
    expect(chat.agentDiscoveryLoading).toBe(true);
    emit({ type: 'state', state: { discoveryLoading: false } });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test.each(['fetchAgents', 'fetchState'] as const)('a late initial %s response cannot restore completed discovery loading', async (method) => {
    testingLibrary.cleanup();
    let finish!: (value: { discoveryLoading: boolean }) => void;
    (api[method] as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    render(<ChatStateProvider><Probe /></ChatStateProvider>);
    emit({ type: 'state', state: { discoveryLoading: false } });
    await act(async () => { finish({ discoveryLoading: true }); });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('a late list response cannot restore loading after another initial request failed', async () => {
    testingLibrary.cleanup();
    let finish!: (value: { discoveryLoading: boolean }) => void;
    (api.fetchState as jest.Mock).mockRejectedValueOnce(new Error('state unavailable'));
    (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    render(<ChatStateProvider><Probe /></ChatStateProvider>);
    await waitFor(() => expect(chat.agentDiscoveryLoading).toBe(false));
    await act(async () => { finish({ discoveryLoading: true }); });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('a late settings list response cannot restore completed discovery loading', async () => {
    let finish!: (value: { discoveryLoading: boolean }) => void;
    (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    let applying!: Promise<void>;
    act(() => { applying = chat.applyAgentConfigSnapshot(savedConfig); });
    emit({ type: 'state', state: { discoveryLoading: false } });
    await act(async () => {
        finish({ discoveryLoading: true });
        await applying;
    });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('an old initialization failure does not dismiss a newly started discovery', async () => {
    testingLibrary.cleanup();
    let fail!: (error: Error) => void;
    (api.fetchState as jest.Mock).mockReturnValueOnce(new Promise((_resolve, reject) => { fail = reject; }));
    render(<ChatStateProvider><Probe /></ChatStateProvider>);
    emit({ type: 'agent_discovery_started' });
    await act(async () => { fail(new Error('old state unavailable')); });
    expect(chat.agentDiscoveryLoading).toBe(true);
    emit({ type: 'state', state: { discoveryLoading: false } });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('a fast manual refresh clears loading without receiving an SSE event', async () => {
    (api.refreshAgents as jest.Mock).mockResolvedValueOnce({ ok: true });
    await act(async () => { await chat.refreshAgents(); });
    expect(api.refreshAgents).toHaveBeenCalledTimes(1);
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('manual refresh still synchronizes when only its start notification has arrived', async () => {
    (api.refreshAgents as jest.Mock).mockImplementationOnce(async () => {
        emit({ type: 'agent_discovery_started' });
        return { ok: true };
    });
    await act(async () => { await chat.refreshAgents(); });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('manual refresh and its completion notification do not reset runtime sessions twice', async () => {
    (api.refreshAgents as jest.Mock).mockImplementationOnce(async () => {
        emit({ type: 'agent_discovery_completed', runtimeChanged: true });
        return { ok: true, runtimeChanged: true };
    });
    await act(async () => { await chat.refreshAgents(); });
    expect(api.fetchSessions).toHaveBeenCalledTimes(2);
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test.each([false, true])('response-first completion is applied only once, notification during synchronization: %s', async (duringSync) => {
    let finish!: (value: { discoveryLoading: boolean }) => void;
    if (duringSync) {
        (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    }
    (api.refreshAgents as jest.Mock).mockResolvedValueOnce({ ok: true, runtimeChanged: true, discoveryId: 'scan-1' });
    let refreshing!: Promise<void>;
    act(() => { refreshing = chat.refreshAgents(); });
    if (duringSync) {
        await waitFor(() => expect(api.fetchAgents).toHaveBeenCalledTimes(2));
        emit({ type: 'agent_discovery_completed', runtimeChanged: true, discoveryId: 'scan-1' });
        await act(async () => { finish({ discoveryLoading: false }); await refreshing; });
    } else {
        await act(async () => { await refreshing; });
        emit({ type: 'agent_discovery_completed', runtimeChanged: true, discoveryId: 'scan-1' });
    }
    expect(api.fetchSessions).toHaveBeenCalledTimes(2);
    expect(api.fetchAgents).toHaveBeenCalledTimes(2);
    expect(chat.agentDiscoveryLoading).toBe(false);
    emit({ type: 'agent_discovery_completed', runtimeChanged: true, discoveryId: 'scan-2' });
    await waitFor(() => expect(api.fetchSessions).toHaveBeenCalledTimes(3));
});

test.each(['sync', 'refresh'] as const)('an old %s failure cannot dismiss a newer discovery mask', async (source) => {
    let fail!: (error: Error) => void;
    let refreshing: Promise<void> | undefined;
    const request = new Promise((_resolve, reject) => { fail = reject; });
    if (source === 'sync') {
        (api.fetchAgents as jest.Mock).mockReturnValueOnce(request);
        emit({ type: 'agent_discovery_completed' });
    } else {
        (api.refreshAgents as jest.Mock).mockReturnValueOnce(request);
        act(() => { refreshing = chat.refreshAgents(); });
    }
    emit({ type: 'agent_discovery_started' });
    await act(async () => { fail(new Error('old request failed')); await refreshing; });
    expect(chat.agentDiscoveryLoading).toBe(true);
    emit({ type: 'agent_discovery_completed' });
    await waitFor(() => expect(chat.agentDiscoveryLoading).toBe(false));
});

test.each(['fetchAgents', 'fetchState', 'settings'] as const)('stale %s responses cannot roll back agent data', async (source) => {
    let finish!: (value: object) => void;
    const oldResponse = new Promise((resolve) => { finish = resolve; });
    let applying: Promise<void> | undefined;
    if (source === 'settings') {
        (api.fetchAgents as jest.Mock).mockReturnValueOnce(oldResponse);
        act(() => { applying = chat.applyAgentConfigSnapshot(savedConfig); });
    } else {
        testingLibrary.cleanup();
        (api[source] as jest.Mock).mockReturnValueOnce(oldResponse);
        render(<ChatStateProvider><Probe /></ChatStateProvider>);
    }
    const agentServers = [{ name: 'agent-b', command: 'agent-b', args: [], env: {} }];
    (api.fetchState as jest.Mock).mockResolvedValue({ activeAgentName: 'agent-b', initialized: true });
    (api.fetchAgents as jest.Mock).mockResolvedValue({ activeAgentName: 'agent-b', agentServers, discoveryLoading: false });
    emit({ type: 'state', state: { activeAgentName: 'agent-b', initialized: true } });
    emit({ type: 'agent_discovery_completed' });
    await waitFor(() => expect(chat.agentServers).toEqual(agentServers));
    await act(async () => {
        finish({ activeAgentName: 'old-agent', agentServers: [], initialized: false, agentError: 'old error', discoveryLoading: true });
        await applying;
    });
    expect(chat.activeAgentName).toBe('agent-b');
    expect(chat.agentServers).toEqual(agentServers);
    expect(chat.agentError).toBeUndefined();
    expect(chat.configOptionsLoading).toBe(false);
});

test('overlapping discovery completions synchronize the latest result instead of retaining a stale loading flag', async () => {
    let finish!: (value: { discoveryLoading: boolean }) => void;
    (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    emit({ type: 'agent_discovery_completed' });
    emit({ type: 'agent_discovery_started' });
    emit({ type: 'agent_discovery_completed' });
    await act(async () => { finish({ discoveryLoading: true }); });
    await waitFor(() => expect(chat.agentDiscoveryLoading).toBe(false));
    expect(api.fetchAgents).toHaveBeenCalledTimes(3);
});

test('a failed coalesced discovery request clears loading and reports the error once', async () => {
    let fail!: (error: Error) => void;
    (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((_resolve, reject) => { fail = reject; }));
    emit({ type: 'agent_discovery_started' });
    emit({ type: 'agent_discovery_completed' });
    emit({ type: 'agent_discovery_completed' });
    await act(async () => { fail(new Error('list unavailable')); });
    await waitFor(() => expect(chat.agentDiscoveryLoading).toBe(false));
    expect(message.error).toHaveBeenCalledTimes(1);
});

test('coalesced completion notifications preserve the latest runtime-change flag', async () => {
    let finish!: (value: { discoveryLoading: boolean }) => void;
    (api.fetchAgents as jest.Mock).mockReturnValueOnce(new Promise((resolve) => { finish = resolve; }));
    emit({ type: 'agent_discovery_completed' });
    emit({ type: 'agent_discovery_completed', runtimeChanged: true });
    await act(async () => { finish({ discoveryLoading: false }); });
    await waitFor(() => expect(api.fetchSessions).toHaveBeenCalledTimes(2));
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test('a failed manual refresh clears discovery loading', async () => {
    (api.refreshAgents as jest.Mock).mockRejectedValueOnce(new Error('refresh failed'));
    await act(async () => { await chat.refreshAgents(); });
    expect(chat.agentDiscoveryLoading).toBe(false);
});

test.each(['switch', 'sessions'])('a failed agent %s request clears the switching mask', async (stage) => {
    if (stage === 'switch') {
        (api.switchAgent as jest.Mock).mockRejectedValueOnce(new Error('switch failed'));
    } else {
        (api.switchAgent as jest.Mock).mockResolvedValueOnce({ activeAgentName: 'agent-b' });
        (api.fetchSessions as jest.Mock).mockRejectedValueOnce(new Error('sessions unavailable'));
    }
    await act(async () => { await chat.setAgent('agent-b'); });
    expect(chat.switchingAgent).toBe(false);
    expect(message.error).toHaveBeenCalledTimes(1);
});

test('a lost completed receipt never reexecutes or changes to execution-failed', async () => {
    (api.claimFrontendCommand as jest.Mock).mockResolvedValue({ claimed: true, claimToken: 'owner' });
    (executeFrontendCommand as jest.Mock).mockResolvedValue({ changed: true });
    (api.respondFrontendCommand as jest.Mock).mockRejectedValue(new TypeError('lost'));
    const request = { type: 'frontend_command_request' as const, requestId: 'r', sessionId: 's', command: 'observe', args: {}, deadline: Date.now() + 5000 };
    emit(request);
    emit(request);
    await waitFor(() => expect(message.warning).toHaveBeenCalledTimes(1));
    expect(executeFrontendCommand).toHaveBeenCalledTimes(1);
    expect(api.respondFrontendCommand).toHaveBeenCalledTimes(2);
    const bodies = (api.respondFrontendCommand as jest.Mock).mock.calls.map(([receipt]) => JSON.parse(receipt.json));
    expect(bodies[0].status).toBe('completed');
    expect(bodies[1]).toEqual(bodies[0]);
});

test.each(['cancel', 'unmount'])('receipt %s aborts the active send without resending', async (action) => {
    (api.claimFrontendCommand as jest.Mock).mockResolvedValue({ claimed: true, claimToken: 'owner' });
    (executeFrontendCommand as jest.Mock).mockResolvedValue({ changed: true });
    let signal: AbortSignal | undefined;
    (api.respondFrontendCommand as jest.Mock).mockImplementation((_receipt, options) => new Promise((_resolve, reject) => {
        signal = options.signal;
        signal?.addEventListener('abort', () => reject(new DOMException('Cancelled', 'AbortError')));
    }));
    emit({ type: 'frontend_command_request', requestId: 'active', sessionId: 's', command: 'observe', args: {}, deadline: Date.now() + 5000 });
    await waitFor(() => expect(api.respondFrontendCommand).toHaveBeenCalledTimes(1));
    if (action === 'cancel') emit({ type: 'frontend_command_cancel', requestId: 'active', reason: 'cancelled' });
    else testingLibrary.cleanup();
    await waitFor(() => expect(signal?.aborted).toBe(true));
    expect(executeFrontendCommand).toHaveBeenCalledTimes(1);
    expect(api.respondFrontendCommand).toHaveBeenCalledTimes(1);
    expect(message.warning).not.toHaveBeenCalled();
});

test('saving the same agent refreshes draft and cached session models without replacing conversations', async () => {
    emit({ type: 'state', state: { configOptions: modelOptions('old', ['old']) } });
    await send('first');
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    emit({ type: 'message_added', sessionId: 'session-a', message: { id: 'reply-a', role: 'assistant', content: [] } });
    await act(async () => { await chat.createSession(); });
    mockSendPrompt.mockResolvedValueOnce({ sessionId: 'session-b' });
    await send('second');
    emit({ type: 'prompt_status', sessionId: 'session-b', pendingPrompt: false });
    emit({ type: 'message_added', sessionId: 'session-b', message: { id: 'reply-b', role: 'assistant', content: [] } });
    const messages = chat.messages;
    const refreshed = modelOptions('new-default', ['new-default', 'session-choice']);
    (api.fetchState as jest.Mock).mockResolvedValue({
        initialized: true, activeAgentName: 'agent-a', configOptions: refreshed, agentCapabilities: { loadSession: true },
    });
    (api.loadSession as jest.Mock).mockImplementation(async (sessionId: string) => ({
        sessionId, messages: [], configOptions: modelOptions(sessionId === 'session-a' ? 'session-choice' : 'new-default', ['new-default', 'session-choice']),
    }));

    await act(async () => { await chat.applyAgentConfigSnapshot(savedConfig); });

    expect(api.loadSession).toHaveBeenCalledWith('session-a', { refreshConfigOnly: true });
    expect(api.loadSession).toHaveBeenCalledWith('session-b', { refreshConfigOnly: true });
    expect(chat.currentSessionId).toBe('session-b');
    expect(chat.messages).toEqual(messages);
    expect(chat.configOptions).toEqual(refreshed);
    await select('session-a');
    expect(chat.configOptions[0].currentValue).toBe('session-choice');
    expect(chat.configOptions[0].options.map((option) => option.value)).toEqual(['new-default', 'session-choice']);
    expect(chat.messages.map((message) => message.id)).toContain('reply-a');
    await act(async () => { await chat.createSession(); });
    expect(chat.configOptions).toEqual(refreshed);
});

test('saving an agent without session loading still refreshes draft options', async () => {
    const refreshed = modelOptions('new', ['new']);
    (api.fetchState as jest.Mock).mockResolvedValue({ initialized: true, activeAgentName: 'agent-a', configOptions: refreshed });
    await act(async () => { await chat.applyAgentConfigSnapshot(savedConfig); });
    expect(chat.configOptions).toEqual(refreshed);
    expect(api.loadSession).not.toHaveBeenCalled();
});

test('a failed options refresh preserves history and does not block other sessions', async () => {
    await send('first');
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    await act(async () => { await chat.createSession(); });
    mockSendPrompt.mockResolvedValueOnce({ sessionId: 'session-b' });
    await send('second');
    emit({ type: 'prompt_status', sessionId: 'session-b', pendingPrompt: false });
    emit({ type: 'message_added', sessionId: 'session-b', message: { id: 'reply-b', role: 'assistant', content: [] } });
    const messages = chat.messages;
    const refreshed = modelOptions('new', ['new']);
    (api.fetchState as jest.Mock).mockResolvedValue({ initialized: true, activeAgentName: 'agent-a', agentCapabilities: { loadSession: true } });
    (api.loadSession as jest.Mock).mockRejectedValueOnce(new Error('reload failed')).mockResolvedValueOnce({ configOptions: refreshed });
    await act(async () => { await chat.applyAgentConfigSnapshot(savedConfig); });
    expect(api.loadSession).toHaveBeenCalledTimes(2);
    expect(chat.currentSessionId).toBe('session-b');
    expect(chat.messages).toEqual(messages);
    expect(chat.configOptions).toEqual(refreshed);
});

test('saving a different active agent does not reopen the previous agent sessions', async () => {
    await send('first');
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    (api.fetchAgents as jest.Mock).mockResolvedValue({ activeAgentName: 'agent-b', agentServers: [] });
    await act(async () => { await chat.applyAgentConfigSnapshot({ ...savedConfig, activeAgentName: 'agent-b' }); });
    expect(api.loadSession).not.toHaveBeenCalled();
    expect(chat.sessions).toEqual([]);
    expect(chat.isDraftSession).toBe(true);
});

test('permission mode can change during a reply and becomes the next draft default', async () => {
    const modes = (currentValue: string): ConfigOption[] => [{
        id: 'mode', category: 'mode', name: 'Permissions', type: 'select', currentValue,
        options: [{ value: 'ask', name: 'Ask for Approval' }, { value: 'full-access', name: 'Full Access' }],
    }];
    emit({ type: 'state', state: { configOptions: modes('ask') } });
    await send('first');
    expect(chat.pendingPrompt).toBe(true);
    (api.setSessionMode as jest.Mock).mockResolvedValue({ configOptions: modes('full-access') });
    await act(async () => { await chat.setMode('full-access'); });
    expect(api.setSessionMode).toHaveBeenCalledWith('full-access', 'session-a');
    expect(chat.configOptions[0].currentValue).toBe('full-access');
    await act(async () => { await chat.createSession(); });
    expect(chat.configOptions[0].currentValue).toBe('full-access');
    await send('second');
    expect(mockSendPrompt).toHaveBeenLastCalledWith('second', true, undefined, [], 'full-access', undefined, undefined);
});

test('a failed live permission change does not change the new draft default', async () => {
    await send('first');
    (api.setSessionMode as jest.Mock).mockRejectedValue(new Error('mode rejected'));
    await act(async () => { await chat.setMode('full-access'); });
    await act(async () => { await chat.createSession(); });
    await send('second');
    expect(mockSendPrompt).toHaveBeenLastCalledWith('second', true, undefined, [], undefined, undefined, undefined);
});

test('preserves sent and replayed image blocks when switching sessions', async () => {
    const image = { id: 'image', name: 'chart.png', mimeType: 'image/png', data: 'iVBORw0KGgo=' };
    act(() => chat.addImages([image]));
    await send('Explain this image');
    expect(chat.images).toEqual([]);
    expect(mockSendPrompt).toHaveBeenLastCalledWith('Explain this image', true, undefined, [image], undefined, undefined, undefined);

    const message = { id: 'user-image', role: 'user' as const, content: [{ ...image, type: 'image' as const }] };
    emit({ type: 'message_added', sessionId: 'session-a', message });
    await act(async () => { await chat.createSession(); });
    const replayed = { ...image, id: 'replayed-image', type: 'image' as const };
    emit({ type: 'message_content_added', sessionId: 'session-a', id: message.id, block: replayed });
    await select('session-a');
    expect(chat.messages[0].content).toEqual([message.content[0], replayed]);
});

test('opens a draft while a session runs and keeps its background replies available', async () => {
    await send('first');
    expect(chat.pendingPrompt).toBe(true);
    await act(async () => { await chat.createSession(); });
    expect(chat.isDraftSession).toBe(true);
    expect(chat.pendingPrompt).toBe(false);
    const reply = { id: 'reply-a', role: 'assistant' as const, content: [] };
    emit({ type: 'message_added', sessionId: 'session-a', message: reply });
    expect(chat.messages).toEqual([]);
    expect(chat.sessions.find((session) => session.sessionId === 'session-a')?.pendingPrompt).toBe(true);
    await select('session-a');
    expect(chat.messages).toEqual([reply]);
    expect(chat.pendingPrompt).toBe(true);
    expect(api.loadSession).not.toHaveBeenCalled();
});

test('an existing session acknowledgement cannot activate a newly opened draft', async () => {
    await send('first');
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    const request = deferred();
    mockSendPrompt.mockReturnValueOnce(request.promise);
    await send('follow-up');
    await act(async () => { await chat.createSession(); });
    await act(async () => request.resolve({ sessionId: 'session-a' }));
    expect(chat.isDraftSession).toBe(true);
    expect(chat.currentSessionId).toBeUndefined();
    expect(chat.pendingPrompt).toBe(false);
});

test.each([true, false])('initial acknowledgements preserve selection and composer (newest resolves first: %s)', async (newestFirst) => {
    const first = deferred();
    const second = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise).mockReturnValueOnce(second.promise);
    await send('first');
    const firstId = chat.currentSessionId;
    await act(async () => { await chat.createSession(); });
    await send('second');
    expect(chat.currentSessionId).not.toBe(firstId);
    expect(chat.sessions.filter((session) => session.isPending)).toHaveLength(2);
    const image = { id: 'image', name: 'image.png', mimeType: 'image/png', data: 'data' };
    act(() => { chat.setInput('unsent'); chat.addImages([image]); });
    if (newestFirst) {
        await act(async () => second.resolve({ sessionId: 'session-b' }));
        await act(async () => first.resolve({ sessionId: 'session-a' }));
    } else {
        const secondId = chat.currentSessionId;
        await act(async () => first.resolve({ sessionId: 'session-a' }));
        expect(chat.currentSessionId).toBe(secondId);
        expect(chat.pendingPrompt).toBe(true);
        await act(async () => second.resolve({ sessionId: 'session-b' }));
    }
    expect(chat.currentSessionId).toBe('session-b');
    expect(chat.pendingPrompt).toBe(true);
    expect(chat.input).toBe('unsent');
    expect(chat.images).toEqual([image]);
    expect(chat.sessions.map((session) => session.sessionId).sort()).toEqual(['session-a', 'session-b']);
});

test('a failed initial request leaves a newer pending conversation intact', async () => {
    const first = deferred();
    const second = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise).mockReturnValueOnce(second.promise);
    await send('first');
    await act(async () => { await chat.createSession(); });
    await send('second');
    const secondId = chat.currentSessionId;
    await act(async () => first.reject(new Error('creation failed')));
    expect(chat.currentSessionId).toBe(secondId);
    expect(chat.pendingPrompt).toBe(true);
    expect(chat.sessions).toHaveLength(1);
    await act(async () => second.resolve({ sessionId: 'session-b' }));
});

test('a failed active creation returns to a draft that can start a fresh session', async () => {
    mockSendPrompt.mockRejectedValueOnce(new Error('creation failed'));
    await send('first');
    expect(chat.isDraftSession).toBe(true);
    expect(chat.pendingPrompt).toBe(false);
    expect(chat.sessions).toEqual([]);
    await send('retry');
    expect(mockSendPrompt).toHaveBeenLastCalledWith('retry', true, undefined, [], undefined, undefined, undefined);
    expect(chat.currentSessionId).toBe('session-a');
});

test.each(['active', 'draft', 'pending'])('creation failure preserves queued messages with %s selected', async (selection) => {
    const first = deferred();
    const retry = deferred();
    const newer = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise);
    await send('first');
    const provisionalId = chat.currentSessionId;
    const image = { id: 'queued-image', name: 'queued.png', mimeType: 'image/png', data: 'queued-data' };
    act(() => {
        chat.selectWelcomePrompt('queued-one', 'memory-tuning-assistant');
        chat.addImages([image]);
    });
    await act(async () => { await chat.sendMessage(); });
    await send('queued-two');
    expect(chat.queuedCount).toBe(2);

    if (selection !== 'active') await act(async () => { await chat.createSession(); });
    if (selection === 'pending') {
        mockSendPrompt.mockReturnValueOnce(newer.promise);
        await send('newer');
    }
    const selectedId = chat.currentSessionId;
    act(() => { chat.setInput('unsent'); chat.addImages([image]); });
    mockSendPrompt.mockReturnValueOnce(retry.promise);
    await act(async () => first.reject(new Error('creation failed')));
    expect(mockSendPrompt).toHaveBeenLastCalledWith('queued-one', true, undefined, [image], undefined, undefined, 'memory-tuning-assistant');
    expect(chat.currentSessionId).toBe(selectedId);
    expect(chat.isDraftSession).toBe(selection === 'draft');
    expect(chat.sessions.some((session) => session.sessionId === provisionalId)).toBe(true);

    await act(async () => retry.resolve({ sessionId: 'session-a' }));
    expect(chat.currentSessionId).toBe(selection === 'active' ? 'session-a' : selectedId);
    expect(chat.input).toBe('unsent');
    expect(chat.images).toEqual([image]);
    await select('session-a');
    expect(chat.queuedPrompts.map((prompt) => prompt.text)).toEqual(['queued-two']);
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenLastCalledWith('queued-two', false, 'session-a', [], undefined, undefined, undefined));
    expect(chat.queuedCount).toBe(0);
    expect(mockSendPrompt).toHaveBeenCalledTimes(selection === 'pending' ? 4 : 3);
    if (selection === 'pending') await act(async () => newer.resolve({ sessionId: 'session-b' }));
});

test('queued messages survive consecutive creation failures', async () => {
    const first = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise);
    await send('first');
    await send('queued-one');
    await send('queued-two');
    mockSendPrompt.mockRejectedValueOnce(new Error('retry failed'));
    await act(async () => first.reject(new Error('creation failed')));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(3));
    expect(mockSendPrompt.mock.calls.slice(1).map((call) => call.slice(0, 3))).toEqual([
        ['queued-one', true, undefined],
        ['queued-two', true, undefined],
    ]);
    expect(chat.currentSessionId).toBe('session-a');
    expect(chat.queuedCount).toBe(0);
});

test('cancelling a queued creation still waits for its real ID after the failed request finishes cleanup', async () => {
    const first = deferred();
    const retry = deferred();
    const refresh = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise).mockReturnValueOnce(retry.promise);
    await send('first');
    await send('queued-one');
    await send('queued-two');
    (api.fetchState as jest.Mock).mockReturnValueOnce(refresh.promise);
    await act(async () => first.reject(new Error('creation failed')));
    expect(mockSendPrompt).toHaveBeenCalledTimes(2);
    await act(async () => refresh.resolve({ sessionId: 'unused' }));
    await act(async () => { void chat.cancelMessage(); });
    expect(api.cancelPrompt).not.toHaveBeenCalled();
    expect(chat.queuedCount).toBe(0);
    await act(async () => retry.resolve({ sessionId: 'session-a' }));
    expect(api.cancelPrompt).toHaveBeenCalledWith('session-a');
    expect(chat.pendingPrompt).toBe(false);
    expect(mockSendPrompt).toHaveBeenCalledTimes(2);
});

test.each(['memory-tuning-assistant', 'slow-card-analysis-assistant'] as const)('queued %s messages follow their original session even when SSE completes before its acknowledgement', async (preset) => {
    const first = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise);
    await send('first');
    act(() => chat.selectWelcomePrompt('queued', preset));
    await act(async () => { await chat.sendMessage(); });
    expect(chat.queuedCount).toBe(1);
    await act(async () => { await chat.createSession(); });
    emit({ type: 'message_added', sessionId: 'session-a', message: { id: 'a', role: 'assistant', content: [] } });
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    expect(mockSendPrompt).toHaveBeenCalledTimes(1);
    await act(async () => first.resolve({ sessionId: 'session-a' }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(2));
    expect(mockSendPrompt).toHaveBeenLastCalledWith('queued', false, 'session-a', [], undefined, undefined, preset);
    expect(chat.isDraftSession).toBe(true);
    expect(chat.queuedCount).toBe(0);
    await select('session-a');
    expect(chat.messages[0].id).toBe('a');
});

test('a fast completed initial reply stays completed after acknowledgement', async () => {
    const request = deferred();
    mockSendPrompt.mockReturnValueOnce(request.promise);
    await send('first');
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    await act(async () => request.resolve({ sessionId: 'session-a' }));
    expect(chat.pendingPrompt).toBe(false);
    expect(chat.sessions[0].status).toBe('completed');
});

test('independent background queues run without waiting for another session acknowledgement', async () => {
    await send('first');
    await send('queued-a1');
    await send('queued-a2');
    await act(async () => { await chat.createSession(); });
    mockSendPrompt.mockResolvedValueOnce({ sessionId: 'session-b' });
    await send('second');
    await send('queued-b');
    const queuedA = deferred();
    mockSendPrompt.mockReturnValueOnce(queuedA.promise).mockResolvedValueOnce({ sessionId: 'session-b' });
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    emit({ type: 'prompt_status', sessionId: 'session-b', pendingPrompt: false });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(4));
    expect(mockSendPrompt.mock.calls[3].slice(0, 3)).toEqual(['queued-b', false, 'session-b']);
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    expect(mockSendPrompt).toHaveBeenCalledTimes(4);
    await act(async () => queuedA.resolve({ sessionId: 'session-a' }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(5));
    expect(mockSendPrompt.mock.calls[4].slice(0, 3)).toEqual(['queued-a2', false, 'session-a']);
});

test('cancelling an initial request waits for its real ID and does not cancel the new draft', async () => {
    const request = deferred();
    mockSendPrompt.mockReturnValueOnce(request.promise);
    await send('first');
    await send('queued');
    await act(async () => { void chat.cancelMessage(); });
    expect(api.cancelPrompt).not.toHaveBeenCalled();
    expect(chat.queuedCount).toBe(0);
    await act(async () => { await chat.createSession(); });
    await act(async () => request.resolve({ sessionId: 'session-a' }));
    expect(api.cancelPrompt).toHaveBeenCalledWith('session-a');
    expect(chat.isDraftSession).toBe(true);
    expect(chat.pendingPrompt).toBe(false);
    expect(mockSendPrompt).toHaveBeenCalledTimes(1);
});

test('a stale creation acknowledgement cannot restore sessions after switching agents', async () => {
    const request = deferred();
    mockSendPrompt.mockReturnValueOnce(request.promise);
    await send('first');
    emit({ type: 'state', state: { activeAgentName: 'agent-b' } });
    await act(async () => request.resolve({ sessionId: 'session-a' }));
    expect(chat.sessions).toEqual([]);
    expect(chat.isDraftSession).toBe(true);
});

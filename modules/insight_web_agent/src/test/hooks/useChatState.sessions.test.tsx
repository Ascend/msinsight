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
import type { ServerEvent } from '../../types';

const { act, render, waitFor } = testingLibrary;

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('../../env', () => ({ apiUrl: (path: string) => path }));
jest.mock('../../api', () => ({
    fetchState: jest.fn(),
    fetchAgents: jest.fn(),
    fetchSessions: jest.fn(),
    loadSession: jest.fn(),
    sendPrompt: jest.fn(),
    cancelPrompt: jest.fn(),
    isBackendUnavailableError: jest.fn(() => false),
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

test('queued messages follow their original session even when SSE completes before its acknowledgement', async () => {
    const first = deferred();
    mockSendPrompt.mockReturnValueOnce(first.promise);
    await send('first');
    act(() => chat.selectWelcomePrompt('queued', 'memory-tuning-assistant'));
    await act(async () => { await chat.sendMessage(); });
    expect(chat.queuedCount).toBe(1);
    await act(async () => { await chat.createSession(); });
    emit({ type: 'message_added', sessionId: 'session-a', message: { id: 'a', role: 'assistant', content: [] } });
    emit({ type: 'prompt_status', sessionId: 'session-a', pendingPrompt: false });
    expect(mockSendPrompt).toHaveBeenCalledTimes(1);
    await act(async () => first.resolve({ sessionId: 'session-a' }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(2));
    expect(mockSendPrompt).toHaveBeenLastCalledWith('queued', false, 'session-a', [], undefined, undefined, 'memory-tuning-assistant');
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

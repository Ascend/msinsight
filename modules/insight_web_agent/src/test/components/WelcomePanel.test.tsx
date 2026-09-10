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
import i18n from '@insight/lib/i18n';
import * as api from '../../api';
import { ChatStateProvider, useChatState } from '../../hooks/useChatState';
import { WelcomePanel } from '../../components/WelcomePanel';
import { Composer } from '../../components/Composer';

jest.mock('antd', () => ({ message: { error: jest.fn() } }));
jest.mock('@insight/lib/components', () => ({ Select: () => null }), { virtual: true });
jest.mock('../../env', () => ({ apiUrl: (path: string) => path }));
jest.mock('../../api', () => ({
    fetchState: jest.fn(),
    fetchAgents: jest.fn(),
    fetchSessions: jest.fn(),
    sendPrompt: jest.fn(),
}));

const mockSendPrompt = api.sendPrompt as jest.Mock;
let eventSource: { onmessage?: (event: { data: string }) => void };
const originalEventSource = globalThis.EventSource;

beforeEach(async () => {
    await i18n.changeLanguage('zhCN');
    (api.fetchState as jest.Mock).mockResolvedValue({ initialized: true, configOptions: [] });
    (api.fetchAgents as jest.Mock).mockResolvedValue({ agentServers: [], discoveryLoading: false });
    (api.fetchSessions as jest.Mock).mockResolvedValue([]);
    mockSendPrompt.mockResolvedValue({ ok: true, sessionId: 'session-1' });
    (globalThis as any).EventSource = class {
        constructor() { eventSource = this; }
        onmessage?: (event: { data: string }) => void;
        close = jest.fn();
    };
});

afterEach(async () => {
    jest.clearAllMocks();
    globalThis.EventSource = originalEventSource;
    await i18n.changeLanguage('enUS');
});

const StateProbe = (): JSX.Element => {
    const { createSession, isDraftSession } = useChatState();
    return <button onClick={() => createSession()} type="button">{isDraftSession ? 'New draft' : 'New session'}</button>;
};

const renderWelcome = async (): Promise<void> => {
    render(<ChatStateProvider><WelcomePanel /><Composer /><StateProbe /></ChatStateProvider>);
    await screen.findByRole('button', { name: 'New draft' });
};

const clickGuide = (title: string): void => {
    const button = screen.getByRole('button', { name: new RegExp(title) });
    button.focus();
    fireEvent.click(button);
};

test.each([
    ['内存优化助手', '使用内存分析助手帮我分析当前数据'],
    ['快慢卡分析助手', '使用快慢卡分析助手帮我分析当前集群数据'],
    ['咨询领域知识', '使用领域知识库帮我回答：'],
])('fills and focuses the composer when clicking %s, including repeated clicks', async (title, prompt) => {
    await renderWelcome();
    for (let index = 0; index < 2; index += 1) {
        clickGuide(title);
        const input = screen.getByRole('textbox') as HTMLTextAreaElement;
        expect(input).toHaveValue(prompt);
        expect(input).toHaveFocus();
        expect(input.selectionStart).toBe(prompt.length);
        expect(input.selectionEnd).toBe(prompt.length);
    }
    expect(mockSendPrompt).not.toHaveBeenCalled();
});

test('sends the memory preset with edited input and clears it for the next message', async () => {
    await renderWelcome();
    clickGuide('内存优化助手');
    fireEvent.change(screen.getByRole('textbox'), { target: { value: '使用内存分析助手帮我分析当前数据，关注峰值' } });
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledWith(
        '使用内存分析助手帮我分析当前数据，关注峰值', true, undefined, [], undefined, undefined, 'memory-tuning-assistant',
    ));

    act(() => eventSource.onmessage?.({ data: JSON.stringify({ type: 'prompt_status', sessionId: 'session-1', pendingPrompt: false }) }));
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'next question' } });
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenLastCalledWith(
        'next question', false, 'session-1', [], undefined, undefined, undefined,
    ));
});

test.each(['快慢卡分析助手', '咨询领域知识'])('replaces the memory preset when selecting %s', async (title) => {
    await renderWelcome();
    clickGuide('内存优化助手');
    clickGuide(title);
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt.mock.calls[0][6]).toBeUndefined();
});

test('clearing the input removes the memory preset', async () => {
    await renderWelcome();
    clickGuide('内存优化助手');
    fireEvent.change(screen.getByRole('textbox'), { target: { value: '' } });
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'ordinary question' } });
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt.mock.calls[0][6]).toBeUndefined();
});

test('starting a new draft removes the memory preset', async () => {
    await renderWelcome();
    clickGuide('内存优化助手');
    fireEvent.click(screen.getByRole('button', { name: 'New draft' }));
    expect(screen.getByRole('textbox')).toHaveValue('');
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'new draft question' } });
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await waitFor(() => expect(mockSendPrompt).toHaveBeenCalledTimes(1));
    expect(mockSendPrompt.mock.calls[0][6]).toBeUndefined();
});

test('keeps the selected preset when a prompt is queued', async () => {
    await renderWelcome();
    fireEvent.change(screen.getByRole('textbox'), { target: { value: 'first question' } });
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    await screen.findByRole('button', { name: 'New session' });
    clickGuide('内存优化助手');
    expect(screen.getByRole('button', { name: '发送' })).toBeEnabled();
    fireEvent.change(screen.getByRole('textbox'), { target: { value: '' } });
    expect(screen.getByRole('button', { name: '取消' })).toBeVisible();
    clickGuide('内存优化助手');
    fireEvent.click(screen.getByRole('button', { name: '发送' }));
    expect(screen.getByRole('textbox')).toHaveValue('');
    expect(screen.getByRole('button', { name: '取消' })).toBeVisible();
    expect(mockSendPrompt).toHaveBeenCalledTimes(1);
    act(() => eventSource.onmessage?.({ data: JSON.stringify({ type: 'prompt_status', sessionId: 'session-1', pendingPrompt: false }) }));
    await waitFor(() => expect(mockSendPrompt).toHaveBeenLastCalledWith(
        '使用内存分析助手帮我分析当前数据', false, 'session-1', [], undefined, undefined, 'memory-tuning-assistant',
    ));
});

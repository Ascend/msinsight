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
import { fetchAgentConfig, saveAgentServersConfig, saveAgentSessionConfig, saveBuiltinAgentConfig } from '../../api';
import { useChatState } from '../../hooks/useChatState';
import { AgentSettingsDialog } from '../../components/AgentSettingsDialog';
import { ChatPanel } from '../../components/ChatPanel';

jest.mock('antd', () => ({
    Drawer: ({ children, open, title }: any) => open ? <section aria-label="Agent Settings"><h2>{title}</h2>{children}</section> : null,
    message: {
        error: jest.fn(),
        success: jest.fn(),
        warning: jest.fn(),
    },
}));

jest.mock('@insight/lib/components', () => ({
    Button: ({ children, ...props }: any) => <button {...props}>{children}</button>,
    Select: ({ onChange, options, value, ...props }: any) => (
        <select aria-label={props['aria-label'] ?? 'select'} onChange={(event) => onChange(event.target.value)} value={value}>
            {options.map((option: any) => <option key={option.value} value={option.value}>{option.label}</option>)}
        </select>
    ),
}), { virtual: true });

jest.mock('@insight/lib/icon/Icon', () => ({
    SetIcon: () => <span>settings-icon</span>,
}), { virtual: true });

jest.mock('../../api', () => ({
    fetchAgentConfig: jest.fn(),
    saveAgentServersConfig: jest.fn(),
    saveAgentSessionConfig: jest.fn(),
    saveBuiltinAgentConfig: jest.fn(),
}));

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

const mockUseChatState = useChatState as jest.Mock;

jest.mock('../../components/Composer', () => ({
    Composer: () => <div>composer</div>,
}));

jest.mock('../../components/MessageList', () => ({
    MessageList: () => <div>messages</div>,
}));

const snapshot = {
    activeAgentName: 'OpenCode',
    agentServers: [
        { name: 'OpenCode', command: 'opencode', args: ['acp'], env: { ACP_DEBUG: '1' } },
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
            extraPaths: ['missing/path'],
        },
    },
};

const mockFetchAgentConfig = fetchAgentConfig as jest.Mock;
const mockSaveAgentServersConfig = saveAgentServersConfig as jest.Mock;
const mockSaveAgentSessionConfig = saveAgentSessionConfig as jest.Mock;
const mockSaveBuiltinAgentConfig = saveBuiltinAgentConfig as jest.Mock;

beforeEach(() => {
    mockUseChatState.mockReturnValue({
        messages: [],
        messagesRef: { current: null },
        pendingPrompt: false,
        respondToPermission: jest.fn(),
        applyAgentConfigSnapshot: jest.fn(),
    });
    mockFetchAgentConfig.mockResolvedValue(snapshot);
    mockSaveAgentServersConfig.mockResolvedValue({ ok: true, snapshot });
    mockSaveAgentSessionConfig.mockResolvedValue({ ok: true, snapshot });
    mockSaveBuiltinAgentConfig.mockResolvedValue({ ok: true, snapshot });
});

afterEach(() => {
    jest.clearAllMocks();
});

test('settings entry opens and displays current config snapshot', async () => {
    render(<ChatPanel />);

    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));

    expect(await screen.findByText('Agent Configuration')).toBeVisible();
    expect(mockFetchAgentConfig).toHaveBeenCalledTimes(1);
    expect(screen.getByLabelText('Agent to edit')).toHaveValue('OpenCode');
    expect(screen.getByLabelText('Command')).toHaveValue('opencode');
    expect(screen.getByDisplayValue('acp')).toBeVisible();
    expect(screen.getByDisplayValue('ACP_DEBUG')).toBeVisible();
    fireEvent.click(screen.getByRole('button', { name: 'Expand' }));
    expect(screen.getByDisplayValue('missing/path')).toBeVisible();
});

test('switches to edited non-active existing agent on save', async () => {
    mockFetchAgentConfig.mockResolvedValue({
        ...snapshot,
        agentServers: [
            { name: 'OpenCode', command: 'opencode', args: ['acp'], env: { ACP_DEBUG: '1' } },
            { name: 'Claude', command: 'claude', args: ['--old'], env: { OLD_ENV: 'legacy' } },
        ],
    });

    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.change(screen.getByLabelText('Agent to edit'), { target: { value: 'Claude' } });
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude-code' } });
    fireEvent.click(screen.getByRole('button', { name: 'Remove arg 1' }));
    fireEvent.click(screen.getByRole('button', { name: 'Add arg' }));
    fireEvent.change(screen.getByLabelText('Arg 1'), { target: { value: '--model=sonnet' } });
    fireEvent.click(screen.getByRole('button', { name: 'Remove env 1' }));
    fireEvent.click(screen.getByRole('button', { name: 'Add env entry' }));
    fireEvent.change(screen.getByLabelText('Env key 1'), { target: { value: 'ANTHROPIC_AUTH_TOKEN' } });
    fireEvent.change(screen.getByLabelText('Env value 1'), { target: { value: 'token' } });
    fireEvent.click(screen.getByLabelText('Save and switch to selected agent'));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig.mock.calls[0][0]).toEqual(expect.objectContaining({
        activeAgentName: 'Claude',
        agentServers: expect.arrayContaining([
            expect.objectContaining({
                name: 'Claude',
                command: 'claude-code',
                args: ['--model=sonnet'],
                env: { ANTHROPIC_AUTH_TOKEN: 'token' },
            }),
        ]),
    }));
});

test('rejects empty args before save', async () => {
    render(<ChatPanel />);
    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.change(screen.getByDisplayValue('acp'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('Args cannot be empty.')).toBeVisible();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('adds a new agent and saves without switching by default', async () => {
    render(<ChatPanel />);
    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('New agent name'), { target: { value: 'Claude' } });
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig.mock.calls[0][0].activeAgentName).toBe('OpenCode');
    expect(mockSaveAgentServersConfig.mock.calls[0][0].agentServers).toEqual(expect.arrayContaining([
        expect.objectContaining({ name: 'Claude', command: 'claude' }),
    ]));
});

test('removes the last existing env row and saves an empty env object', async () => {
    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.click(screen.getByRole('button', { name: 'Remove env 1' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig.mock.calls[0][0].agentServers).toEqual(expect.arrayContaining([
        expect.objectContaining({ name: 'OpenCode', env: {} }),
    ]));
});

test('adds draft agent args and multiple env rows when saving and switching to the new agent', async () => {
    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('New agent name'), { target: { value: 'Claude' } });
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add arg' }));
    fireEvent.change(screen.getByLabelText('Arg 1'), { target: { value: '--model=sonnet' } });
    fireEvent.change(screen.getByLabelText('Env key 1'), { target: { value: 'ANTHROPIC_AUTH_TOKEN' } });
    fireEvent.change(screen.getByLabelText('Env value 1'), { target: { value: 'token' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add env entry' }));
    fireEvent.change(screen.getByLabelText('Env key 2'), { target: { value: 'ANTHROPIC_BASE_URL' } });
    fireEvent.change(screen.getByLabelText('Env value 2'), { target: { value: 'https://example.test' } });
    fireEvent.click(screen.getByLabelText('Save and switch to this agent'));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig.mock.calls[0][0]).toEqual(expect.objectContaining({
        activeAgentName: 'Claude',
        agentServers: expect.arrayContaining([
            expect.objectContaining({
                name: 'Claude',
                command: 'claude',
                args: ['--model=sonnet'],
                env: {
                    ANTHROPIC_AUTH_TOKEN: 'token',
                    ANTHROPIC_BASE_URL: 'https://example.test',
                },
            }),
        ]),
    }));
});

test('adds and removes multiple extra path rows before save', async () => {
    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.click(screen.getByRole('button', { name: 'Expand' }));
    fireEvent.click(screen.getByRole('button', { name: 'Add path' }));
    fireEvent.change(screen.getByLabelText('Extra allowlist paths 2'), { target: { value: 'tmp/path' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add path' }));
    fireEvent.change(screen.getByLabelText('Extra allowlist paths 3'), { target: { value: 'remove/me' } });
    fireEvent.click(screen.getByRole('button', { name: 'Remove path 3' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentSessionConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentSessionConfig.mock.calls[0][0].defaultAllowlist.extraPaths).toEqual([
        'missing/path',
        'tmp/path',
    ]);
});

test('shows a clear busy message and disables save while a prompt is in flight', async () => {
    mockUseChatState.mockReturnValue({
        messages: [],
        messagesRef: { current: null },
        pendingPrompt: true,
        respondToPermission: jest.fn(),
        applyAgentConfigSnapshot: jest.fn(),
    });

    render(<ChatPanel />);
    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    await screen.findByText('Agent Configuration');

    expect(screen.getByText('Agent is busy. Wait for the current prompt to finish before saving settings.')).toBeVisible();
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('settings save and reload keep the messages list untouched', async () => {
    const existingMessages = [
        { id: 'msg-1', role: 'assistant' as const, text: 'previous assistant reply that must stay visible' },
    ];
    let applyMock = jest.fn();
    let currentMessages: typeof existingMessages = existingMessages;
    mockUseChatState.mockImplementation(() => ({
        messages: currentMessages,
        messagesRef: { current: null },
        pendingPrompt: false,
        respondToPermission: jest.fn(),
        applyAgentConfigSnapshot: (nextSnapshot: unknown) => {
            applyMock(nextSnapshot);
            currentMessages = existingMessages;
        },
    }));
    mockSaveAgentServersConfig.mockResolvedValue({ ok: true, snapshot: { ...snapshot, activeAgentName: 'OpenCode' } });

    render(<ChatPanel />);

    expect(screen.getByText('messages')).toBeVisible();

    fireEvent.click(screen.getByRole('button', { name: 'Agent settings' }));
    await screen.findByText('Agent Configuration');
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(applyMock).toHaveBeenCalledTimes(1));

    expect(screen.getByText('messages')).toBeVisible();
    expect(currentMessages).toEqual(existingMessages);
});

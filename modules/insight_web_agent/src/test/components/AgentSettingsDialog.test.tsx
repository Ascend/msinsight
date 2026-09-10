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
import { act, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import '@testing-library/jest-dom';
import { fetchAgentConfig, saveAgentServersConfig, saveAgentSessionConfig, saveBuiltinAgentConfig } from '../../api';
import { useChatState } from '../../hooks/useChatState';
import { AgentSettingsDialog } from '../../components/AgentSettingsDialog';
import { ChatPanel } from '../../components/ChatPanel';

jest.mock('antd', () => ({
    Drawer: ({ children, onClose, open, title }: any) => open ? <section aria-label="Agent Settings"><h2>{title}</h2><button aria-label="Close drawer" onClick={onClose} />{children}</section> : null,
    Modal: ({ children, footer, open, title }: any) => open ? <section aria-label={title} role="dialog">{children}{footer}</section> : null,
    message: {
        error: jest.fn(),
        success: jest.fn(),
        warning: jest.fn(),
    },
}));

jest.mock('@insight/lib/components', () => ({
    Alert: ({ message }: any) => <div role="alert">{message}</div>,
    Button: ({ children, ...props }: any) => <button {...props}>{children}</button>,
    Input: (props: any) => <input {...props} />,
    InputNumber: ({ onChange, value, ...props }: any) => <input {...props} onChange={(event) => onChange(Number(event.target.value))} type="number" value={value} />,
    PasswordInput: (props: any) => <input {...props} type="password" />,
    Select: ({ onChange, options, value, ...props }: any) => (
        <select aria-label={props['aria-label'] ?? 'select'} onChange={(event) => onChange(event.target.value)} value={value}>
            {options.map((option: any) => <option key={option.value} value={option.value}>{option.label}</option>)}
        </select>
    ),
    Tooltip: ({ children, title }: any) => (
        <span>
            {children}
            {title ? <span role="tooltip">{title}</span> : null}
        </span>
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
    isBackendUnavailableError: () => false,
}));

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

jest.mock('../../components/JsonEditor', () => ({
    JsonEditor: ({ ariaLabel, onChange, value }: any) => (
        <textarea aria-label={ariaLabel} onChange={(event) => onChange(event.target.value)} value={value} />
    ),
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

const renderChatPanelWithSettings = (): void => {
    render(<>
        <ChatPanel />
        <AgentSettingsDialog trigger={<button type="button">Open settings</button>} />
    </>);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
};

beforeEach(() => {
    mockUseChatState.mockReturnValue({
        sessions: [],
        messages: [],
        messagesRef: { current: null },
        notices: [],
        pendingPrompt: false,
        respondToPermission: jest.fn(),
        applyAgentConfigSnapshot: jest.fn(),
    });
    let freshSnapshot = JSON.parse(JSON.stringify(snapshot));
    mockFetchAgentConfig.mockResolvedValue(freshSnapshot);
    mockSaveAgentServersConfig.mockImplementation(async (config) => {
        freshSnapshot = { ...freshSnapshot, ...config };
        return { ok: true, snapshot: freshSnapshot };
    });
    mockSaveAgentSessionConfig.mockImplementation(async (sessionConfig) => {
        freshSnapshot = { ...freshSnapshot, sessionConfig };
        return { ok: true, snapshot: freshSnapshot };
    });
    mockSaveBuiltinAgentConfig.mockImplementation(async (builtinAgent) => {
        freshSnapshot = { ...freshSnapshot, builtinAgent };
        return { ok: true, snapshot: freshSnapshot };
    });
});

afterEach(() => {
    jest.clearAllMocks();
});

const openSettings = async (): Promise<void> => {
    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByLabelText('Command');
};

test('save is disabled until config changes, and disabled again when edits are reverted', async () => {
    await openSettings();
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'updated' } });
    expect(screen.getByRole('button', { name: 'Save' })).toBeEnabled();
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'opencode' } });
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();

    fireEvent.click(screen.getByRole('tab', { name: 'Script configuration' }));
    await screen.findByRole('textbox', { name: 'Script configuration' });
    fireEvent.click(screen.getByRole('button', { name: 'Format' }));
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    fireEvent.click(screen.getByRole('button', { name: 'MS Insight_Native' }));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    fireEvent.click(screen.getByLabelText('Save and switch to selected agent'));
    expect(screen.getByRole('button', { name: 'Save' })).toBeEnabled();
});

test('an untouched new agent is clean, and session-only save does not create an empty agent', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    fireEvent.click(screen.getByRole('button', { name: 'Session Config' }));
    fireEvent.change(screen.getByLabelText('Request timeout'), { target: { value: '40000' } });
    expect(screen.getByRole('button', { name: 'Save' })).toBeEnabled();
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(mockSaveAgentSessionConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test.each(['MS Insight_Native', 'Add agent'])('asks before %s and can cancel or discard the edits', async (destination) => {
    await openSettings();
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'unsaved-command' } });
    fireEvent.click(screen.getByRole('button', { name: /OpenCode/ }));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: destination }));
    const modal = screen.getByRole('dialog');
    expect(modal).toHaveTextContent('Do you want to save your changes to OpenCode?');
    fireEvent.click(within(modal).getByRole('button', { name: 'Cancel' }));
    expect(screen.getByLabelText('Command')).toHaveValue('unsaved-command');
    fireEvent.click(screen.getByRole('button', { name: destination }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: "Don't save" }));
    fireEvent.click(screen.getByRole('button', { name: /OpenCode/ }));
    expect(screen.getByLabelText('Command')).toHaveValue('opencode');
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test.each(['MS Insight_Native', 'Add agent'])('saves the current agent before continuing to %s', async (destination) => {
    await openSettings();
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'saved-command' } });
    fireEvent.click(screen.getByRole('button', { name: destination }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Save changes' }));
    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(screen.getByLabelText('Agent name')).toHaveValue(destination === 'Add agent' ? '' : 'MS Insight_Native'));
    expect(screen.getByText('Agent Configuration')).toBeVisible();
    fireEvent.click(screen.getByRole('button', { name: /OpenCode/ }));
    expect(screen.getByLabelText('Command')).toHaveValue('saved-command');
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
});

test('preserves new agent drafts when cancelling and saves a draft before adding another', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('Agent name'), { target: { value: 'Claude' } });
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude' } });
    fireEvent.click(screen.getByRole('button', { name: /OpenCode/ }));
    expect(screen.getByRole('dialog')).toHaveTextContent('Do you want to save your changes to Claude?');
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Cancel' }));
    expect(screen.getByLabelText('Agent name')).toHaveValue('Claude');
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Save changes' }));
    await waitFor(() => expect(screen.getByLabelText('Agent name')).toHaveValue(''));
    expect(screen.getByRole('button', { name: /Claude/ })).toHaveAttribute('aria-pressed', 'false');
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
});

test.each(['Back', 'Cancel', 'Close drawer'])('protects unsaved changes on %s', async (closeButton) => {
    await openSettings();
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'unsaved-command' } });
    fireEvent.click(screen.getByRole('button', { name: closeButton }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Cancel' }));
    expect(screen.getByLabelText('Command')).toHaveValue('unsaved-command');
    fireEvent.click(screen.getByRole('button', { name: closeButton }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: "Don't save" }));
    expect(screen.queryByText('Agent Configuration')).not.toBeInTheDocument();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('failed validation stops navigation and shows errors beside each invalid field', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('Agent name'), { target: { value: 'OpenCode' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add arg' }));
    fireEvent.click(screen.getByRole('button', { name: 'Add env entry' }));
    fireEvent.click(screen.getByRole('button', { name: 'MS Insight_Native' }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Save changes' }));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    for (const label of ['Agent name', 'Command', 'Arg 1', 'Env key 1']) {
        const input = screen.getByLabelText(label);
        expect(input).toHaveAttribute('aria-invalid', 'true');
        const error = document.getElementById(input.getAttribute('aria-describedby') ?? '');
        expect(error).toBeVisible();
        expect(input.parentElement).toContainElement(error);
    }
    fireEvent.change(screen.getByLabelText('Agent name'), { target: { value: 'Claude' } });
    expect(screen.getByLabelText('Agent name')).toHaveAttribute('aria-invalid', 'false');
    expect(screen.queryByText('Agent name must be unique.')).not.toBeInTheDocument();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('builtin required fields validate on blur and clear after correction', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('button', { name: 'MS Insight_Native' }));
    for (const label of ['Provider', 'Model', 'Base URL']) {
        const input = screen.getByLabelText(label);
        fireEvent.change(input, { target: { value: '' } });
        fireEvent.blur(input);
        expect(input).toHaveAttribute('aria-invalid', 'true');
        fireEvent.change(input, { target: { value: 'corrected' } });
        expect(input).toHaveAttribute('aria-invalid', 'false');
    }
});

test('script configuration shows a format help example for the current agent type', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('tab', { name: 'Script configuration' }));
    expect(await screen.findByRole('button', { name: 'View script configuration format' })).toBeVisible();
    expect(screen.getByRole('tooltip')).toHaveTextContent('"command": "claude"');
    expect(screen.getByRole('tooltip')).toHaveTextContent('"args": ["acp"]');

    fireEvent.click(screen.getByRole('button', { name: 'MS Insight_Native' }));
    expect(screen.getByRole('tooltip')).toHaveTextContent('"provider": "openai"');
    expect(screen.getByRole('tooltip')).not.toHaveTextContent('"command": "claude"');

    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    expect(screen.getByRole('tooltip')).toHaveTextContent('"name": "Claude"');
    expect(screen.getByRole('tooltip')).toHaveTextContent('"command": "claude"');
});

test('invalid JSON is preserved when cancelling navigation and cannot bypass save validation', async () => {
    await openSettings();
    fireEvent.click(screen.getByRole('tab', { name: 'Script configuration' }));
    const editor = await screen.findByRole('textbox', { name: 'Script configuration' });
    fireEvent.change(editor, { target: { value: '{ invalid' } });
    fireEvent.click(screen.getByRole('tab', { name: 'Script configuration' }));
    expect(editor).toHaveValue('{ invalid');
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Cancel' }));
    expect(editor).toHaveValue('{ invalid');
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(screen.getByRole('alert').closest('.script-config-panel')).not.toBeNull();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
    fireEvent.change(editor, { target: { value: JSON.stringify({ command: '', args: [], env: {} }) } });
    expect(screen.getByRole('alert')).toHaveTextContent('Command cannot be empty.');
    fireEvent.change(editor, { target: { value: JSON.stringify({ command: 'opencode', args: ['acp'], env: { ACP_DEBUG: '1' } }) } });
    expect(screen.queryByRole('alert')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
});

test('failed save retains the current agent and edits for retry', async () => {
    await openSettings();
    mockSaveAgentServersConfig.mockRejectedValueOnce(new Error('Save failed'));
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'updated' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add agent' }));
    fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Save changes' }));
    expect(await screen.findByText('Save failed')).toBeVisible();
    expect(screen.getByLabelText('Command')).toHaveValue('updated');
    expect(screen.getByRole('button', { name: 'Save' })).toBeEnabled();
    expect(screen.queryByRole('button', { name: 'New agent' })).not.toBeInTheDocument();
});

test('prevents editing or leaving the form while save is in flight', async () => {
    await openSettings();
    let finishSave: (result: unknown) => void = () => {};
    mockSaveAgentServersConfig.mockReturnValueOnce(new Promise((resolve) => { finishSave = resolve; }));
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'updated' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(screen.getByLabelText('Command')).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Add agent' })).toBeDisabled();
    fireEvent.click(screen.getByRole('button', { name: 'Close drawer' }));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    expect(screen.getByText('Agent Configuration')).toBeVisible();
    await act(async () => { finishSave({ ok: true }); });
    expect(screen.queryByText('Agent Configuration')).not.toBeInTheDocument();
    expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1);
});

test('settings entry opens and displays current config snapshot', async () => {
    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);

    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));

    expect(await screen.findByText('Agent Configuration')).toBeVisible();
    expect(mockFetchAgentConfig).toHaveBeenCalledTimes(1);
    expect(await screen.findByRole('button', { name: /OpenCode/ })).toHaveAttribute('aria-pressed', 'true');
    expect(screen.getByLabelText('Command')).toHaveValue('opencode');
    expect(screen.getByDisplayValue('acp')).toBeVisible();
    expect(screen.getByDisplayValue('ACP_DEBUG')).toBeVisible();
    expect(screen.queryByText('Built-in agent settings are stored separately from generic ACP launch configurations.')).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: /MS Insight_Native/ }));
    expect(screen.getByRole('alert')).toHaveTextContent('Built-in agent settings are stored separately from generic ACP launch configurations.');
    fireEvent.click(await screen.findByRole('button', { name: 'Session Config' }));
    expect(await screen.findByDisplayValue('missing/path')).toBeVisible();
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

    fireEvent.click(await screen.findByRole('button', { name: /Claude/ }));
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude-code' } });
    fireEvent.click(screen.getByRole('button', { name: 'Remove arg 1' }));
    fireEvent.click(screen.getByRole('button', { name: 'Add arg' }));
    fireEvent.change(screen.getByLabelText('Arg 1'), { target: { value: '--model=sonnet' } });
    fireEvent.click(await screen.findByRole('button', { name: 'Remove env 1' }));
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
    renderChatPanelWithSettings();
    await screen.findByText('Agent Configuration');
    await screen.findByRole('button', { name: /OpenCode/ });

    fireEvent.change(screen.getByDisplayValue('acp'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('Args cannot be empty.')).toBeVisible();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('adds a new agent and saves without switching by default', async () => {
    renderChatPanelWithSettings();
    await screen.findByText('Agent Configuration');

    fireEvent.click(await screen.findByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('Agent name'), { target: { value: 'Claude' } });
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

    fireEvent.click(await screen.findByRole('button', { name: 'Remove env 1' }));
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

    fireEvent.click(await screen.findByRole('button', { name: 'Add agent' }));
    fireEvent.change(screen.getByLabelText('Agent name'), { target: { value: 'Claude' } });
    fireEvent.change(screen.getByLabelText('Command'), { target: { value: 'claude' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add arg' }));
    fireEvent.change(screen.getByLabelText('Arg 1'), { target: { value: '--model=sonnet' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add env entry' }));
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

    fireEvent.click(await screen.findByRole('button', { name: 'Session Config' }));
    expect(await screen.findByDisplayValue('missing/path')).toBeVisible();
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

test('shows auto-detected agents as read-only and copies them into a custom draft', async () => {
    mockFetchAgentConfig.mockResolvedValue({
        ...snapshot,
        catalogAgents: [
            { name: 'OpenCode(auto)', command: 'opencode', args: ['acp'], env: {}, available: true },
            { name: 'Claude Code(auto)', command: 'claude-agent-acp', args: [], env: {}, available: false },
        ],
    });

    render(<AgentSettingsDialog trigger={<button type="button">Open settings</button>} />);
    fireEvent.click(screen.getByRole('button', { name: 'Open settings' }));
    await screen.findByText('Agent Configuration');

    fireEvent.click(await screen.findByRole('button', { name: /Claude Code\(auto\) \(Unavailable\)/ }));
    expect(screen.getByRole('alert')).toHaveTextContent('This agent was not detected, so it cannot be switched to or edited.');
    expect(screen.getByLabelText('Command')).toHaveValue('claude-agent-acp');
    expect(screen.getByLabelText('Command')).toHaveProperty('readOnly', true);
    expect(screen.queryByRole('button', { name: 'Add arg' })).not.toBeInTheDocument();
    expect(screen.getByLabelText('Save and switch to selected agent')).toBeDisabled();

    fireEvent.click(screen.getByRole('button', { name: 'Copy as custom agent' }));
    expect(screen.getByLabelText('Agent name')).toHaveValue('Claude Code');
    expect(screen.getByLabelText('Command')).toHaveProperty('readOnly', false);
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    expect(mockSaveAgentServersConfig.mock.calls[0][0].agentServers).toEqual(expect.arrayContaining([
        expect.objectContaining({ name: 'Claude Code', command: 'claude-agent-acp' }),
    ]));
});

test('shows a clear busy message and disables save while a prompt is in flight', async () => {
    mockUseChatState.mockReturnValue({
        sessions: [],
        messages: [],
        messagesRef: { current: null },
        pendingPrompt: true,
        respondToPermission: jest.fn(),
        applyAgentConfigSnapshot: jest.fn(),
    });

    renderChatPanelWithSettings();
    await screen.findByText('Agent Configuration');
    await screen.findByRole('button', { name: /OpenCode/ });

    expect(screen.getByText('Agent is busy. Wait for the current prompt to finish before saving settings.')).toBeVisible();
    expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    expect(mockSaveAgentServersConfig).not.toHaveBeenCalled();
});

test('settings save and reload keep the messages list untouched', async () => {
    const existingMessages = [
        { id: 'msg-1', role: 'assistant' as const, content: [{ id: 'text-1', type: 'text' as const, text: 'previous assistant reply that must stay visible' }] },
    ];
    let applyMock = jest.fn();
    let currentMessages: typeof existingMessages = existingMessages;
    mockUseChatState.mockImplementation(() => ({
        sessions: [],
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

    renderChatPanelWithSettings();

    expect(screen.getByText('messages')).toBeVisible();

    await screen.findByText('Agent Configuration');
    fireEvent.change(await screen.findByLabelText('Command'), { target: { value: 'opencode-updated' } });
    fireEvent.click(await screen.findByRole('button', { name: 'Save' }));

    await waitFor(() => expect(mockSaveAgentServersConfig).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(applyMock).toHaveBeenCalledTimes(1));

    expect(screen.getByText('messages')).toBeVisible();
    expect(currentMessages).toEqual(existingMessages);
});

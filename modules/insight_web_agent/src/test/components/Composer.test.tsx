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
import { fireEvent, render, screen } from '@testing-library/react';
import { createRef } from 'react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { Composer } from '../../components/Composer';

jest.mock('@insight/lib/components', () => ({
    Button: ({ children, ...props }: any) => <button {...props}>{children}</button>,
    Select: ({ onChange, options, value }: any) => (
        <select aria-label="config picker" onChange={(event) => onChange(event.target.value)} value={value}>
            {options.map((option: any) => <option key={option.value} value={option.value}>{option.label}</option>)}
        </select>
    ),
}), { virtual: true });

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

const mockUseChatState = useChatState as jest.Mock;

const renderComposer = (overrides: Record<string, unknown> = {}) => {
    const state = {
        addImages: jest.fn(),
        activeAgentName: 'claude',
        agentInfo: undefined,
        availableCommands: [],
        availableSkills: [],
        cancelMessage: jest.fn(),
        clearQueuedPrompts: jest.fn(),
        composerRef: createRef<HTMLTextAreaElement>(),
        configOptions: [],
        images: [],
        input: 'hello',
        pendingPrompt: false,
        queuedCount: 0,
        queuedPrompts: [],
        removeImage: jest.fn(),
        removeQueuedPrompt: jest.fn(),
        sendMessage: jest.fn(),
        setInput: jest.fn(),
        setMode: jest.fn(),
        setModel: jest.fn(),
        ...overrides,
    };
    mockUseChatState.mockReturnValue(state);
    const view = render(<Composer />);
    return { ...state, rerender: view.rerender };
};

afterEach(() => {
    jest.clearAllMocks();
});

test('grows with the draft up to eight lines and shrinks after clearing it', () => {
    const state = renderComposer({ input: '' });
    const textarea = screen.getByRole('textbox');
    expect(textarea).toHaveStyle({ height: '60px', overflowY: 'hidden' });

    let contentHeight = 100;
    Object.defineProperty(textarea, 'scrollHeight', { configurable: true, get: () => contentHeight });
    mockUseChatState.mockReturnValue({ ...state, input: 'line\n'.repeat(4) + 'line' });
    state.rerender(<Composer />);
    expect(textarea).toHaveStyle({ height: '100px', overflowY: 'hidden' });

    contentHeight = 200;
    const longDraft = 'line\n'.repeat(9) + 'line';
    mockUseChatState.mockReturnValue({ ...state, input: longDraft });
    state.rerender(<Composer />);
    expect(textarea).toHaveStyle({ height: '160px', overflowY: 'auto' });
    expect(textarea).toHaveValue(longDraft);

    contentHeight = 20;
    mockUseChatState.mockReturnValue({ ...state, input: '' });
    state.rerender(<Composer />);
    expect(textarea).toHaveStyle({ height: '60px', overflowY: 'hidden' });
});

test('shows a disabled loading picker while welcome configuration is being loaded', () => {
    renderComposer({ configOptionsLoading: true });

    expect(screen.getByRole('status')).toHaveTextContent('Loading...');
    expect(screen.getByRole('button', { name: 'Loading...' })).toBeDisabled();
});

test('does not keep a loading picker for an agent without configuration options', () => {
    renderComposer({ configOptionsLoading: false });

    expect(screen.queryByRole('status')).not.toBeInTheDocument();
});

test('does not send or prevent Enter while IME composition is active', () => {
    const state = renderComposer();
    const input = screen.getByRole('textbox');

    fireEvent.compositionStart(input);
    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(true);
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('sends on normal Enter when not composing', () => {
    const state = renderComposer();
    const input = screen.getByRole('textbox');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(false);
    expect(state.sendMessage).toHaveBeenCalledTimes(1);
});

test('keeps Shift Enter for textarea newline insertion', () => {
    const state = renderComposer();
    const input = screen.getByRole('textbox');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter', shiftKey: true });

    expect(defaultAllowed).toBe(true);
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('uses Enter to insert command completion without sending when not composing', () => {
    const state = renderComposer({
        input: '/hel',
        availableSkills: [{ name: 'help-me', description: 'Show help' }],
    });
    const input = screen.getByRole('textbox');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(false);
    expect(state.setInput).toHaveBeenCalledWith('/help-me ');
    expect(state.sendMessage).not.toHaveBeenCalled();
});

const commandOptions = {
    input: '/',
    availableSkills: [{ name: 'analyze-memory' }],
    availableCommands: [{ name: 'help' }, { name: 'status' }],
};

test.each(['Enter', 'Tab'])('uses arrow navigation and %s to insert the highlighted completion', (key) => {
    const state = renderComposer(commandOptions);
    const input = screen.getByRole('textbox');
    input.focus();
    expect(screen.getByRole('option', { name: 'analyze-memory' })).toHaveAttribute('aria-selected', 'true');
    expect(fireEvent.keyDown(input, { key: 'ArrowDown' })).toBe(false);
    const selected = screen.getByRole('option', { name: 'help' });
    expect(selected).toHaveClass('active');
    expect(input).toHaveAttribute('aria-activedescendant', selected.id);
    expect(input).toHaveFocus();
    fireEvent.keyDown(input, { key });
    expect(state.setInput).toHaveBeenCalledWith('/help ');
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('wraps arrow navigation at both ends and scrolls only the command menu', () => {
    renderComposer(commandOptions);
    const input = screen.getByRole('textbox');
    const menu = screen.getByRole('listbox');
    Object.defineProperty(menu, 'clientHeight', { value: 70 });
    screen.getAllByRole('option').forEach((option, index) => {
        Object.defineProperties(option, { offsetTop: { value: index * 50 }, offsetHeight: { value: 50 } });
    });
    fireEvent.keyDown(input, { key: 'ArrowUp' });
    expect(screen.getByRole('option', { name: 'status' })).toHaveAttribute('aria-selected', 'true');
    expect(menu.scrollTop).toBe(80);
    fireEvent.keyDown(input, { key: 'ArrowDown' });
    expect(screen.getByRole('option', { name: 'analyze-memory' })).toHaveAttribute('aria-selected', 'true');
    expect(menu.scrollTop).toBe(0);
    fireEvent.keyDown(input, { key: 'ArrowDown' });
    expect(menu.scrollTop).toBe(30);
});

test('resets the highlight when filtering or reopening the command menu', () => {
    const state = renderComposer(commandOptions);
    const input = screen.getByRole('textbox');
    fireEvent.keyDown(input, { key: 'ArrowUp' });
    mockUseChatState.mockReturnValue({ ...state, input: '/he' });
    state.rerender(<Composer />);
    expect(screen.getByRole('option', { name: 'help' })).toHaveAttribute('aria-selected', 'true');
    mockUseChatState.mockReturnValue({ ...state, input: '/help ' });
    state.rerender(<Composer />);
    expect(screen.queryByRole('listbox')).not.toBeInTheDocument();
    mockUseChatState.mockReturnValue({ ...state, input: '/' });
    state.rerender(<Composer />);
    expect(screen.getByRole('option', { name: 'analyze-memory' })).toHaveAttribute('aria-selected', 'true');
});

test('keeps mouse highlighting and keyboard confirmation in sync', () => {
    const state = renderComposer(commandOptions);
    fireEvent.mouseMove(screen.getByRole('option', { name: 'status' }));
    fireEvent.keyDown(screen.getByRole('textbox'), { key: 'Enter' });
    expect(state.setInput).toHaveBeenCalledWith('/status ');
});

test('leaves arrows and confirmation keys to IME while composing', () => {
    const state = renderComposer(commandOptions);
    const input = screen.getByRole('textbox');
    fireEvent.compositionStart(input);
    for (const key of ['ArrowDown', 'ArrowUp', 'Enter', 'Tab']) {
        expect(fireEvent.keyDown(input, { key })).toBe(true);
    }
    expect(screen.getByRole('option', { name: 'analyze-memory' })).toHaveAttribute('aria-selected', 'true');
    expect(state.setInput).not.toHaveBeenCalled();
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('preserves normal arrow keys when there are no command matches', () => {
    renderComposer({ ...commandOptions, input: '/not-found' });
    const input = screen.getByRole('textbox');
    expect(screen.queryByRole('listbox')).not.toBeInTheDocument();
    expect(fireEvent.keyDown(input, { key: 'ArrowDown' })).toBe(true);
    expect(fireEvent.keyDown(input, { key: 'ArrowUp' })).toBe(true);
});

test('shows the command placeholder', () => {
    renderComposer();

    expect(screen.getByPlaceholderText('Type / to use a command')).toBeVisible();
});

test.each(['', '   '])('shows the stop icon and cancels while running with empty input %j', (input) => {
    const state = renderComposer({ pendingPrompt: true, input });
    const cancelButton = screen.getByRole('button', { name: 'Cancel' });

    expect(cancelButton).toHaveClass('executing');
    expect(cancelButton.querySelector('img')).toHaveAttribute('src', 'stop.svg');
    fireEvent.click(cancelButton);

    expect(state.cancelMessage).toHaveBeenCalledTimes(1);
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test.each([
    { input: 'next question', images: [] },
    { input: '', images: [{ id: 'image-1', name: 'snapshot.png', mimeType: 'image/png', data: 'aW1hZ2U=' }] },
])('sends a draft instead of cancelling the running prompt: %j', (draft) => {
    const state = renderComposer({ pendingPrompt: true, ...draft });
    const sendButton = screen.getByRole('button', { name: 'Send' });
    expect(sendButton).toBeEnabled();
    expect(sendButton).not.toHaveClass('executing');
    expect(sendButton.querySelector('img[src="stop.svg"]')).not.toBeInTheDocument();
    fireEvent.click(sendButton);
    expect(state.sendMessage).toHaveBeenCalledTimes(1);
    expect(state.cancelMessage).not.toHaveBeenCalled();
});

test('expands, collapses, and removes queued prompts', () => {
    const state = renderComposer({
        queuedCount: 2,
        queuedPrompts: [
            { text: 'first queued prompt', images: [] },
            { text: 'second queued prompt', images: [] },
        ],
    });

    const toggle = screen.getByRole('button', { name: /2 Queued Message/ });
    expect(screen.getByText('first queued prompt')).toBeVisible();

    fireEvent.click(screen.getAllByRole('button', { name: 'Remove' })[0]);
    expect(state.removeQueuedPrompt).toHaveBeenCalledWith(0);

    fireEvent.click(toggle);
    expect(toggle).toHaveAttribute('aria-expanded', 'false');
    expect(toggle.closest('.queue-preview')).toHaveClass('collapsed');
    expect(screen.queryByText('first queued prompt')).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Clear All' }));
    expect(state.clearQueuedPrompts).toHaveBeenCalledTimes(1);
});

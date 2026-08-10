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
    render(<Composer />);
    return state;
};

afterEach(() => {
    jest.clearAllMocks();
});

test('does not send or prevent Enter while IME composition is active', () => {
    const state = renderComposer();
    const input = screen.getByPlaceholderText('Message claude');

    fireEvent.compositionStart(input);
    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(true);
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('sends on normal Enter when not composing', () => {
    const state = renderComposer();
    const input = screen.getByPlaceholderText('Message claude');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(false);
    expect(state.sendMessage).toHaveBeenCalledTimes(1);
});

test('keeps Shift Enter for textarea newline insertion', () => {
    const state = renderComposer();
    const input = screen.getByPlaceholderText('Message claude');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter', shiftKey: true });

    expect(defaultAllowed).toBe(true);
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('uses Enter to insert command completion without sending when not composing', () => {
    const state = renderComposer({
        input: '/hel',
        availableSkills: [{ name: 'help-me', description: 'Show help' }],
    });
    const input = screen.getByPlaceholderText('Message claude');

    const defaultAllowed = fireEvent.keyDown(input, { key: 'Enter' });

    expect(defaultAllowed).toBe(false);
    expect(state.setInput).toHaveBeenCalledWith('/help-me ');
    expect(state.sendMessage).not.toHaveBeenCalled();
});

test('shows the Enter and Shift Enter shortcut hint', () => {
    renderComposer();

    expect(screen.getByText('Enter to send · Shift+Enter for newline')).toBeVisible();
});

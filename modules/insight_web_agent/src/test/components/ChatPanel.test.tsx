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
import { act, fireEvent, render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { ChatPanel } from '../../components/ChatPanel';

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

jest.mock('../../components/Composer', () => ({ Composer: () => <div>Composer</div> }));
jest.mock('../../components/MessageList', () => ({
    MessageList: () => <div>Messages</div>,
    ModelSwitchNotice: ({ notice }: { notice: { model: string } }) => (
        <div role="status">{`Switched model: ${notice.model}`}</div>
    ),
}));
jest.mock('../../components/WelcomePanel', () => ({ WelcomePanel: () => <div>Welcome</div> }));

const mockUseChatState = useChatState as jest.Mock;

const chatState = {
    currentSessionId: 'session-1',
    isDraftSession: false,
    messages: [{ id: 'message-1', role: 'assistant', content: [] }],
    messagesRef: { current: null },
    notices: [],
    pendingPrompt: false,
    respondToPermission: jest.fn(),
    sessions: [{ sessionId: 'session-1', title: 'Analyze operator jitter' }],
};

const originalResizeObserver = globalThis.ResizeObserver;
let resize: () => void;
const disconnect = jest.fn();

beforeEach(() => {
    disconnect.mockClear();
    (globalThis as any).ResizeObserver = class {
        constructor(callback: () => void) { resize = callback; }
        observe = jest.fn();
        disconnect = disconnect;
    };
});

afterEach(() => {
    globalThis.ResizeObserver = originalResizeObserver;
});

const renderScrollablePanel = () => {
    let state = { ...chatState, pendingPrompt: true, scrollToLatestRequest: 0 };
    mockUseChatState.mockReturnValue(state);
    const view = render(<ChatPanel />);
    const container = view.container.querySelector('.messages') as HTMLElement;
    Object.defineProperties(container, {
        scrollHeight: { configurable: true, value: 1000, writable: true },
        clientHeight: { configurable: true, value: 400, writable: true },
    });
    act(() => resize());
    const update = (height: number, overrides: Partial<typeof state> = {}): void => {
        Object.defineProperty(container, 'scrollHeight', { value: height });
        state = { ...state, messages: [...state.messages], ...overrides };
        mockUseChatState.mockReturnValue(state);
        view.rerender(<ChatPanel />);
    };
    const scroll = (top: number): void => {
        container.scrollTop = top;
        fireEvent.scroll(container);
    };
    return { container, update, scroll, unmount: view.unmount };
};

test('follows streaming content while already at the bottom', () => {
    const { container, update } = renderScrollablePanel();
    expect(container.scrollTop).toBe(600);
    update(1400);
    expect(container.scrollTop).toBe(1000);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
});

test('pauses on upward wheel input before the scroll event and preserves the reading position', () => {
    const { container, update, scroll } = renderScrollablePanel();
    fireEvent.wheel(container, { deltaY: -20 });
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    update(1400);
    expect(container.scrollTop).toBe(600);
    expect(screen.getByRole('button', { name: 'View latest' })).toBeVisible();
    scroll(400);
    update(1800);
    expect(container.scrollTop).toBe(400);
});

test('upward scrolling pauses even within the bottom threshold and downward scrolling resumes near the bottom', () => {
    const { container, update, scroll } = renderScrollablePanel();
    scroll(580);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    update(1040);
    expect(container.scrollTop).toBe(580);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    Object.defineProperty(container, 'scrollHeight', { value: 1060 });
    act(() => resize());
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    Object.defineProperty(container, 'scrollHeight', { value: 1061 });
    act(() => resize());
    expect(screen.getByRole('button', { name: 'View latest' })).toBeVisible();
    update(1100);
    expect(container.scrollTop).toBe(580);
    scroll(640);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    update(1400);
    expect(container.scrollTop).toBe(1000);
});

test('view latest returns to the bottom and resumes following', () => {
    const { container, update, scroll } = renderScrollablePanel();
    scroll(200);
    update(1400);
    const button = screen.getByRole('button', { name: 'View latest' });
    expect(button.querySelectorAll('.latest-output-dot')).toHaveLength(3);
    expect(button.querySelector('svg')).not.toBeInTheDocument();
    update(1400, { pendingPrompt: false });
    expect(button.querySelector('.latest-output-dots')).not.toBeInTheDocument();
    expect(button.querySelector('svg')).toBeInTheDocument();
    expect(container.scrollTop).toBe(200);
    fireEvent.click(screen.getByRole('button', { name: 'View latest' }));
    expect(container.scrollTop).toBe(1000);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
    update(1800);
    expect(container.scrollTop).toBe(1400);
});

test('sending a message and switching sessions both restore following', () => {
    const { container, update, scroll } = renderScrollablePanel();
    scroll(200);
    update(1400, { scrollToLatestRequest: 1 });
    expect(container.scrollTop).toBe(1000);
    scroll(400);
    update(1800, { currentSessionId: 'session-2' });
    expect(container.scrollTop).toBe(1400);
    expect(screen.queryByRole('button', { name: 'View latest' })).not.toBeInTheDocument();
});

test('content and viewport resizing respect paused following and disconnect on unmount', () => {
    const { container, scroll, unmount } = renderScrollablePanel();
    Object.defineProperty(container, 'scrollHeight', { value: 1400 });
    act(() => resize());
    expect(container.scrollTop).toBe(1000);
    scroll(200);
    Object.defineProperty(container, 'scrollHeight', { value: 1800 });
    Object.defineProperty(container, 'clientHeight', { value: 300 });
    act(() => resize());
    expect(container.scrollTop).toBe(200);
    unmount();
    expect(disconnect).toHaveBeenCalled();
});

test('shows the active session title above the conversation', () => {
    mockUseChatState.mockReturnValue(chatState);
    render(<ChatPanel />);

    expect(screen.getByText('Analyze operator jitter')).toBeVisible();
});

test('does not show a session title for a new draft conversation', () => {
    mockUseChatState.mockReturnValue({ ...chatState, isDraftSession: true, messages: [] });
    render(<ChatPanel />);

    expect(screen.queryByText('Analyze operator jitter')).not.toBeInTheDocument();
    expect(screen.getByText('Welcome')).toBeVisible();
});

test('shows a model switch notice under the welcome panel after changing models', () => {
    mockUseChatState.mockReturnValue({
        ...chatState,
        isDraftSession: true,
        messages: [],
        notices: [{ id: 'notice-1', type: 'model_switch', model: 'GLM-4.7' }],
    });
    render(<ChatPanel />);

    expect(screen.getByText('Welcome')).toBeVisible();
    expect(screen.getByRole('status')).toHaveTextContent('Switched model: GLM-4.7');
});

/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { ChatPage } from '../../components/ChatPage';

jest.mock('../../hooks/useChatState', () => ({ useChatState: jest.fn() }));
jest.mock('../../components/ChatPanel', () => ({
    ChatPanel: () => <div>Chat panel<textarea aria-label="Message" /></div>,
}));
jest.mock('../../components/SessionSidebar', () => ({ SessionSidebar: () => <div>Session sidebar</div> }));

const mockUseChatState = useChatState as jest.Mock;

test('shows background knowledge loading without hiding the chat panel', () => {
    mockUseChatState.mockReturnValue({ ragStatus: { enabled: false, phase: 'initializing' } });
    const { rerender } = render(<ChatPage />);
    expect(screen.getByRole('status')).toHaveTextContent('The knowledge base is loading in the background');
    expect(screen.getByText('Chat panel')).toBeVisible();
    mockUseChatState.mockReturnValue({ ragStatus: { enabled: true, phase: 'ready' } });
    rerender(<ChatPage />);
    expect(screen.queryByRole('status')).not.toBeInTheDocument();
});

test('shows knowledge failure separately from backend connectivity', () => {
    mockUseChatState.mockReturnValue({ ragStatus: { enabled: false, phase: 'disabled', reason: 'pack_unreadable' } });
    render(<ChatPage />);
    expect(screen.getByRole('status')).toHaveTextContent('The knowledge base is unavailable');
    expect(screen.getByText('Chat panel')).toBeVisible();
});

test('does not show a failure for explicitly disabled knowledge retrieval', () => {
    mockUseChatState.mockReturnValue({ ragStatus: { enabled: false, phase: 'disabled', reason: 'disabled' } });
    render(<ChatPage />);
    expect(screen.queryByRole('status')).not.toBeInTheDocument();
});

test('preserves both startup loading and RAG status without losing the draft during their independent transitions', () => {
    mockUseChatState.mockReturnValue({
        agentDiscoveryLoading: true, switchingAgent: false,
        ragStatus: { enabled: false, phase: 'initializing' },
    });
    const view = render(<ChatPage />);
    const input = screen.getByRole('textbox', { name: 'Message' }) as HTMLTextAreaElement;
    input.value = 'draft';
    expect(view.container.querySelectorAll('.agent-loading-mask')).toHaveLength(1);
    expect(view.container.querySelector('.rag-status')).toHaveTextContent('The knowledge base is loading');

    mockUseChatState.mockReturnValue({
        agentDiscoveryLoading: false, switchingAgent: false,
        ragStatus: { enabled: false, phase: 'initializing' },
    });
    view.rerender(<ChatPage />);
    expect(view.container.querySelector('.agent-loading-mask')).not.toBeInTheDocument();
    expect(screen.getByRole('main')).toHaveAttribute('aria-busy', 'false');
    expect(screen.getByRole('status')).toHaveTextContent('The knowledge base is loading');
    expect(screen.getByRole('textbox', { name: 'Message' })).toBe(input);
    expect(input).toHaveValue('draft');

    mockUseChatState.mockReturnValue({
        agentDiscoveryLoading: false, switchingAgent: false,
        ragStatus: { enabled: true, phase: 'ready' },
    });
    view.rerender(<ChatPage />);
    expect(screen.queryByRole('status')).not.toBeInTheDocument();
    expect(screen.getByRole('textbox', { name: 'Message' })).toBe(input);
    expect(input).toHaveValue('draft');
});

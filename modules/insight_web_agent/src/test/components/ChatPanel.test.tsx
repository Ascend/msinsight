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
import { render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { useChatState } from '../../hooks/useChatState';
import { ChatPanel } from '../../components/ChatPanel';

jest.mock('../../hooks/useChatState', () => ({
    useChatState: jest.fn(),
}));

jest.mock('../../components/Composer', () => ({ Composer: () => <div>Composer</div> }));
jest.mock('../../components/MessageList', () => ({ MessageList: () => <div>Messages</div> }));
jest.mock('../../components/WelcomePanel', () => ({ WelcomePanel: () => <div>Welcome</div> }));

const mockUseChatState = useChatState as jest.Mock;

const chatState = {
    currentSessionId: 'session-1',
    isDraftSession: false,
    messages: [{ id: 'message-1', role: 'assistant', content: [] }],
    messagesRef: { current: null },
    pendingPrompt: false,
    respondToPermission: jest.fn(),
    sessions: [{ sessionId: 'session-1', title: 'Analyze operator jitter' }],
};

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

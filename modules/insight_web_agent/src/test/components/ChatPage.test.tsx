/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { ThemeProvider } from '@emotion/react';
import { render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { ChatPage } from '../../components/ChatPage';
import { useChatState } from '../../hooks/useChatState';

jest.mock('../../hooks/useChatState', () => ({ useChatState: jest.fn() }));
jest.mock('react-i18next', () => ({ useTranslation: () => ({ t: (key: string) => key }) }));
jest.mock('../../components/SessionSidebar', () => ({
    SessionSidebar: () => <header>Agent toolbar</header>,
}));
jest.mock('../../components/ChatPanel', () => ({
    ChatPanel: () => <section><h1>Welcome</h1><textarea aria-label="Message" /></section>,
}));

const mockUseChatState = useChatState as jest.Mock;
const page = <ThemeProvider theme={{
    bgColor: '#ffffff',
    bgColorLight: '#f5f5f5',
    textColorPrimary: '#222222',
    borderColor: '#cccccc',
    borderRadiusLarge: '8px',
    primaryColor: '#007aff',
}}><ChatPage /></ThemeProvider>;

test('shows the loaded Agent page underneath a translucent startup loading mask', () => {
    mockUseChatState.mockReturnValue({ agentDiscoveryLoading: true, switchingAgent: false });
    render(page);

    expect(screen.getByText('Agent toolbar')).toBeVisible();
    expect(screen.getByRole('heading', { name: 'Welcome' })).toBeVisible();
    expect(screen.getByRole('textbox', { name: 'Message' })).toBeVisible();
    expect(screen.getByRole('main')).toHaveAttribute('aria-busy', 'true');
    expect(screen.getByRole('status')).toHaveStyle({ backgroundColor: 'rgba(245, 245, 245, 0.8)' });
    expect(screen.getByRole('status')).toHaveTextContent('loading');
});

test('removes loading without remounting the Agent page or discarding its draft', () => {
    mockUseChatState.mockReturnValue({ agentDiscoveryLoading: true, switchingAgent: false });
    const view = render(page);
    const input = screen.getByRole('textbox', { name: 'Message' }) as HTMLTextAreaElement;
    input.value = 'draft';

    mockUseChatState.mockReturnValue({ agentDiscoveryLoading: false, switchingAgent: false });
    view.rerender(<ThemeProvider theme={{ bgColor: '#ffffff' }}><ChatPage /></ThemeProvider>);

    expect(screen.queryByRole('status')).not.toBeInTheDocument();
    expect(screen.getByRole('main')).toHaveAttribute('aria-busy', 'false');
    expect(screen.getByRole('textbox', { name: 'Message' })).toBe(input);
    expect(input).toHaveValue('draft');
});

test('uses the same mask style with agent-switching text', () => {
    mockUseChatState.mockReturnValue({ agentDiscoveryLoading: false, switchingAgent: true });
    render(page);

    expect(screen.getByRole('status')).toHaveStyle({ backgroundColor: 'rgba(245, 245, 245, 0.8)' });
    expect(screen.getByRole('status')).toHaveTextContent('switchingAgent');
    expect(screen.getByRole('main')).toHaveAttribute('aria-busy', 'true');
});

test('renders only one mask when startup and switching overlap', () => {
    mockUseChatState.mockReturnValue({ agentDiscoveryLoading: true, switchingAgent: true });
    render(page);

    expect(screen.getAllByRole('status')).toHaveLength(1);
    expect(screen.getByRole('status')).toHaveTextContent('loading');
});

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
// eslint-disable-next-line import/named
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import '@testing-library/jest-dom';
import React, { createRef } from 'react';
import { SessionHistoryPopover } from '../../components/SessionHistoryPopover';

const sessions = [
    { sessionId: 'session-1', title: 'Analyze operator jitter', updatedAt: '1m' },
    { sessionId: 'session-2', title: 'Generate Python error handling', updatedAt: '2d' },
];

const renderPopover = (overrides: Partial<React.ComponentProps<typeof SessionHistoryPopover>> = {}): {
    onClose: jest.Mock;
    onDelete: jest.Mock;
    onAttentionChange: jest.Mock;
    onSelect: jest.Mock;
} => {
    const anchorRef = createRef<HTMLButtonElement>();
    const onClose = jest.fn();
    const onDelete = jest.fn();
    const onAttentionChange = jest.fn();
    const onSelect = jest.fn();
    render(<>
        <button ref={anchorRef} type="button">History trigger</button>
        <SessionHistoryPopover
            anchorRef={anchorRef}
            currentSessionId="session-1"
            onClose={onClose}
            onDelete={onDelete}
            onAttentionChange={onAttentionChange}
            onSelect={onSelect}
            open
            sessions={sessions}
            {...overrides}
        />
    </>);
    return { onAttentionChange, onClose, onDelete, onSelect };
};

test('filters sessions and selects a matching conversation', async () => {
    const { onClose, onSelect } = renderPopover();

    expect(await screen.findByRole('dialog', { name: 'Chat history' })).toBeVisible();
    fireEvent.change(screen.getByRole('searchbox', { name: 'Search chat history' }), { target: { value: 'Python' } });

    expect(screen.queryByText('Analyze operator jitter')).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Generate Python error handling' }));
    expect(onSelect).toHaveBeenCalledWith(sessions[1]);
    expect(onClose).toHaveBeenCalledTimes(1);
});

test('deletes a conversation without selecting it', async () => {
    const { onDelete, onSelect } = renderPopover();

    fireEvent.click(await screen.findByRole('button', { name: 'Delete conversation: Analyze operator jitter' }));
    expect(onDelete).toHaveBeenCalledWith(sessions[0]);
    expect(onSelect).not.toHaveBeenCalled();
});

test('closes when Escape is pressed', async () => {
    const { onClose } = renderPopover();

    await screen.findByRole('dialog', { name: 'Chat history' });
    fireEvent.keyDown(document, { key: 'Escape' });
    expect(onClose).toHaveBeenCalledTimes(1);
});

test('marks a running session with the loading state', async () => {
    const { onAttentionChange } = renderPopover({
        sessions: [{ ...sessions[0], pendingPrompt: true, status: 'working' }],
    });

    const sessionButton = await screen.findByRole('button', { name: 'Analyze operator jitter' });
    expect(sessionButton.parentElement).toHaveClass('working');
    await waitFor(() => expect(onAttentionChange).toHaveBeenLastCalledWith(true));
});

test('marks a newly completed background session as unread', async () => {
    const anchorRef = createRef<HTMLButtonElement>();
    const props = {
        anchorRef,
        currentSessionId: 'session-2',
        onClose: jest.fn(),
        onDelete: jest.fn(),
        onSelect: jest.fn(),
        open: true,
    };
    const { rerender } = render(<>
        <button ref={anchorRef} type="button">History trigger</button>
        <SessionHistoryPopover {...props} sessions={[{ ...sessions[0], status: 'working' }]} />
    </>);

    rerender(<>
        <button ref={anchorRef} type="button">History trigger</button>
        <SessionHistoryPopover {...props} sessions={[{ ...sessions[0], status: 'completed' }]} />
    </>);

    const sessionButton = await screen.findByRole('button', { name: 'Analyze operator jitter' });
    expect(sessionButton.parentElement).toHaveClass('unread');
    expect(screen.getByText('Completed')).toBeVisible();
});

test('shows time and no unread dot for the selected completed session', async () => {
    renderPopover({
        sessions: [{ ...sessions[0], status: 'completed' }],
    });

    const sessionButton = await screen.findByRole('button', { name: 'Analyze operator jitter' });
    expect(sessionButton.parentElement).not.toHaveClass('unread');
    expect(screen.getByText('1m')).toBeVisible();
    expect(screen.queryByText('Completed')).not.toBeInTheDocument();
});

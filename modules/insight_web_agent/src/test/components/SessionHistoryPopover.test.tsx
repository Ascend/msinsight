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
    onExport: jest.Mock;
    onAttentionChange: jest.Mock;
    onSelect: jest.Mock;
} => {
    const anchorRef = createRef<HTMLButtonElement>();
    const onClose = jest.fn();
    const onDelete = jest.fn();
    const onExport = jest.fn();
    const onAttentionChange = jest.fn();
    const onSelect = jest.fn();
    render(<>
        <button ref={anchorRef} type="button">History trigger</button>
        <SessionHistoryPopover
            anchorRef={anchorRef}
            currentSessionId="session-1"
            onClose={onClose}
            onDelete={onDelete}
            onExport={onExport}
            onAttentionChange={onAttentionChange}
            onSelect={onSelect}
            open
            sessions={sessions}
            {...overrides}
        />
    </>);
    return { onAttentionChange, onClose, onDelete, onExport, onSelect };
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

test('exports a conversation without selecting it', async () => {
    const { onExport, onSelect } = renderPopover();

    fireEvent.click(await screen.findByRole('button', { name: 'Export conversation: Analyze operator jitter' }));
    expect(onExport).toHaveBeenCalledWith(sessions[0]);
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
        onExport: jest.fn(),
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

test('title search supports case-insensitive separated characters and literal punctuation', async () => {
    renderPopover({ sessions: [
        { sessionId: 'deepseek', title: 'DeepSeek Memory' },
        { sessionId: 'reverse', title: 'sd' },
        { sessionId: 'literal', title: 'Result [A+B]' },
    ] });
    fireEvent.change(screen.getByRole('searchbox'), { target: { value: 'DS' } });
    expect(screen.getByRole('button', { name: 'DeepSeek Memory' })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'sd' })).not.toBeInTheDocument();
    fireEvent.change(screen.getByRole('searchbox'), { target: { value: '[A+B]' } });
    expect(screen.getByRole('button', { name: 'Result [A+B]' })).toBeInTheDocument();
});

test('renames without selecting or closing the conversation and trims the saved title', async () => {
    const onRename = jest.fn().mockResolvedValue(undefined);
    const { onSelect, onClose } = renderPopover({ onRename });
    fireEvent.click(screen.getByRole('button', { name: 'Rename Analyze operator jitter' }));
    fireEvent.change(screen.getByRole('textbox', { name: 'Session title' }), { target: { value: ' DeepSeek investigation ' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(onRename).toHaveBeenCalledWith(sessions[0], 'DeepSeek investigation'));
    await waitFor(() => expect(screen.queryByRole('textbox', { name: 'Session title' })).not.toBeInTheDocument());
    expect(onSelect).not.toHaveBeenCalled();
    expect(onClose).not.toHaveBeenCalled();
});

test('rejects blank/long titles, cancels with Escape and does not submit IME Enter', async () => {
    const onRename = jest.fn().mockResolvedValue(undefined);
    const { onClose } = renderPopover({ onRename });
    fireEvent.click(screen.getByRole('button', { name: 'Rename Analyze operator jitter' }));
    const input = screen.getByRole('textbox', { name: 'Session title' });
    for (const title of ['  ', 'x'.repeat(201)]) {
        fireEvent.change(input, { target: { value: title } });
        expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    }
    fireEvent.change(input, { target: { value: 'Valid title' } });
    fireEvent.keyDown(input, { key: 'Enter', isComposing: true });
    expect(onRename).not.toHaveBeenCalled();
    fireEvent.keyDown(input, { key: 'Escape' });
    expect(screen.queryByRole('textbox', { name: 'Session title' })).not.toBeInTheDocument();
    expect(onClose).not.toHaveBeenCalled();
});

test('preserves the edit and shows save failure instead of pretending the rename succeeded', async () => {
    const onRename = jest.fn().mockRejectedValue(new Error('Could not save title'));
    renderPopover({ onRename });
    fireEvent.click(screen.getByRole('button', { name: 'Rename Analyze operator jitter' }));
    fireEvent.change(screen.getByRole('textbox', { name: 'Session title' }), { target: { value: 'Retry this title' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByRole('alert')).toHaveTextContent('Could not save title');
    expect(screen.getByRole('textbox', { name: 'Session title' })).toHaveValue('Retry this title');
    expect(screen.getByRole('button', { name: 'Analyze operator jitter' })).toBeInTheDocument();
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

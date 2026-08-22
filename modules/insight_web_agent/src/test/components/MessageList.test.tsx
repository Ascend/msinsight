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
import userEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';

const mockExecuteFrontendCommand = jest.fn();

jest.mock('../../bridge/frontendAgentCommandTransport', () => ({
    executeFrontendCommand: (...args: unknown[]) => mockExecuteFrontendCommand(...args),
}));

jest.mock('react-markdown', () => ({
    __esModule: true,
    default: ({ children }: { children: string }) => <MockMarkdown text={children} />,
}));

jest.mock('remark-gfm', () => ({
    __esModule: true,
    default: jest.fn(),
}));

import { MessageList, toolCallDisplayName } from '../../components/MessageList';

const noopPermissionDecision = jest.fn();
const actionXml = (blockId: number): string => `<insight-action>
{
  "label": "Block #${blockId}",
  "description": "Highlight block #${blockId}.",
  "command": "MemScope.lifecycleGraph.selectBlock",
  "args": { "blockId": ${blockId} }
}
</insight-action>`;

beforeEach(() => {
    mockExecuteFrontendCommand.mockReset();
});

const MockMarkdown = ({ text }: { text: string }): JSX.Element => {
    if (text.includes('```')) {
        return <>
            <pre><code>const value = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";</code></pre>
            <table><tbody><tr><th>HeaderHeaderHeaderHeader</th><th>OtherHeaderHeaderHeader</th></tr></tbody></table>
        </>;
    }
    return <p>A very long token abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz and <code>/workspace/really/long/path/that/should/wrap/in/the/panel/file.ts</code></p>;
};

test('shows the msinsight command as the tool card name', () => {
    expect(toolCallDisplayName({
        toolCallId: 'call-1',
        name: 'msinsight',
        status: 'in_progress',
        input: '{"command":"MemScope.table.getDisplayedData","args":{"targetId":"table-1"}}',
    })).toBe('MemScope.table.getDisplayedData');
    expect(toolCallDisplayName({
        toolCallId: 'call-2',
        name: 'msinsight',
        status: 'in_progress',
        input: 'invalid JSON',
    })).toBe('msinsight');
    expect(toolCallDisplayName({
        toolCallId: 'call-3',
        name: 'other-tool',
        status: 'in_progress',
        input: '{"command":"observe"}',
    })).toBe('other-tool');
});

test('renders assistant content blocks in text-tool-text order', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-ordered',
            role: 'assistant',
            content: [
                { id: 'text-before', type: 'text', text: 'before tool' },
                { id: 'call-1', type: 'tool', toolCall: { toolCallId: 'call-1', name: 'Read', status: 'completed', output: 'done' } },
                { id: 'text-after', type: 'text', text: 'after tool' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const message = document.querySelector('.message') as HTMLElement;
    expect(Array.from(message.children).slice(0, 3).map((node) => node.className)).toEqual(['rich-text', 'tool-calls', 'rich-text']);
});

test('renders a valid action from assistant XML text', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-actions',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: `Found a block.\n${actionXml(123)}` }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.getByRole('button', { name: 'Block #123' })).toBeInTheDocument();
});

test('does not turn user-authored XML into an action', () => {
    render(<MessageList
        messages={[{
            id: 'user-actions',
            role: 'user',
            content: [{ id: 'text-1', type: 'text', text: actionXml(123) }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.queryByRole('button', { name: 'Block #123' })).not.toBeInTheDocument();
});

test('preflights an action and executes it only after approval', async () => {
    mockExecuteFrontendCommand
        .mockResolvedValueOnce({ command: { name: 'MemScope.lifecycleGraph.selectBlock', title: 'Select memory block', description: 'Select one block in the lifecycle graph.', inputSchema: { type: 'object' } } })
        .mockResolvedValueOnce({ accepted: true });
    render(<MessageList
        messages={[{
            id: 'assistant-actions',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: actionXml(123) }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    await userEvent.click(screen.getByRole('button', { name: 'Block #123' }));

    expect(mockExecuteFrontendCommand).toHaveBeenCalledTimes(1);
    expect(mockExecuteFrontendCommand.mock.calls[0][0]).toBe('help');
    expect(mockExecuteFrontendCommand.mock.calls[0][1]).toEqual({ command: 'MemScope.lifecycleGraph.selectBlock' });
    expect(await screen.findByText('Select memory block')).toBeInTheDocument();
    expect(screen.getByText('Highlight block #123.')).toBeInTheDocument();
    expect(screen.getByText(/"blockId": 123/)).toBeInTheDocument();

    await userEvent.click(screen.getByRole('button', { name: 'Approve and run' }));

    expect(mockExecuteFrontendCommand).toHaveBeenCalledTimes(2);
    expect(mockExecuteFrontendCommand.mock.calls[1][0]).toBe('MemScope.lifecycleGraph.selectBlock');
    expect(mockExecuteFrontendCommand.mock.calls[1][1]).toEqual({ blockId: 123 });
    expect(await screen.findByText(/Command completed.*accepted.*true/)).toBeInTheDocument();
});

test('hides an unclosed action while streaming and shows it literally after completion', () => {
    const source = 'Before<insight-action>{"label":"Block';
    const { rerender } = render(<MessageList
        messages={[{ id: 'assistant-actions', role: 'assistant', content: [{ id: 'text-1', type: 'text', text: source }] }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(screen.getByText(/Before/)).toBeInTheDocument();
    expect(screen.queryByText(/insight-action/)).not.toBeInTheDocument();

    rerender(<MessageList
        messages={[{ id: 'assistant-actions', role: 'assistant', content: [{ id: 'text-1', type: 'text', text: source }] }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.getByText(/<insight-action>/)).toBeInTheDocument();
});

test('bounds a string command result in the local Action UI', async () => {
    mockExecuteFrontendCommand
        .mockResolvedValueOnce({ command: { name: 'observe', title: 'Observe page', description: 'Observe the current page.', inputSchema: { type: 'object' } } })
        .mockResolvedValueOnce('x'.repeat(600));
    render(<MessageList
        messages={[{ id: 'assistant-actions', role: 'assistant', content: [{ id: 'text-1', type: 'text', text: '<insight-action>{"label":"Observe","description":"Observe the page.","command":"observe","args":{}}</insight-action>' }] }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    await userEvent.click(screen.getByRole('button', { name: 'Observe' }));
    await userEvent.click(screen.getByRole('button', { name: 'Approve and run' }));

    const result = await screen.findByText(/Command completed/);
    expect(result.textContent?.endsWith('…')).toBe(true);
    expect(result.textContent?.length).toBeLessThan(600);
});

test('allows multiple action approval cards to remain open', async () => {
    mockExecuteFrontendCommand.mockImplementation(async (_command: string, args: Record<string, unknown>) => ({
        command: {
            name: args.command,
            title: String(args.command),
            description: 'Trusted capability.',
            inputSchema: { type: 'object' },
        },
    }));
    render(<MessageList
        messages={[{
            id: 'assistant-actions',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: `${actionXml(1)}\n${actionXml(2)}` }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    await userEvent.click(screen.getByRole('button', { name: 'Block #1' }));
    await userEvent.click(screen.getByRole('button', { name: 'Block #2' }));

    expect(screen.getAllByText('Confirm page action')).toHaveLength(2);
});

test('cancel closes approval without executing the target command', async () => {
    mockExecuteFrontendCommand.mockResolvedValue({ command: { name: 'observe', title: 'Observe page', description: 'Observe the current page.', inputSchema: { type: 'object' } } });
    render(<MessageList
        messages={[{
            id: 'assistant-actions',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: '<insight-action>{"label":"Observe","description":"Observe the page.","command":"observe","args":{}}</insight-action>' }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    await userEvent.click(screen.getByRole('button', { name: 'Observe' }));
    await userEvent.click(screen.getByRole('button', { name: 'Cancel' }));

    expect(mockExecuteFrontendCommand).toHaveBeenCalledTimes(1);
    expect(screen.queryByText('Confirm page action')).not.toBeInTheDocument();
});

test('wraps long markdown text and inline code inside the message width', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-long-text',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: 'A very long token abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz and `/workspace/really/long/path/that/should/wrap/in/the/panel/file.ts`' }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const message = screen.getByText(/A very long token/).closest('.message') as HTMLElement;
    const richText = message.querySelector('.rich-text') as HTMLElement;
    const paragraph = richText.querySelector('p') as HTMLElement;
    const inlineCode = richText.querySelector('code') as HTMLElement;

    expect(getComputedStyle(message).minWidth).toMatch(/^0(px)?$/);
    expect(getComputedStyle(richText).minWidth).toMatch(/^0(px)?$/);
    expect(getComputedStyle(richText).maxWidth).toBe('100%');
    expect(getComputedStyle(paragraph).overflowWrap).toBe('anywhere');
    expect(getComputedStyle(inlineCode).whiteSpace).toBe('normal');
    expect(getComputedStyle(inlineCode).overflowWrap).toBe('anywhere');
});

test('keeps user prompts sticky and expands overflowing content', async () => {
    const onOuterWheel = jest.fn();
    render(<div onWheel={onOuterWheel}>
        <MessageList
            messages={[{
                id: 'user-long-prompt',
                role: 'user',
                content: [{ id: 'text-1', type: 'text', text: 'A long user prompt' }],
            }]}
            onPermissionDecision={noopPermissionDecision}
            pendingPrompt={false}
        />
    </div>);

    const message = document.querySelector('.message.user') as HTMLElement;
    const content = message.querySelector('.user-prompt-content') as HTMLElement;
    Object.defineProperty(content, 'scrollHeight', { configurable: true, value: 120 });
    fireEvent(window, new Event('resize'));

    expect(getComputedStyle(message.closest('.user-prompt-sticky') as HTMLElement).position).toBe('sticky');
    expect(message).toHaveClass('overflowing');

    await userEvent.click(screen.getByRole('button', { name: 'Expand' }));
    expect(message).toHaveClass('expanded');
    expect(screen.getByRole('button', { name: 'Collapse' })).toBeEnabled();

    Object.defineProperty(content, 'clientHeight', { configurable: true, value: 50 });
    Object.defineProperty(content, 'scrollTop', { configurable: true, writable: true, value: 20 });
    fireEvent.wheel(content, { deltaY: 10 });
    expect(onOuterWheel).not.toHaveBeenCalled();

    content.scrollTop = 70;
    fireEvent.wheel(content, { deltaY: 10 });
    expect(onOuterWheel).toHaveBeenCalledTimes(1);

    await userEvent.click(screen.getByRole('button', { name: 'Collapse' }));
    expect(content.scrollTop).toBe(0);
    expect(message).not.toHaveClass('expanded');
});

test('bounds each sticky prompt to its own conversation turn', () => {
    render(<MessageList
        messages={[
            { id: 'user-1', role: 'user', content: [{ id: 'user-text-1', type: 'text', text: 'Long first prompt' }] },
            { id: 'assistant-1', role: 'assistant', content: [{ id: 'answer-1', type: 'text', text: 'First answer' }] },
            { id: 'user-2', role: 'user', content: [{ id: 'user-text-2', type: 'text', text: 'Short second prompt' }] },
            { id: 'assistant-2', role: 'assistant', content: [{ id: 'answer-2', type: 'text', text: 'Second answer' }] },
        ]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const turns = document.querySelectorAll('.message-turn');
    expect(turns).toHaveLength(2);
    expect(turns[0].querySelector('.message.user')).toBeInTheDocument();
    expect(turns[0].querySelector('.message.assistant')).toBeInTheDocument();
    expect(turns[1].querySelector('.message.user')).toBeInTheDocument();
});

test('shows completed thinking time before the assistant answer', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-with-duration',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: 'Answer text' }],
            startedAt: 1000,
            durationMs: 126000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const message = document.querySelector('.message.assistant') as HTMLElement;
    const duration = screen.getByText('Elapsed 126s');
    const richText = message.querySelector('.rich-text') as HTMLElement;

    expect(message.firstElementChild).toBe(duration);
    expect(duration.compareDocumentPosition(richText) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
    expect(getComputedStyle(message).borderTopWidth).toBe('0px');
    expect(getComputedStyle(message).backgroundColor).toBe('transparent');
});

test('updates the elapsed time while the assistant is thinking', () => {
    jest.useFakeTimers();
    jest.setSystemTime(5000);

    render(<MessageList
        messages={[{
            id: 'assistant-thinking',
            role: 'assistant',
            content: [],
            startedAt: 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(screen.getByText('Elapsed 4.0s')).toBeInTheDocument();

    act(() => jest.advanceTimersByTime(1000));
    expect(screen.getByText('Elapsed 5.0s')).toBeInTheDocument();

    jest.useRealTimers();
});

test('hides the thinking indicator once answer text starts streaming', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-streaming-answer',
            role: 'assistant',
            content: [{ id: 'answer-1', type: 'text', text: 'Partial answer' }],
            startedAt: Date.now() - 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(screen.queryByText('Thinking...')).not.toBeInTheDocument();
    expect(screen.getByText('Elapsed 1.0s')).toBeInTheDocument();
});

test('shows historical thinking content collapsed by default', async () => {
    render(<MessageList
        messages={[{
            id: 'assistant-with-thinking',
            role: 'assistant',
            content: [
                { id: 'thinking-1', type: 'thinking', text: 'Historical reasoning' },
                { id: 'text-1', type: 'text', text: 'Answer text' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const details = screen.getByText('Thinking process').closest('details') as HTMLDetailsElement;
    expect(details).not.toHaveAttribute('open');
    expect(details.querySelector('.thinking-content')).toBeInTheDocument();

    await userEvent.click(screen.getByText('Thinking process'));
    expect(details).toHaveAttribute('open');
});

test('uses the elapsed time as the thinking details toggle when duration is available', async () => {
    render(<MessageList
        messages={[{
            id: 'assistant-with-duration-and-thinking',
            role: 'assistant',
            content: [
                { id: 'thinking-1', type: 'thinking', text: 'Reasoning details' },
                { id: 'text-1', type: 'text', text: 'Answer text' },
            ],
            startedAt: 1000,
            durationMs: 5000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.queryByText('Thinking process')).not.toBeInTheDocument();
    const details = screen.getByText('Elapsed 5.0s').closest('details') as HTMLDetailsElement;
    expect(details.querySelector('.thinking-chevron')).toBeInTheDocument();
    expect(details).not.toHaveAttribute('open');

    await userEvent.click(screen.getByText('Elapsed 5.0s'));
    expect(details).toHaveAttribute('open');
});

test('keeps wide code blocks and tables horizontally scrollable inside their own blocks', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-wide-blocks',
            role: 'assistant',
            content: [{ id: 'text-1', type: 'text', text: '```ts\nconst value = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";\n```\n\n| HeaderHeaderHeaderHeader | OtherHeaderHeaderHeader |\n| --- | --- |\n| CellCellCellCellCell | OtherCellCellCellCell |' }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const pre = document.querySelector('.rich-text pre') as HTMLElement;
    const preCode = pre.querySelector('code') as HTMLElement;
    const table = document.querySelector('.rich-text table') as HTMLElement;

    expect(getComputedStyle(pre).maxWidth).toBe('100%');
    expect(getComputedStyle(pre).overflowX).toBe('auto');
    expect(getComputedStyle(preCode).whiteSpace).toBe('pre');
    expect(getComputedStyle(table).maxWidth).toBe('100%');
    expect(getComputedStyle(table).overflowX).toBe('auto');
});

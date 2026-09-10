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
import { ThemeProvider } from '@emotion/react';
import { act, fireEvent, render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import '@testing-library/jest-dom';
import { themeInstance } from '@insight/lib/theme';

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
    expect(Array.from(message.children).slice(0, 3).map((node) => node.className)).toEqual(['rich-text', 'thinking-timeline answer-meta-details', 'rich-text']);
});

test('keeps thinking and tool calls in their original execution order', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-interleaved',
            role: 'assistant',
            content: [
                { id: 'thinking-1', type: 'thinking', text: 'Plan the lookup.' },
                { id: 'call-1', type: 'tool', toolCall: { toolCallId: 'call-1', name: 'rag_retrieve', status: 'completed', output: 'first result' } },
                { id: 'thinking-2', type: 'thinking', text: 'Use the first result.' },
                { id: 'call-2', type: 'tool', toolCall: { toolCallId: 'call-2', name: 'rag_retrieve', status: 'completed', output: 'second result' } },
                { id: 'text-1', type: 'text', text: 'Final answer.' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const message = document.querySelector('.message') as HTMLElement;
    expect(Array.from(message.children).map((node) => node.className)).toEqual(['thinking-timeline answer-meta-details', 'rich-text']);
    expect(message.querySelectorAll('.timeline-item')).toHaveLength(5);
    expect(message.querySelectorAll('.timeline-item.completed')).toHaveLength(4);
    expect(Array.from(message.querySelectorAll('.timeline-title')).map((node) => node.textContent)).toEqual([
        'Thought completed',
        'rag_retrieve completed',
        'Thought completed',
        'rag_retrieve completed',
        'Thought completed, starting answer:',
    ]);
});

test('shows per-step durations when thinking timestamps and tool durations are available', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-timed-timeline',
            role: 'assistant',
            startedAt: 1000,
            durationMs: 13000,
            content: [
                { id: 'thinking-1', type: 'thinking', text: 'Plan', startedAt: 1000 },
                { id: 'call-1', type: 'tool', toolCall: { toolCallId: 'call-1', name: 'Read', status: 'completed', startedAt: 3000, durationMs: 5000, output: 'done' } },
                { id: 'thinking-2', type: 'thinking', text: 'Answer', startedAt: 8000 },
                { id: 'text-1', type: 'text', text: 'Final answer.' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(Array.from(document.querySelectorAll('.timeline-duration')).map((node) => node.textContent)).toEqual(['2.0s', '5.0s', '6.0s']);
});

test('keeps tool call output collapsed until the output row is expanded', async () => {
    render(<MessageList
        messages={[{
            id: 'assistant-collapsed-tool',
            role: 'assistant',
            content: [
                {
                    id: 'call-1',
                    type: 'tool',
                    toolCall: {
                        toolCallId: 'call-1',
                        name: 'Read',
                        status: 'completed',
                        input: '{"path":"/tmp/example.ts"}',
                        output: 'file contents',
                    },
                },
                { id: 'text-1', type: 'text', text: 'Final answer.' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const timeline = document.querySelector('.thinking-timeline') as HTMLDetailsElement;
    await userEvent.click(timeline.querySelector('summary') as HTMLElement);

    const toolItem = timeline.querySelector('.timeline-item.tool') as HTMLElement;
    const toolDetails = toolItem.querySelector('.timeline-tool-details') as HTMLDetailsElement;
    expect(toolDetails.querySelector('.timeline-tool-target')).toHaveTextContent('/tmp/example.ts');
    expect(toolDetails).not.toHaveAttribute('open');
    expect(Array.from(toolDetails.querySelectorAll('.tool-section')).map((node) => node.textContent)).toEqual([
        'Input{"path":"/tmp/example.ts"}',
        'Outputfile contents',
    ]);

    await userEvent.click(toolDetails.querySelector('.timeline-tool-target') as HTMLElement);
    expect(toolDetails).toHaveAttribute('open');
    expect(timeline).toHaveAttribute('open');
});

test('marks a failed tool call with a red timeline marker', async () => {
    render(<MessageList
        messages={[{
            id: 'assistant-failed-tool',
            role: 'assistant',
            content: [{
                id: 'call-1',
                type: 'tool',
                toolCall: { toolCallId: 'call-1', name: 'Read', status: 'failed', output: 'not found' },
            }],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    const toolItem = document.querySelector('.timeline-item.tool') as HTMLElement;
    expect(toolItem).toHaveClass('failed');
    expect(getComputedStyle(toolItem.querySelector('.timeline-marker') as HTMLElement).backgroundColor).not.toBe('rgba(191, 191, 191, 1)');
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

test('renders generic Tool permission details and only Agent-provided actions', () => {
    render(<MessageList
        messages={[{
            id: 'permission:tool-1',
            role: 'assistant',
            content: [],
            permission: {
                sessionId: 'session-1',
                requestId: 'tool-1',
                kind: 'tool',
                target: 'msinsight-capabilities_msinsight',
                details: { input: { command: 'observe' } },
                actions: ['allow_once', 'deny'],
                state: 'pending',
            },
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.getByText('Allow tool use?')).toBeInTheDocument();
    expect(screen.getByText('msinsight-capabilities_msinsight')).toBeInTheDocument();
    expect(screen.getByText(/"command": "observe"/)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Allow once' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Deny' })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Allow for this session' })).not.toBeInTheDocument();
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

test('styles sent user messages from the theme token and keeps a square top-right corner', () => {
    const { rerender } = render(<ThemeProvider theme={themeInstance.getTheme().light}>
        <MessageList
            messages={[{
                id: 'user-style',
                role: 'user',
                content: [{ id: 'text-1', type: 'text', text: 'Hello' }],
            }]}
            onPermissionDecision={noopPermissionDecision}
            pendingPrompt={false}
        />
    </ThemeProvider>);

    const message = document.querySelector('.message.user') as HTMLElement;
    expect(getComputedStyle(message).backgroundColor).toBe('rgb(237, 243, 254)');
    expect(getComputedStyle(message).borderRadius).toBe('16px 0 16px 16px');

    rerender(<ThemeProvider theme={themeInstance.getTheme().dark}>
        <MessageList
            messages={[{
                id: 'user-style',
                role: 'user',
                content: [{ id: 'text-1', type: 'text', text: 'Hello' }],
            }]}
            onPermissionDecision={noopPermissionDecision}
            pendingPrompt={false}
        />
    </ThemeProvider>);

    expect(getComputedStyle(message).backgroundColor).toBe('rgb(33, 61, 91)');
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
    const duration = screen.getByText('Thought completed 2m06s');
    const richText = message.querySelector('.rich-text') as HTMLElement;

    expect(message.firstElementChild).toHaveClass('thinking-summary');
    expect(message.firstElementChild).toContainElement(duration);
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

    expect(screen.getByText('Thinking 4.0s')).toBeInTheDocument();
    expect(document.querySelector('.thinking-sparkle')).toBeInTheDocument();

    act(() => jest.advanceTimersByTime(1000));
    expect(screen.getByText('Thinking 5.0s')).toBeInTheDocument();

    jest.useRealTimers();
});

test('hides the thinking indicator once answer text starts streaming', () => {
    jest.useFakeTimers();
    jest.setSystemTime(2000);
    render(<MessageList
        messages={[{
            id: 'assistant-streaming-answer',
            role: 'assistant',
            content: [{ id: 'answer-1', type: 'text', text: 'Partial answer' }],
            startedAt: 1000,
            durationMs: 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(screen.queryByText(/^Thinking/)).not.toBeInTheDocument();
    expect(document.querySelector('.thinking-sparkle')).not.toBeInTheDocument();
    expect(screen.getByText('Thought completed 1.0s')).toBeInTheDocument();
    act(() => jest.advanceTimersByTime(2000));
    expect(screen.getByText('Thought completed 1.0s')).toBeInTheDocument();
    jest.useRealTimers();
});

test('does not render a second thinking status under the assistant message', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-waiting',
            role: 'assistant',
            content: [],
            startedAt: 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(document.querySelectorAll('.thinking-summary')).toHaveLength(1);
    expect(document.querySelector('.thinking-indicator')).not.toBeInTheDocument();
    expect(screen.queryByText('Thinking')).not.toBeInTheDocument();
});

test('shows tool-result analysis as the last live timeline step', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-analyzing',
            role: 'assistant',
            activity: 'analyzing_tool_results',
            content: [
                { id: 'call-1', type: 'tool', toolCall: { toolCallId: 'call-1', name: 'Read', status: 'completed', output: 'done' } },
            ],
            startedAt: 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(document.querySelector('.thinking-indicator')).not.toBeInTheDocument();
    const analyzing = document.querySelector('.timeline-item.analyzing') as HTMLElement;
    expect(analyzing).toHaveClass('active');
    expect(analyzing).toHaveTextContent('Analyzing tool results...');
});

test('renders a rate-limit retry as an alert instead of a thinking indicator', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-retry',
            role: 'assistant',
            activity: { type: 'model_retry', attempt: 2, maxAttempts: 5, retryAfterSeconds: 8 },
            content: [],
            startedAt: 1000,
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    expect(document.querySelector('.thinking-indicator')).not.toBeInTheDocument();
    expect(document.querySelector('.model-retry-alert')).toHaveTextContent(
        'The model service is rate limited. Request 2/5 is in progress; recovery is expected in 8 seconds...',
    );
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

    const details = document.querySelector('.thinking-timeline') as HTMLDetailsElement;
    expect(details).not.toHaveAttribute('open');
    expect(details.querySelector('.thinking-content')).toBeInTheDocument();

    await userEvent.click(details.querySelector('summary') as HTMLElement);
    expect(details).toHaveAttribute('open');
});

test('expands thinking while the assistant is working and collapses it after completion', () => {
    const message = {
        id: 'assistant-live-thinking',
        role: 'assistant' as const,
        content: [{ id: 'thinking-1', type: 'thinking' as const, text: 'Live reasoning' }],
        startedAt: 1000,
    };
    const { rerender } = render(<MessageList
        messages={[message]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt
    />);

    const details = document.querySelector('.answer-meta-details') as HTMLDetailsElement;
    expect(details).toHaveAttribute('open');
    expect(details.querySelector('.thinking-sparkle')).toBeInTheDocument();

    rerender(<MessageList
        messages={[{ ...message, durationMs: 5000 }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(details).not.toHaveAttribute('open');
    expect(details.querySelector('.thinking-sparkle')).not.toBeInTheDocument();
});

test('uses the completed thinking status as the details toggle when duration is available', async () => {
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

    expect(screen.queryByText('Thinking')).not.toBeInTheDocument();
    const details = document.querySelector('.thinking-timeline') as HTMLDetailsElement;
    expect(details.querySelector('.thinking-chevron')).toBeInTheDocument();
    expect(details).not.toHaveAttribute('open');

    await userEvent.click(details.querySelector('summary') as HTMLElement);
    expect(details).toHaveAttribute('open');
});

test('vertically centers timeline markers on the first line of the title', async () => {
    render(<MessageList
        messages={[{
            id: 'assistant-marker-alignment',
            role: 'assistant',
            content: [
                { id: 'thinking-1', type: 'thinking', text: 'Reasoning details' },
                { id: 'text-1', type: 'text', text: 'Answer text' },
            ],
        }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const details = document.querySelector('.thinking-timeline') as HTMLDetailsElement;
    await userEvent.click(details.querySelector('summary') as HTMLElement);

    const item = details.querySelector('.timeline-item.thinking') as HTMLElement;
    const marker = item.querySelector('.timeline-marker') as HTMLElement;
    const heading = item.querySelector('.timeline-heading') as HTMLElement;

    expect(getComputedStyle(heading).fontSize).toBe('14px');
    expect(getComputedStyle(heading).lineHeight).toBe('1.5');
    expect(getComputedStyle(marker).width).toBe('8px');
    expect(getComputedStyle(marker).height).toBe('8px');
});

test('shows a model switch notice at the conversation bottom when there are no messages', () => {
    render(<MessageList
        messages={[]}
        notices={[{ id: 'notice-1', type: 'model_switch', model: 'GLM-4.7' }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    expect(screen.getByRole('status')).toHaveTextContent('Switched model: GLM-4.7');
});

test('keeps a model switch notice after the message that was current when the model changed', () => {
    render(<MessageList
        messages={[
            { id: 'user-1', role: 'user', content: [{ id: 'user-text-1', type: 'text', text: 'first question' }] },
            { id: 'assistant-1', role: 'assistant', content: [{ id: 'text-1', type: 'text', text: 'first answer' }] },
            { id: 'user-2', role: 'user', content: [{ id: 'user-text-2', type: 'text', text: 'second question' }] },
        ]}
        notices={[{ id: 'notice-1', type: 'model_switch', model: 'GLM-4.7', afterMessageId: 'assistant-1' }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const turns = document.querySelectorAll('.message-turn');
    expect(turns).toHaveLength(2);
    expect(turns[0].querySelector('[role="status"]')).toHaveTextContent('Switched model: GLM-4.7');
    expect(turns[1].querySelector('[role="status"]')).toBeNull();
    const assistant = turns[0].querySelector('.message.assistant') as HTMLElement;
    const notice = turns[0].querySelector('[role="status"]') as HTMLElement;
    expect(assistant.compareDocumentPosition(notice) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
});

test('places an unanchored model switch notice before later messages', () => {
    render(<MessageList
        messages={[{ id: 'user-1', role: 'user', content: [{ id: 'user-text-1', type: 'text', text: 'hello' }] }]}
        notices={[{ id: 'notice-1', type: 'model_switch', model: 'GLM-4.7' }]}
        onPermissionDecision={noopPermissionDecision}
        pendingPrompt={false}
    />);

    const status = screen.getByRole('status');
    const prompt = document.querySelector('.user-prompt-sticky') as HTMLElement;
    expect(status.compareDocumentPosition(prompt) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
    expect(status).toHaveTextContent('Switched model: GLM-4.7');
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

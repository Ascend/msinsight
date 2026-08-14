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

jest.mock('react-markdown', () => ({
    __esModule: true,
    default: ({ children }: { children: string }) => <MockMarkdown text={children} />,
}));

jest.mock('remark-gfm', () => ({
    __esModule: true,
    default: jest.fn(),
}));

import { MessageList, toolCallDisplayName } from '../../components/MessageList';
import { upsertToolCall } from '../../hooks/toolCalls';

const noopPermissionDecision = jest.fn();

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

test('upserts live tool call completion without duplicating its card', () => {
    const started = upsertToolCall([], {
        toolCallId: 'call-1',
        name: 'msinsight',
        status: 'in_progress',
        input: '{}',
        startedAt: 100,
    });
    const completed = upsertToolCall(started, {
        toolCallId: 'call-1',
        name: 'msinsight',
        status: 'completed',
        output: '{"module":{"tables":[]}}',
        durationMs: 50,
    });

    expect(completed).toEqual([{
        toolCallId: 'call-1',
        name: 'msinsight',
        status: 'completed',
        input: '{}',
        output: '{"module":{"tables":[]}}',
        startedAt: 100,
        durationMs: 50,
    }]);
});

test('wraps long markdown text and inline code inside the message width', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-long-text',
            role: 'assistant',
            text: 'A very long token abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz and `/workspace/really/long/path/that/should/wrap/in/the/panel/file.ts`',
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

test('keeps wide code blocks and tables horizontally scrollable inside their own blocks', () => {
    render(<MessageList
        messages={[{
            id: 'assistant-wide-blocks',
            role: 'assistant',
            text: '```ts\nconst value = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";\n```\n\n| HeaderHeaderHeaderHeader | OtherHeaderHeaderHeader |\n| --- | --- |\n| CellCellCellCellCell | OtherCellCellCellCell |',
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

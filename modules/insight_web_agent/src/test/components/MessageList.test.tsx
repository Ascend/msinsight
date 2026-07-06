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

import { MessageList } from '../../components/MessageList';

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

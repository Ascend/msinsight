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
import React from 'react';
import { ThemeProvider } from '@emotion/react';
import { act, fireEvent, render, screen } from '@testing-library/react';
import '@testing-library/jest-dom';
import { themeInstance } from '@insight/lib/theme';
import { CodeBlock } from '../../../components/markdown/CodeBlock';
import { MarkdownRenderer } from '../../../components/markdown/MarkdownRenderer';

const Wrapper = ({ children }: React.PropsWithChildren): JSX.Element => (
    <ThemeProvider theme={themeInstance.getTheme().dark}>{children}</ThemeProvider>
);
const renderBlock = (element: React.ReactElement): ReturnType<typeof render> => render(element, { wrapper: Wrapper });

const restoreProperty = (object: object, name: string, descriptor?: PropertyDescriptor): void => {
    if (descriptor) Object.defineProperty(object, name, descriptor);
    else Reflect.deleteProperty(object, name);
};

describe('CodeBlock', () => {
    const clipboard = Object.getOwnPropertyDescriptor(navigator, 'clipboard');
    const execCommand = Object.getOwnPropertyDescriptor(document, 'execCommand');

    afterEach(() => {
        restoreProperty(navigator, 'clipboard', clipboard);
        restoreProperty(document, 'execCommand', execCommand);
        jest.useRealTimers();
    });

    test('renders the language label and highlights known languages', () => {
        const { container } = renderBlock(<CodeBlock code={'def run():\n    return 1'} language="py" />);
        expect(screen.getByText('python')).toBeInTheDocument();
        expect(container.querySelector('.hljs-keyword')).not.toBeNull();
    });

    test('falls back to plain text for unknown languages', () => {
        const { container } = renderBlock(<CodeBlock code={'<unsafe>'} language="unknown-lang" />);
        expect(screen.getByText('text')).toBeInTheDocument();
        expect(container.querySelector('code')?.innerHTML).toBe('&lt;unsafe&gt;');
    });

    test('copies the raw code and restores the label', async () => {
        jest.useFakeTimers();
        const writeText = jest.fn().mockResolvedValue(undefined);
        Object.defineProperty(navigator, 'clipboard', {
            configurable: true,
            value: { writeText },
        });
        renderBlock(<CodeBlock code={'print(1)'} language="python" />);

        fireEvent.click(screen.getByRole('button', { name: 'Copy' }));
        expect(writeText).toHaveBeenCalledWith('print(1)');
        await act(async () => {
            await Promise.resolve();
        });
        expect(screen.getByRole('button', { name: 'Copied' })).toBeInTheDocument();

        act(() => {
            jest.advanceTimersByTime(2000);
        });
        expect(screen.getByRole('button', { name: 'Copy' })).toBeInTheDocument();
    });

    test.each([false, undefined])('reports copy failure when the fallback returns %s', async (result) => {
        const writeText = jest.fn().mockRejectedValue(new Error('Clipboard permission denied'));
        Object.defineProperty(navigator, 'clipboard', { configurable: true, value: { writeText } });
        Object.defineProperty(document, 'execCommand', {
            configurable: true,
            value: result === undefined ? undefined : jest.fn().mockReturnValue(result),
        });
        renderBlock(<CodeBlock code="print(1)" language="python" />);

        fireEvent.click(screen.getByRole('button', { name: 'Copy' }));
        expect(await screen.findByRole('button', { name: 'Copy failed' })).toBeInTheDocument();
        expect(screen.queryByRole('button', { name: 'Copied' })).not.toBeInTheDocument();
        expect(document.querySelector('textarea')).not.toBeInTheDocument();
    });

    test('reports success when the legacy clipboard fallback succeeds', async () => {
        Object.defineProperty(navigator, 'clipboard', { configurable: true, value: undefined });
        const copy = jest.fn().mockReturnValue(true);
        Object.defineProperty(document, 'execCommand', { configurable: true, value: copy });
        renderBlock(<CodeBlock code="print(1)" language="python" />);

        fireEvent.click(screen.getByRole('button', { name: 'Copy' }));
        expect(await screen.findByRole('button', { name: 'Copied' })).toBeInTheDocument();
        expect(copy).toHaveBeenCalledWith('copy');
    });

    test('toggles line wrapping', () => {
        const { container } = renderBlock(<CodeBlock code={'const a = 1;'} language="javascript" />);
        const shell = container.querySelector('.md-code-block');
        expect(shell).not.toHaveClass('wrapped');

        fireEvent.click(screen.getByRole('button', { name: 'Wrap' }));
        expect(shell).toHaveClass('wrapped');
        expect(screen.getByRole('button', { name: 'Unwrap' })).toHaveAttribute('aria-pressed', 'true');
    });

    test('collapses long code blocks until expanded', () => {
        const longCode = Array.from({ length: 40 }, (_, index) => `line ${index}`).join('\n');
        const { container } = renderBlock(<CodeBlock code={longCode} language="text" />);
        const shell = container.querySelector('.md-code-block');
        expect(shell).toHaveClass('clamped');

        fireEvent.click(screen.getByRole('button', { name: 'Expand' }));
        expect(shell).not.toHaveClass('clamped');
        expect(screen.getByRole('button', { name: 'Collapse' })).toBeInTheDocument();
    });
});

describe('MarkdownRenderer', () => {
    test('renders multiline bracket math as a block without consuming the next paragraph', () => {
        const text = ['Before', '', '\\[', '\\begin{aligned}', 'x &= 1 \\\\', 'y &= 2', '\\end{aligned}', '\\]', '', 'After'].join('\n');
        const { container } = renderBlock(<MarkdownRenderer text={text} />);
        expect(container.querySelector('.katex-display')).toBeInTheDocument();
        expect(container.querySelector('.katex-error')).not.toBeInTheDocument();
        expect(screen.getByText('After').tagName).toBe('P');
    });

    test('renders one-line bracket math in display mode', () => {
        const { container } = renderBlock(<MarkdownRenderer text={'\\[x+1\\]'} />);
        expect(container.querySelector('.katex-display')).toBeInTheDocument();
    });

    test('preserves external link titles and the footnote backlink target', () => {
        const text = '[site](https://example.com "Link title") and a note[^note].\n\n[^note]: Footnote.';
        renderBlock(<MarkdownRenderer text={text} />);
        const site = screen.getByRole('link', { name: 'site' });
        expect(site).toHaveAttribute('title', 'Link title');
        expect(site).toHaveAttribute('target', '_blank');
        const reference = screen.getByRole('link', { name: '1' });
        const backlink = screen.getByRole('link', { name: 'Back to reference 1' });
        expect(reference).toHaveAttribute('id', 'user-content-fnref-note');
        expect(backlink).toHaveAttribute('href', '#user-content-fnref-note');
        expect(backlink).not.toHaveAttribute('target');
    });

    test('keeps wrap and expansion state when streaming finishes and highlighting starts', () => {
        const code = Array.from({ length: 35 }, () => 'return 1').join('\n');
        const text = `\`\`\`python\n${code}\n\`\`\``;
        const { container, rerender } = renderBlock(<MarkdownRenderer streaming text={text} />);
        expect(container.querySelector('.hljs-keyword')).not.toBeInTheDocument();
        fireEvent.click(screen.getByRole('button', { name: 'Wrap' }));
        fireEvent.click(screen.getByRole('button', { name: 'Expand' }));

        rerender(<MarkdownRenderer text={text} />);
        expect(container.querySelector('.hljs-keyword')).toBeInTheDocument();
        expect(screen.getByRole('button', { name: 'Unwrap' })).toHaveAttribute('aria-pressed', 'true');
        expect(screen.getByRole('button', { name: 'Collapse' })).toHaveAttribute('aria-expanded', 'true');
    });
});

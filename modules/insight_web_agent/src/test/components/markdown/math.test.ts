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
import { normalizeMath } from '../../../components/markdown/math';

describe('normalizeMath', () => {
    test('converts inline and display LaTeX delimiters', () => {
        expect(normalizeMath('value \\(x^2 + y^2\\) here')).toBe('value $x^2 + y^2$ here');
        expect(normalizeMath('\\( x^2 \\)')).toBe('$x^2$');
        expect(normalizeMath('\\[E = mc^2\\]')).toBe('$$\nE = mc^2\n$$');
    });

    test('keeps fenced and inline code untouched', () => {
        const fenced = '```text\n\\(x\\)\n```';
        const inline = 'use `\\(x\\)` syntax';
        expect(normalizeMath(fenced)).toBe(fenced);
        expect(normalizeMath(inline)).toBe(inline);
    });

    test('keeps indented code intact, including dollar fences while streaming', () => {
        const text = 'Example:\n\n    print("\\(x\\)")\n    echo $$\n\nAfter';
        expect(normalizeMath(text)).toBe(text);
        expect(normalizeMath(text, true)).toBe(text);
    });

    test('keeps an existing inline formula and a literal double dollar intact', () => {
        const text = '$\\text{literal \\(x\\)}$ and a literal $$ without a closing fence.';
        expect(normalizeMath(text, true)).toBe(text);
    });

    test('normalizes display fences without breaking quote and list containers', () => {
        expect(normalizeMath('> \\[x+1\\]')).toBe('> $$\n> x+1\n> $$');
        expect(normalizeMath('- \\[x+1\\]')).toBe('- $$\n  x+1\n  $$');
    });

    test('keeps escaped backslashes untouched', () => {
        expect(normalizeMath('path \\\\(x\\\\)')).toBe('path \\\\(x\\\\)');
    });

    test('does not convert an unclosed delimiter', () => {
        expect(normalizeMath('broken \\(x')).toBe('broken \\(x');
    });

    test('hides an unclosed display block while streaming', () => {
        expect(normalizeMath('result:\n$$\na + b', true)).toBe('result:\n');
        expect(normalizeMath('result:\n$$\na + b\n$$', true)).toBe('result:\n$$\na + b\n$$');
    });

    test('keeps balanced display math when streaming completes', () => {
        const text = '$$\n\\frac{a}{b}\n$$';
        expect(normalizeMath(text, true)).toBe(text);
        expect(normalizeMath(text)).toBe(text);
    });

    test('keeps closed native display math in nested containers while streaming', () => {
        const text = '> $$$\n> x^2\n> $$$\n\nAfter';
        expect(normalizeMath(text, true)).toBe(text);
    });

    test('keeps link and image destinations and titles untouched', () => {
        const link = '[文档](https://example.com/a\\(b\\))';
        expect(normalizeMath(link)).toBe(link);
        const titled = '[文档](https://example.com/a\\(b\\) "标题 \\(x\\)")';
        expect(normalizeMath(titled)).toBe(titled);
        const image = '![图](https://example.com/a\\(b\\).png "t\\(x\\)")';
        expect(normalizeMath(image)).toBe(image);
        const definition = '[ref]: https://example.com/a\\(b\\) "t\\(x\\)"';
        expect(normalizeMath(definition)).toBe(definition);
        const reference = '[文档][ref]';
        expect(normalizeMath(reference)).toBe(reference);
    });

    test('still converts formulas in link text while preserving the destination', () => {
        expect(normalizeMath('[公式 \\(x^2\\)](https://example.com/a\\(b\\))'))
            .toBe('[公式 $x^2$](https://example.com/a\\(b\\))');
    });

    test('keeps literal currency amounts as text', () => {
        expect(normalizeMath('Price increased from $5 to $10')).toBe('Price increased from \\$5 to \\$10');
        expect(normalizeMath('Price increased from $5-$10')).toBe('Price increased from \\$5-\\$10');
        expect(normalizeMath('cost $5, revenue $10')).toBe('cost \\$5, revenue \\$10');
    });

    test('does not escape real inline or display math', () => {
        expect(normalizeMath('$x^2$')).toBe('$x^2$');
        expect(normalizeMath('$$5+5$$')).toBe('$$5+5$$');
    });
});

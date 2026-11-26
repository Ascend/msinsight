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
import { render } from '@testing-library/react';
import '@testing-library/jest-dom';
import { MarkdownRenderer } from '../../../components/markdown/MarkdownRenderer';

describe('MarkdownRenderer', () => {
    test('keeps literal currency amounts in chat text', () => {
        const { container } = render(<MarkdownRenderer text="Price increased from $5 to $10" />);
        expect(container.querySelector('.katex')).toBeNull();
        expect(container.textContent).toContain('Price increased from $5 to $10');
    });

    test('keeps adjacent currency ranges as text', () => {
        const { container } = render(<MarkdownRenderer text="Price increased from $5-$10" />);
        expect(container.querySelector('.katex')).toBeNull();
        expect(container.textContent).toContain('Price increased from $5-$10');
    });

    test('still renders inline math that does not look like money', () => {
        const { container } = render(<MarkdownRenderer text="Euler: $e^{i\\pi}+1=0$" />);
        expect(container.querySelector('.katex')).not.toBeNull();
    });
});

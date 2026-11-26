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
import { createContext, useContext, useMemo } from 'react';
import type { ComponentPropsWithoutRef } from 'react';
import ReactMarkdown from 'react-markdown';
import type { Components, ExtraProps } from 'react-markdown';
import rehypeKatex from 'rehype-katex';
import type { Element, ElementContent } from 'hast';
import 'katex/dist/katex.min.css';
import './markdown.css';
import { CodeBlock } from './CodeBlock';
import { markdownRemarkPlugins, normalizeMath } from './math';

const KATEX_OPTIONS = {
    throwOnError: false,
    strict: 'ignore',
    trust: false,
    output: 'htmlAndMathml',
    errorColor: '#cc0000',
} as const;

const textOf = (node?: ElementContent | ElementContent[]): string => {
    if (!node) return '';
    const nodes = Array.isArray(node) ? node : [node];
    return nodes.map((child) => {
        if (child.type === 'text') return child.value;
        if (child.type === 'element') return textOf(child.children);
        return '';
    }).join('');
};

const languageOf = (element?: Element): string | undefined => {
    const className = element?.properties?.className;
    const classes = Array.isArray(className)
        ? className.map(String)
        : typeof className === 'string' ? className.split(/\s+/) : [];
    const languageClass = classes.find(item => item.startsWith('language-'));
    return languageClass?.slice('language-'.length);
};

const StreamingContext = createContext(false);

const MarkdownPre = ({ node, children }: ComponentPropsWithoutRef<'pre'> & ExtraProps): JSX.Element => {
    const streaming = useContext(StreamingContext);
    const codeElement = node?.children.find(
        (child: ElementContent): child is Element => child.type === 'element' && child.tagName === 'code',
    );
    if (!codeElement) return <pre>{children}</pre>;
    return (
        <CodeBlock
            code={textOf(codeElement).replace(/\n$/, '')}
            language={languageOf(codeElement)}
            streaming={streaming}
        />
    );
};

const MARKDOWN_COMPONENTS: Components = {
    a: ({ node: _node, href, ...props }) => {
        if (href?.startsWith('#')) return <a {...props} href={href} />;
        return <a {...props} href={href} rel="noopener noreferrer" target="_blank" />;
    },
    pre: MarkdownPre,
};

export const MarkdownRenderer = ({
    text,
    streaming = false,
}: {
    text: string;
    streaming?: boolean;
}): JSX.Element => {
    const content = useMemo(() => normalizeMath(text, streaming), [text, streaming]);

    return (
        <StreamingContext.Provider value={streaming}>
            <ReactMarkdown
                components={MARKDOWN_COMPONENTS}
                rehypePlugins={[[rehypeKatex, KATEX_OPTIONS]]}
                remarkPlugins={markdownRemarkPlugins}
            >
                {content}
            </ReactMarkdown>
        </StreamingContext.Provider>
    );
};

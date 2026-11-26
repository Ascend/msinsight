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
import type { Nodes } from 'mdast';
import remarkGfm from 'remark-gfm';
import remarkMath from 'remark-math';
import remarkParse from 'remark-parse';
import { unified } from 'unified';

export const markdownRemarkPlugins = [remarkGfm, remarkMath];

const parser = unified().use(remarkParse).use(markdownRemarkPlugins).freeze();

interface ProtectedRange {
    start: number;
    end: number;
    displayMath: boolean;
}

const nodeRange = (node: Nodes): { start: number; end: number } | undefined => {
    const start = node.position?.start.offset;
    const end = node.position?.end.offset;
    return start === undefined || end === undefined ? undefined : { start, end };
};

// Link/image destinations and titles are not display text: converting \(...\) inside them
// would silently rewrite the URL or title. Protect everything between "](" and the final ")".
const linkDestinationRange = (text: string, node: Nodes): ProtectedRange | undefined => {
    const range = nodeRange(node);
    if (!range) return undefined;
    const source = text.slice(range.start, range.end);
    if (source.startsWith('<')) return { ...range, displayMath: false };
    if (!source.endsWith(')')) return undefined;
    const boundary = source.lastIndexOf('](');
    if (boundary < 0) return undefined;
    return { start: range.start + boundary + 2, end: range.end - 1, displayMath: false };
};

const isCurrencyLikeMath = (text: string, node: Nodes): boolean => {
    if (node.type !== 'inlineMath') return false;
    const range = nodeRange(node);
    if (!range) return false;
    // Single-dollar spans that start with a digit are almost always money ("$5 to $10").
    return /^\$[0-9]/.test(text.slice(range.start, range.end));
};

const protectedRanges = (text: string, protectMath = true): ProtectedRange[] => {
    const ranges: ProtectedRange[] = [];
    const visit = (node: Nodes): void => {
        if (node.type === 'code' || node.type === 'inlineCode') {
            const range = nodeRange(node);
            if (range) ranges.push({ ...range, displayMath: false });
            return;
        }
        if (node.type === 'math' || node.type === 'inlineMath') {
            // Currency-like spans are exposed so the currency pass can escape their "$".
            if (!protectMath && isCurrencyLikeMath(text, node)) return;
            const range = nodeRange(node);
            if (range) ranges.push({ ...range, displayMath: node.type === 'math' });
            return;
        }
        if (node.type === 'link' || node.type === 'image') {
            const range = nodeRange(node);
            const destination = linkDestinationRange(text, node);
            if (destination) ranges.push(destination);
            const coversWholeNode = Boolean(range && destination && destination.start === range.start && destination.end === range.end);
            if (!coversWholeNode && 'children' in node) node.children.forEach(visit);
            return;
        }
        if (node.type === 'definition' || node.type === 'linkReference' || node.type === 'imageReference' ||
            node.type === 'html' || node.type === 'footnoteReference') {
            const range = nodeRange(node);
            if (range) ranges.push({ ...range, displayMath: false });
            return;
        }
        if ('children' in node) node.children.forEach(visit);
    };
    visit(parser.parse(text));
    return ranges.sort((left, right) => left.start - right.start);
};

const escapeCurrencySegment = (segment: string): string => {
    let output = '';
    for (let index = 0; index < segment.length; index += 1) {
        const character = segment[index];
        const next = segment[index + 1];
        if (character === '$' && !isEscaped(segment, index) && next !== undefined && next >= '0' && next <= '9') {
            output += '\\$';
            continue;
        }
        output += character;
    }
    return output;
};

const escapeCurrencyDollars = (text: string, ranges: ProtectedRange[]): string => {
    let output = '';
    let cursor = 0;
    for (const range of ranges) {
        output += escapeCurrencySegment(text.slice(cursor, range.start));
        output += text.slice(range.start, range.end);
        cursor = range.end;
    }
    return output + escapeCurrencySegment(text.slice(cursor));
};

const isEscaped = (text: string, index: number): boolean => {
    let backslashes = 0;
    for (let cursor = index - 1; cursor >= 0 && text[cursor] === '\\'; cursor -= 1) {
        backslashes += 1;
    }
    return backslashes % 2 === 1;
};

const findDelimiter = (text: string, delimiter: string, from: number, end: number): number => {
    let index = text.indexOf(delimiter, from);
    while (index >= 0 && index + delimiter.length <= end) {
        if (!isEscaped(text, index)) return index;
        index = text.indexOf(delimiter, index + delimiter.length);
    }
    return -1;
};

const containerPrefix = /^(?:[ \t]*>[ \t]?)*[ \t]*/;

const displayMath = (text: string, start: number, close: number): string => {
    const before = text.slice(text.lastIndexOf('\n', start - 1) + 1, start);
    const prefix = containerPrefix.exec(before)?.[0] ?? '';
    const listMarker = /^(?:[-+*]|\d+[.)])[ \t]+/.exec(before.slice(prefix.length))?.[0] ?? '';
    const continuation = prefix + ' '.repeat(listMarker.length);
    const newLine = `\n${continuation}`;
    const body = text.slice(start + 2, close);
    const lastLineStart = body.lastIndexOf('\n') + 1;
    const lastLine = body.slice(lastLineStart);

    // Keep both fences on their own lines without moving the formula out of a list or quote.
    const beforeFence = before === prefix + listMarker ? '' : newLine;
    const afterOpen = /^[ \t]*\r?\n/.test(body) ? '' : newLine;
    const beforeClose = lastLineStart > 0 && containerPrefix.exec(lastLine)?.[0] === lastLine ? '' : newLine;
    const afterFence = /^[ \t]*(?:\r?\n|$)/.test(text.slice(close + 2)) ? '' : newLine;
    return `${beforeFence}$$${afterOpen}${body}${beforeClose}$$${afterFence}`;
};

const convertDelimiters = (text: string, start: number, end: number): string => {
    let output = '';
    let index = start;
    while (index < end) {
        if (text[index] === '\\' && !isEscaped(text, index) && index + 1 < end) {
            const next = text[index + 1];
            if (next === '(') {
                const close = findDelimiter(text, '\\)', index + 2, end);
                if (close >= 0) {
                    output += `$${text.slice(index + 2, close).trim()}$`;
                    index = close + 2;
                    continue;
                }
            }
            if (next === '[') {
                const close = findDelimiter(text, '\\]', index + 2, end);
                if (close >= 0) {
                    output += displayMath(text, index, close);
                    index = close + 2;
                    continue;
                }
            }
        }
        output += text[index];
        index += 1;
    }
    return output;
};

const isClosedDisplayMath = (source: string): boolean => {
    const open = /^\${2,}/.exec(source)?.[0];
    const lines = source.split(/\r?\n/);
    const last = lines.at(-1) ?? '';
    const prefix = containerPrefix.exec(last)?.[0] ?? '';
    const close = /^(\${2,})[ \t]*$/.exec(last.slice(prefix.length))?.[1];
    return lines.length > 1 && open !== undefined && close !== undefined && close.length >= open.length;
};

/**
 * Converts \(...\) / \[...\] into the $...$ / $$...$$ syntax understood by remark-math,
 * leaving Markdown code untouched. While streaming, an unclosed $$ block is hidden
 * so the user never sees a half-rendered formula.
 */
export const normalizeMath = (text: string, streaming = false): string => {
    const source = /\$[0-9]/.test(text)
        ? escapeCurrencyDollars(text, protectedRanges(text, false))
        : text;
    const convert = source.includes('\\(') || source.includes('\\[');
    if (!convert && !(streaming && source.includes('$$'))) return source;
    const ranges = protectedRanges(source);
    let output = '';
    let cursor = 0;
    for (const range of ranges) {
        output += convert ? convertDelimiters(source, cursor, range.start) : source.slice(cursor, range.start);
        if (streaming && range.displayMath && !isClosedDisplayMath(source.slice(range.start, range.end))) return output;
        output += source.slice(range.start, range.end);
        cursor = range.end;
    }
    return output + (convert ? convertDelimiters(source, cursor, source.length) : source.slice(cursor));
};

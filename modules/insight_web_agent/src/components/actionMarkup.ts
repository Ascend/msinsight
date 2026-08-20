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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import type { ActionItem } from '../types';

const OPEN_TAG = '<insight-action>';
const CLOSE_TAG = '</insight-action>';
const MAX_PAYLOAD_LENGTH = 16000;
const MAX_JSON_VALUES = 2000;
const ACTION_FIELDS = new Set(['label', 'description', 'command', 'args']);

interface TextRange {
    start: number;
    end: number;
}

export type ActionMarkupSegment =
    | { key: string; type: 'markdown'; text: string }
    | { key: string; type: 'literal'; text: string }
    | { key: string; type: 'action'; action: ActionItem };

export const parseActionMarkup = (
    text: string,
    { streaming = false, keyPrefix = 'content' }: { streaming?: boolean; keyPrefix?: string } = {},
): ActionMarkupSegment[] => {
    const ranges = markdownCodeRanges(text);
    const segments: ActionMarkupSegment[] = [];
    let cursor = 0;

    while (cursor < text.length) {
        const openIndex = findOutsideRanges(text, OPEN_TAG, cursor, ranges);
        if (openIndex < 0) {
            appendTrailingContent(segments, text, cursor, ranges, streaming, keyPrefix);
            break;
        }
        appendMarkdown(segments, text.slice(cursor, openIndex), keyPrefix, cursor);
        const closeIndex = findOutsideRanges(text, CLOSE_TAG, openIndex + OPEN_TAG.length, ranges);
        if (closeIndex < 0) {
            if (!streaming) appendLiteral(segments, text.slice(openIndex), keyPrefix, openIndex);
            break;
        }
        const nestedOpenIndex = findOutsideRanges(text, OPEN_TAG, openIndex + OPEN_TAG.length, ranges);
        if (nestedOpenIndex >= 0 && nestedOpenIndex < closeIndex) {
            appendLiteral(segments, text.slice(openIndex, nestedOpenIndex), keyPrefix, openIndex);
            cursor = nestedOpenIndex;
            continue;
        }
        const end = closeIndex + CLOSE_TAG.length;
        const source = text.slice(openIndex, end);
        const action = parseActionPayload(text.slice(openIndex + OPEN_TAG.length, closeIndex), `${keyPrefix}:action:${openIndex}`);
        if (action) segments.push({ key: action.actionId, type: 'action', action });
        else appendLiteral(segments, source, keyPrefix, openIndex);
        cursor = end;
    }

    return segments;
};

const parseActionPayload = (source: string, actionId: string): ActionItem | undefined => {
    if (source.length > MAX_PAYLOAD_LENGTH) return undefined;
    let value: unknown;
    try {
        value = JSON.parse(source.trim());
    } catch (_error) {
        return undefined;
    }
    if (!value || typeof value !== 'object' || Array.isArray(value)) return undefined;
    const record = value as Record<string, unknown>;
    if (Object.keys(record).some(key => !ACTION_FIELDS.has(key))) return undefined;
    if (!Object.keys(record).every(key => ACTION_FIELDS.has(key)) || Object.keys(record).length !== ACTION_FIELDS.size) return undefined;
    const label = boundedString(record.label, 200);
    const description = boundedString(record.description, 2000);
    const command = boundedString(record.command, 300);
    const args = record.args;
    if (!label || !description || !command || !isJsonObject(args)) return undefined;
    return { actionId, label, description, command, args };
};

const boundedString = (value: unknown, maxLength: number): string | undefined => {
    if (typeof value !== 'string') return undefined;
    const text = value.trim();
    return text && text.length <= maxLength ? text : undefined;
};

const isJsonObject = (value: unknown): value is Record<string, unknown> => (
    Boolean(value) && typeof value === 'object' && !Array.isArray(value) && isJsonValue(value)
);

const isJsonValue = (value: unknown): boolean => {
    const remaining = [value];
    let visited = 0;
    while (remaining.length > 0) {
        if (++visited > MAX_JSON_VALUES) return false;
        const current = remaining.pop();
        if (current === null || typeof current === 'string' || typeof current === 'boolean') continue;
        if (typeof current === 'number') {
            if (!Number.isFinite(current)) return false;
            continue;
        }
        if (Array.isArray(current)) {
            remaining.push(...current);
            continue;
        }
        if (typeof current === 'object') {
            remaining.push(...Object.values(current as Record<string, unknown>));
            continue;
        }
        return false;
    }
    return true;
};

const appendTrailingContent = (
    segments: ActionMarkupSegment[],
    text: string,
    cursor: number,
    ranges: TextRange[],
    streaming: boolean,
    keyPrefix: string,
): void => {
    const closeIndex = findOutsideRanges(text, CLOSE_TAG, cursor, ranges);
    const partialIndex = streaming ? trailingOpenTagPrefixIndex(text, cursor, ranges) : -1;
    const end = partialIndex >= 0 ? partialIndex : text.length;
    if (closeIndex < 0 || closeIndex >= end) {
        appendMarkdown(segments, text.slice(cursor, end), keyPrefix, cursor);
        return;
    }
    appendMarkdown(segments, text.slice(cursor, closeIndex), keyPrefix, cursor);
    appendLiteral(segments, CLOSE_TAG, keyPrefix, closeIndex);
    appendTrailingContent(segments, text, closeIndex + CLOSE_TAG.length, ranges, streaming, keyPrefix);
};

const appendMarkdown = (segments: ActionMarkupSegment[], text: string, keyPrefix: string, offset: number): void => {
    if (!text) return;
    const previous = segments.at(-1);
    if (previous?.type === 'markdown') {
        previous.text += text;
        return;
    }
    segments.push({ key: `${keyPrefix}:markdown:${offset}`, type: 'markdown', text });
};

const appendLiteral = (segments: ActionMarkupSegment[], text: string, keyPrefix: string, offset: number): void => {
    if (!text) return;
    segments.push({ key: `${keyPrefix}:literal:${offset}`, type: 'literal', text });
};

const trailingOpenTagPrefixIndex = (text: string, start: number, ranges: TextRange[]): number => {
    const maxLength = Math.min(OPEN_TAG.length - 1, text.length - start);
    for (let length = maxLength; length > 0; length -= 1) {
        const index = text.length - length;
        if (text.endsWith(OPEN_TAG.slice(0, length)) && !ranges.some(range => index >= range.start && index < range.end)) return index;
    }
    return -1;
};

const findOutsideRanges = (text: string, needle: string, start: number, ranges: TextRange[]): number => {
    let index = text.indexOf(needle, start);
    while (index >= 0) {
        if (!ranges.some(range => index >= range.start && index < range.end)) return index;
        index = text.indexOf(needle, index + needle.length);
    }
    return -1;
};

const markdownCodeRanges = (text: string): TextRange[] => {
    const fenced = fencedCodeRanges(text);
    const inline = inlineCodeRanges(text, fenced);
    return [...fenced, ...inline].sort((left, right) => left.start - right.start);
};

const markdownFenceLine = (line: string): { marker: string; trailing: string } | undefined => {
    let content = line;
    while (true) {
        const container = /^(?: {0,3}> ?| {0,3}(?:[-+*]|\d+[.)]) {1,4})/.exec(content)?.[0];
        if (!container) break;
        content = content.slice(container.length);
    }
    const match = /^(?: {0,3})(`{3,}|~{3,})(.*)$/.exec(content);
    return match ? { marker: match[1], trailing: match[2] } : undefined;
};

const fencedCodeRanges = (text: string): TextRange[] => {
    const ranges: TextRange[] = [];
    const lines = text.match(/.*(?:\n|$)/g)?.filter(Boolean) ?? [];
    let offset = 0;
    let fence: { character: string; length: number; start: number } | undefined;
    for (const line of lines) {
        const content = line.endsWith('\n') ? line.slice(0, -1) : line;
        const fenceLine = markdownFenceLine(content);
        const marker = fenceLine?.marker;
        if (!fence && marker) {
            fence = { character: marker[0], length: marker.length, start: offset };
        } else if (fence && marker?.[0] === fence.character && marker.length >= fence.length && fenceLine?.trailing.trim() === '') {
            ranges.push({ start: fence.start, end: offset + line.length });
            fence = undefined;
        }
        offset += line.length;
    }
    if (fence) ranges.push({ start: fence.start, end: text.length });
    return ranges;
};

const inlineCodeRanges = (text: string, fenced: TextRange[]): TextRange[] => {
    const ranges: TextRange[] = [];
    let index = 0;
    while (index < text.length) {
        const fencedRange = fenced.find(range => index >= range.start && index < range.end);
        if (fencedRange) {
            index = fencedRange.end;
            continue;
        }
        if (text[index] !== '`') {
            index += 1;
            continue;
        }
        let length = 1;
        while (text[index + length] === '`') length += 1;
        const marker = '`'.repeat(length);
        let close = text.indexOf(marker, index + length);
        while (close >= 0 && fenced.some(range => close >= range.start && close < range.end)) {
            close = text.indexOf(marker, close + length);
        }
        if (close < 0) {
            index += length;
            continue;
        }
        ranges.push({ start: index, end: close + length });
        index = close + length;
    }
    return ranges;
};

/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

const CONTEXT_REF = "insight-(?:hidden-context|system-prompt|assistant-preset)://project";
const contextMarker = () => new RegExp(`<context\\s+ref=["'](${CONTEXT_REF})["']\\s*(/?)>|\\[(${CONTEXT_REF})\\]`, "g");
const legacyContextHead = new RegExp(CONTEXT_REF);

const nextContextMarker = (pattern, text) => {
    let marker = pattern.exec(text);
    // Bracketed URIs can also be ordinary references in an assistant explanation.
    while (marker?.[3] && marker.index >= 200) {
        marker = pattern.exec(text);
    }
    return marker;
};

export const isHiddenResource = (block) => block?.type === "resource"
    && String(block.resource?.uri ?? "").startsWith("insight-");

// Find the end without parsing/copying a potentially large project-context object.
const jsonObjectEnd = (text, start) => {
    if (text[start] !== "{") {
        return undefined;
    }
    let depth = 0;
    let quoted = false;
    let escaped = false;
    for (let index = start; index < text.length; index++) {
        const character = text[index];
        if (quoted) {
            if (escaped) {
                escaped = false;
            } else if (character === "\\") {
                escaped = true;
            } else if (character === '"') {
                quoted = false;
            }
        } else if (character === '"') {
            quoted = true;
        } else if (character === "{") {
            depth++;
        } else if (character === "}") {
            depth--;
            if (depth === 0) {
                return index + 1;
            }
        }
    }
    return undefined;
};

const visibleTextRanges = (text, hiddenPromptTexts) => {
    if (!legacyContextHead.test(text)) {
        return [[0, text.length]];
    }
    const pattern = contextMarker();
    let marker = nextContextMarker(pattern, text);
    if (!marker) {
        // Compatibility with agents that prefix a URI without a context tag.
        return legacyContextHead.test(text.slice(0, 200)) ? [] : [[0, text.length]];
    }
    const ranges = [];
    let cursor = 0;
    while (marker) {
        ranges.push([cursor, marker.index]);
        const bodyStart = pattern.lastIndex;
        const ref = marker[1] ?? marker[3];
        let end;
        if (marker[1] && !marker[2]) {
            const closing = text.indexOf("</context>", bodyStart);
            if (closing !== -1) {
                end = closing + "</context>".length;
            }
        }
        let payloadStart = bodyStart;
        while (payloadStart < text.length && /\s/.test(text[payloadStart])) {
            payloadStart++;
        }
        if (end === undefined && ref === "insight-hidden-context://project") {
            end = jsonObjectEnd(text, payloadStart);
        }
        if (end === undefined) {
            const prompt = hiddenPromptTexts.find((candidate) => typeof candidate === "string"
                && candidate.trim() && text.startsWith(candidate.trim(), payloadStart));
            if (prompt) {
                end = payloadStart + prompt.trim().length;
            }
        }
        if (end === undefined) {
            // Old/unknown unbounded resource text has no reliable visible-body delimiter.
            // Do not guess its length or expose its continuation chunks.
            const next = nextContextMarker(pattern, text);
            end = next?.index ?? text.length;
        }
        cursor = end;
        pattern.lastIndex = end;
        marker = nextContextMarker(pattern, text);
    }
    ranges.push([cursor, text.length]);
    // Trim leading replay separators without losing source offsets or block identities.
    let leading = true;
    return ranges.flatMap(([start, end]) => {
        if (leading) {
            const newline = /^(?:\r?\n)+/.exec(text.slice(start, end));
            start += newline?.[0].length ?? 0;
        }
        if (start === end) {
            return [];
        }
        leading = false;
        return [[start, end]];
    });
};

/** Only presentation is filtered. Never apply this to outgoing prompts/model history. */
export const filterHistoryText = (value, hiddenPromptTexts = []) => {
    const text = String(value ?? "");
    return visibleTextRanges(text, hiddenPromptTexts).map(([start, end]) => text.slice(start, end)).join("");
};

const filterTextBlocks = (blocks, hiddenPromptTexts) => {
    const text = blocks.map((block) => block.text ?? "").join("");
    const ranges = visibleTextRanges(text, hiddenPromptTexts);
    let offset = 0;
    let rangeIndex = 0;
    return blocks.flatMap((block) => {
        const start = offset;
        offset += (block.text ?? "").length;
        const parts = [];
        while (rangeIndex < ranges.length && ranges[rangeIndex][0] < offset) {
            const [from, to] = ranges[rangeIndex];
            parts.push(text.slice(Math.max(start, from), Math.min(offset, to)));
            if (to > offset) {
                break;
            }
            rangeIndex++;
        }
        const visible = parts.join("");
        return visible ? [{ ...block, text: visible }] : [];
    });
};

export const filterHistoryMessages = (messages, hiddenPromptTexts = []) => messages.flatMap((message) => {
    const content = [];
    let values = [];
    if (typeof message.content === "string") {
        values = [{ type: "text", text: message.content }];
    } else if (Array.isArray(message.content)) {
        values = message.content;
    }
    if (!values.length) {
        return [message];
    }
    // Coalesce replay fragments before matching: a URI or its body can span updates.
    for (const value of values) {
        const block = typeof value === "string" ? { type: "text", text: value } : value;
        if (!block || isHiddenResource(block)) {
            continue;
        }
        const previous = content.at(-1);
        if (["text", "thinking"].includes(block.type) && previous?.type === block.type
            && (!block.id || !previous.id || block.id === previous.id)) {
            previous.text = (previous.text ?? "") + (block.text ?? "");
        } else {
            content.push({ ...block });
        }
    }
    const filtered = [];
    for (let index = 0; index < content.length;) {
        const block = content[index];
        if (!["text", "thinking"].includes(block.type)) {
            filtered.push(block);
            index++;
            continue;
        }
        let end = index + 1;
        while (end < content.length && content[end].type === block.type) {
            end++;
        }
        // Content IDs identify blocks, not hidden-payload boundaries. Match the full
        // textual run, then project visible ranges back onto the original blocks.
        filtered.push(...filterTextBlocks(content.slice(index, end), hiddenPromptTexts));
        index = end;
    }
    return filtered.length ? [{ ...message, content: filtered }] : [];
});

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
import { TOOL_ERROR_LIMITS } from "./toolErrorPolicy.mjs";

const CREDENTIAL_QUOTE_SOURCE = String.raw`(\b(?:api[_-]?key|access[_-]?token|refresh[_-]?token|token|secret|password|authorization|claimToken|connectionToken)\b["']?\s*[:=]\s*)("|')`;
// A tail sample may start inside a quoted credential value whose label was cut off. The label
// can sit anywhere before the window, so the whole capture (bounded upstream) is scanned once
// to decide whether the quote opened before the window is still open where the sample starts.
// A fresh RegExp per call keeps the scan stateless: a module-level /g regex would carry
// lastIndex across diagnostics and skip labels in the next capture.
const credentialQuoteOpenAt = (text, index) => {
    const label = new RegExp(CREDENTIAL_QUOTE_SOURCE, "gi");
    let last;
    let match;
    while ((match = label.exec(text)) !== null && match.index < index) {
        last = { start: match.index + match[1].length, quote: match[2] };
    }
    if (!last) return false;
    for (let i = last.start + 1; i < index; i += 1) {
        if (text[i] === "\\") {
            i += 1;
        } else if (text[i] === last.quote) {
            return false;
        }
    }
    return true;
};

export const readDiagnosticProperty = (value, key) => {
    if (value === null || !["object", "function"].includes(typeof value)) return undefined;
    try {
        const descriptor = Object.getOwnPropertyDescriptor(value, key);
        return descriptor && Object.hasOwn(descriptor, "value") ? descriptor.value : undefined;
    } catch {
        return undefined;
    }
};

export const sliceDiagnosticText = (value, limit, side = "head") => {
    if (typeof value !== "string" || limit <= 0) return "";
    if (value.length <= limit) return value;
    let text = side === "tail" ? value.slice(-limit) : value.slice(0, limit);
    if (side === "tail" && /[\uDC00-\uDFFF]/.test(text[0] ?? "")) text = text.slice(1);
    if (side !== "tail" && /[\uD800-\uDBFF]/.test(text.at(-1) ?? "")) text = text.slice(0, -1);
    return text;
};

export const redactDiagnosticText = (value) => {
    if (typeof value !== "string") return "";
    const text = sliceDiagnosticText(value, TOOL_ERROR_LIMITS.inputText);
    if (/<\s*(?:!doctype|html|head|body|script)(?:\s|>)/i.test(text)) return "[HTML response omitted]";
    return text
        .replace(/\u001b(?:\[[0-?]*[ -/]*[@-~]|\][^\u0007]*(?:\u0007|$))/g, "")
        .replace(/[\u0000-\u0008\u000B-\u001F\u007F]/g, "")
        .replace(/\bBearer\s+[A-Za-z0-9._~+/-]+=*/gi, "Bearer [REDACTED]")
        // Whole auth values: a scheme word (Basic/Digest/...) is only the first token, so the
        // generic key/value rule below would leave the encoded credential behind.
        .replace(/(\b(?:authorization|proxy-authorization)\b["']?\s*[:=]\s*)(?:basic|bearer|digest|ntlm)\s+[A-Za-z0-9._~+/=-]+/gi, "$1[REDACTED]")
        .replace(/(\b[a-z][a-z0-9+.-]*:\/\/)[^/\s@]+@/gi, "$1[REDACTED]@")
        // Sampling can remove a closing quote or leave a trailing escape.
        .replace(/(\b(?:api[_-]?key|access[_-]?token|refresh[_-]?token|token|secret|password|authorization|claimToken|connectionToken)\b["']?\s*[:=]\s*)("(?:\\[\s\S]|[^"\\])*(?:"|\\?$)|'(?:\\[\s\S]|[^'\\])*(?:'|\\?$))/gi, '$1"[REDACTED]"')
        .replace(/(\b(?:api[_-]?key|access[_-]?token|refresh[_-]?token|token|secret|password|authorization|claimToken|connectionToken)\b["']?\s*[:=]\s*)(?!["']|\[REDACTED\])[^\s"'&,}\]]+/gi, "$1[REDACTED]")
        .replace(/\b(?:sk|ghp|github_pat)[-_][A-Za-z0-9_-]{12,}/g, "[REDACTED]")
        .replace(/\bfile:\/\/[^\s"'<>]+/gi, "[path]")
        .replace(/(["'])(?:[A-Za-z]:[\\/]|\/(?!\/))[^"'<>\r\n]+\1/g, "[path]")
        .replace(/\b[A-Za-z]:[\\/](?:Users|Documents and Settings)[\\/][^\s"'<>]+/gi, "[user path]")
        .replace(/\/(?:home|Users)\/[^\s"'<>]+/g, "[user path]")
        .replace(/\b[A-Za-z]:[\\/][^\s"'<>]+/g, "[path]")
        .replace(/\\\\[^\\/\s"'<>]+[\\/][^\s"'<>]+/g, "[path]")
        .replace(/(^|[\s"'(=])\/(?!\/)[^\s"'<>),]+/g, "$1[path]");
};

export const boundedDiagnosticText = (value, limit, side = "head") => {
    if (typeof value !== "string") return undefined;
    // Bound processing before redaction; only the captured fragment can be reported.
    let sampled = sliceDiagnosticText(value, TOOL_ERROR_LIMITS.inputText, side);
    if (side === "tail" && value.length > sampled.length
        && credentialQuoteOpenAt(value, value.length - sampled.length)) {
        // The sampled tail starts inside a quoted credential value; the label may be lost.
        return "[Diagnostic fragment omitted]";
    }
    if (side === "tail" && value.length > sampled.length && value[value.length - sampled.length - 1] !== "\n") {
        // A partial line may have lost its credential label; do not expose it.
        const firstLineEnd = sampled.indexOf("\n");
        sampled = firstLineEnd < 0 ? "[Diagnostic fragment omitted]" : sampled.slice(firstLineEnd + 1);
    }
    return sliceDiagnosticText(redactDiagnosticText(sampled), limit, side);
};

export const markDiagnosticTruncation = (record, field) => {
    if (!record.truncation) record.truncation = Object.create(null);
    record.truncation[field] = true;
};

// Only accepts records constructed by the projector, never an untrusted input.
export const fitDiagnosticRecord = (record, limit, essential = []) => {
    const length = () => JSON.stringify(record).length;
    for (const field of ["stdout", "availableSkills", "stderr"]) {
        if (length() <= limit || record[field] === undefined) continue;
        markDiagnosticTruncation(record, field);
        if (Array.isArray(record[field])) {
            while (record[field].length && length() > limit) record[field].pop();
            continue;
        }
        const original = record[field];
        let low = 0;
        let high = original.length;
        while (low < high) {
            const middle = Math.ceil((low + high) / 2);
            record[field] = sliceDiagnosticText(original, middle, field === "stderr" ? "tail" : "head");
            if (length() <= limit) low = middle;
            else high = middle - 1;
        }
        record[field] = sliceDiagnosticText(original, low, field === "stderr" ? "tail" : "head");
    }
    for (const field of Object.keys(record).reverse()) {
        if (length() <= limit) break;
        if (essential.includes(field) || field === "truncation") continue;
        delete record[field];
        markDiagnosticTruncation(record, "fieldsOmitted");
    }
    return record;
};

export const freezeDiagnosticValue = (value) => {
    if (value && typeof value === "object") {
        for (const child of Object.values(value)) freezeDiagnosticValue(child);
        Object.freeze(value);
    }
    return value;
};

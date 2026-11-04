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
import {
    TOOL_ERROR_LIMITS,
    TOOL_ERROR_OUTCOMES,
    TOOL_ERROR_RETRY_ADVICE,
    TOOL_ERROR_SOURCES,
    TOOL_ERROR_STAGES,
    isKnownCauseCode,
    isKnownToolErrorCode,
    toolErrorPolicy,
} from "./toolErrorPolicy.mjs";
import {
    boundedDiagnosticText,
    fitDiagnosticRecord,
    freezeDiagnosticValue,
    markDiagnosticTruncation,
    readDiagnosticProperty as read,
    sliceDiagnosticText,
} from "./toolErrorBudget.mjs";

const safeShapes = new WeakSet();
const errorShapes = new WeakMap();
const modelErrors = new WeakMap();
const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
export const isToolDiagnosticId = (value) => typeof value === "string" && UUID.test(value);
const identifier = (value, limit = 128) => {
    if (typeof value !== "string" || value.length > limit || !/^[\w.-]+$/.test(value)) return undefined;
    return boundedDiagnosticText(value, limit) === value ? value : undefined;
};
const enumValue = (value, allowed, fallback) => allowed.includes(value) ? value : fallback;
const isObject = (value) => value !== null && ["object", "function"].includes(typeof value);
const cachedShape = (value) => isObject(value) ? errorShapes.get(value) : undefined;

const errorName = (value) => {
    const name = read(value, "name");
    if (["AbortError", "TimeoutError"].includes(name)) return name;
    try {
        if (value instanceof DOMException) {
            return Object.getOwnPropertyDescriptor(DOMException.prototype, "name").get.call(value);
        }
    } catch {
        // Proxies and foreign objects are not trusted native exceptions.
    }
    return "Error";
};
const errorMessage = (value) => {
    const message = read(value, "message");
    if (typeof message === "string") return message;
    if (typeof value === "string") return value;
    try {
        if (value instanceof DOMException) {
            return Object.getOwnPropertyDescriptor(DOMException.prototype, "message").get.call(value);
        }
    } catch {
        // Never invoke an arbitrary accessor or toString as a fallback.
    }
    return "Unknown tool error";
};

const normalizeContext = (context) => {
    const diagnosticId = read(context, "diagnosticId");
    return {
        diagnosticId: isToolDiagnosticId(diagnosticId) ? diagnosticId : "unassigned",
        tool: identifier(read(context, "toolName")) ?? "unknown",
        source: enumValue(read(context, "source"), TOOL_ERROR_SOURCES, "runtime"),
        stage: enumValue(read(context, "stage"), TOOL_ERROR_STAGES, "execute"),
        outcome: enumValue(read(context, "outcome"), TOOL_ERROR_OUTCOMES, "unknown"),
        retryAdvice: enumValue(read(context, "retryAdvice"), TOOL_ERROR_RETRY_ADVICE, "do_not_retry"),
    };
};

const projectField = (field, value, policy) => {
    if (field === "stderr" || field === "stdout") {
        return boundedDiagnosticText(value, TOOL_ERROR_LIMITS[field], field === "stderr" ? "tail" : "head");
    }
    if (field === "reason") {
        if (policy.reasonValues.includes(value)) return value;
        return policy.reasonFallback;
    }
    if (field === "exitCode") return value === null || Number.isSafeInteger(value) ? value : undefined;
    if (field === "sliceIndex") return value === null || (Number.isSafeInteger(value) && value >= 0) ? value : undefined;
    if (field === "phase") return ["loading", "parsing", "rendering", "ready", "busy"].includes(value) ? value : undefined;
    if (field === "signal") return typeof value === "string" && /^SIG[A-Z0-9]{1,20}$/.test(value) ? value : undefined;
    if (field === "causeCode") return isKnownCauseCode(value) ? value : undefined;
    if (["timeoutMs", "limitBytes", "total", "status", "httpStatus", "eventId", "expectedRevision", "currentRevision", "requestedPage", "maxPage", "pageSize", "page", "rowCount"].includes(field)) {
        return Number.isSafeInteger(value) && value >= 0 ? value : undefined;
    }
    if (field === "contentType") {
        if (typeof value !== "string") return undefined;
        const type = value.slice(0, 160).split(";")[0].trim().toLowerCase();
        return ["application/json", "application/problem+json", "text/plain", "text/html", "application/octet-stream"].includes(type) ? type : "unknown";
    }
    if (field === "backendCode") return Number.isSafeInteger(value) ? value : identifier(value, 80);
    if (field === "query") {
        const query = Object.create(null);
        for (const key of ["page", "pageSize"]) {
            const projected = projectField(key, read(value, key), policy);
            if (projected !== undefined) query[key] = projected;
        }
        return Object.keys(query).length ? query : undefined;
    }
    if (field === "availableSkills") {
        const result = [];
        const length = read(value, "length");
        if (!Number.isSafeInteger(length) || length < 0) return undefined;
        for (let i = 0; i < Math.min(length, TOOL_ERROR_LIMITS.skills); i++) {
            const name = identifier(read(value, String(i)), TOOL_ERROR_LIMITS.skillName);
            if (name) result.push(name);
        }
        return result;
    }
    return value === null ? null : identifier(value);
};

const projectDetails = (value, policy, context) => {
    const details = Object.assign(Object.create(null), context);
    for (const field of policy.fields) {
        const raw = read(value, field);
        const projected = projectField(field, raw, policy);
        if (projected !== undefined) details[field] = projected;
        if (["stderr", "stdout"].includes(field) && typeof raw === "string" && raw.length > TOOL_ERROR_LIMITS[field]) {
            markDiagnosticTruncation(details, field);
        }
        const arrayLength = field === "availableSkills" ? read(raw, "length") : undefined;
        if (Number.isSafeInteger(arrayLength) && arrayLength > TOOL_ERROR_LIMITS.skills) {
            markDiagnosticTruncation(details, field);
        }
    }
    for (const field of ["stderr", "stdout", "message", "availableSkills", "captureLimited", "responseBody", "stateOmitted", "fieldsOmitted"]) {
        if (read(read(value, "truncation"), field) === true) markDiagnosticTruncation(details, field);
    }
    if (read(value, "captureLimited") === true) markDiagnosticTruncation(details, "captureLimited");
    return details;
};

const projectState = (state, policy, details) => {
    if (state === undefined) return undefined;
    const output = Object.create(null);
    for (const field of policy.state) {
        const value = read(state, field);
        const projected = field === "ready" && typeof value === "boolean" ? value : projectField(field, value, policy);
        if (projected !== undefined) output[field] = projected;
        if (field === "query" && (read(value, "filters") !== undefined || read(value, "sort") !== undefined)) {
            markDiagnosticTruncation(details, "stateOmitted");
        }
    }
    if (!Object.keys(output).length) {
        markDiagnosticTruncation(details, "stateOmitted");
        return undefined;
    }
    fitDiagnosticRecord(output, TOOL_ERROR_LIMITS.state);
    if (!Object.keys(output).length || output.truncation) {
        markDiagnosticTruncation(details, "stateOmitted");
        delete output.truncation;
    }
    return Object.keys(output).length ? output : undefined;
};

const causeCode = (value) => {
    let cause = value;
    for (let depth = 0; depth < TOOL_ERROR_LIMITS.causeDepth; depth++) {
        const code = read(cause, "code");
        if (isKnownCauseCode(code)) return code;
        cause = read(cause, "cause");
    }
    return undefined;
};

export const normalizeToolError = (value, context) => {
    const cached = cachedShape(value);
    if (context === undefined && cached) return cached;
    if (context === undefined && safeShapes.has(value)) return value;
    const input = cached ?? value;
    const code = read(input, "code");
    const name = errorName(value);
    const normalizedCode = isKnownToolErrorCode(code) ? code : name === "AbortError" ? "TOOL_CANCELLED" : "TOOL_EXECUTION_FAILED";
    const policy = toolErrorPolicy(normalizedCode);
    const normalizedContext = normalizeContext(context);
    const details = projectDetails(read(input, "details"), policy, normalizedContext);
    const knownCause = causeCode(value);
    if (knownCause && policy.fields.includes("causeCode")) details.causeCode = knownCause;
    const state = projectState(read(input, "state"), policy, details);
    const originalMessage = policy.message ?? errorMessage(input);
    const message = boundedDiagnosticText(originalMessage, TOOL_ERROR_LIMITS.message)?.trim() || "Unknown tool error";
    if (originalMessage.length > TOOL_ERROR_LIMITS.message) markDiagnosticTruncation(details, "message");
    fitDiagnosticRecord(details, TOOL_ERROR_LIMITS.details, [
        "diagnosticId", "tool", "source", "stage", "outcome", "retryAdvice", "reason",
    ]);
    const shape = {
        code: normalizedCode,
        message,
        retryable: policy.retry && read(input, "retryable") === true && normalizedContext.outcome === "not_started",
        details,
        ...(state === undefined ? {} : { state }),
    };
    safeShapes.add(shape);
    return freezeDiagnosticValue(shape);
};

// Only our private cache can supply inherited execution facts, not arbitrary details.
export const inheritToolErrorContext = (value, fallback) => {
    const context = normalizeContext(fallback);
    const result = { ...context, toolName: context.tool };
    delete result.tool;
    const previous = cachedShape(value);
    if (previous?.details.diagnosticId === context.diagnosticId) {
        for (const field of ["source", "stage", "outcome", "retryAdvice"]) result[field] = previous.details[field];
    }
    return result;
};

export const formatToolError = (value) => {
    const shape = safeShapes.has(value) ? value : normalizeToolError(value);
    const text = JSON.stringify(shape);
    if (text.length <= TOOL_ERROR_LIMITS.text) return text;
    // JSON escaping can expand malformed Unicode; trim the message, not process diagnostics.
    const output = { ...shape, details: { ...shape.details, truncation: { ...shape.details.truncation, message: true } } };
    fitDiagnosticRecord(output.details, TOOL_ERROR_LIMITS.details, [
        "diagnosticId", "tool", "source", "stage", "outcome", "retryAdvice", "reason",
    ]);
    let low = 0;
    let high = shape.message.length;
    while (low < high) {
        const middle = Math.ceil((low + high) / 2);
        output.message = sliceDiagnosticText(shape.message, middle);
        if (JSON.stringify(output).length <= TOOL_ERROR_LIMITS.text) low = middle;
        else high = middle - 1;
    }
    output.message = sliceDiagnosticText(shape.message, low);
    return JSON.stringify(output);
};

export const createToolError = (value, cause) => {
    const shape = safeShapes.has(value) ? value : normalizeToolError(value);
    const error = Object.assign(new Error(shape.message, cause === undefined ? undefined : { cause }), shape);
    error.name = errorName(cause);
    Object.defineProperty(error, "toJSON", { value: () => shape });
    errorShapes.set(error, shape);
    return error;
};

export const toModelFacingToolError = (value, context) => {
    const normalizedContext = normalizeContext(context);
    const previous = isObject(value) ? modelErrors.get(value) : undefined;
    if (previous && Object.entries(normalizedContext).every(([key, field]) => previous.details[key] === field)) return value;
    const shape = normalizeToolError(value, context);
    const error = createToolError(shape, value);
    error.message = formatToolError(shape);
    error.name = errorName(value);
    modelErrors.set(error, shape);
    return error;
};

export const buildProcessDetails = (value, context) => {
    const source = Object.create(null);
    for (const field of ["exitCode", "signal", "stdout", "stderr", "timeoutMs", "limitBytes", "causeCode", "captureLimited"]) {
        const fieldValue = read(value, field);
        if (fieldValue !== undefined) source[field] = fieldValue;
    }
    const details = projectDetails(source, toolErrorPolicy("CLI_EXIT_NONZERO"), normalizeContext(context));
    return freezeDiagnosticValue(fitDiagnosticRecord(details, TOOL_ERROR_LIMITS.details, [
        "diagnosticId", "tool", "source", "stage", "outcome", "retryAdvice",
    ]));
};

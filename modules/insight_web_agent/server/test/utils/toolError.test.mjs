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
import assert from "node:assert/strict";
import test from "node:test";
import { readFile } from "node:fs/promises";
import {
    buildProcessDetails,
    createToolError,
    formatToolError,
    normalizeToolError,
    toModelFacingToolError,
    inheritToolErrorContext,
} from "../../utils/toolError.mjs";
import { isKnownToolErrorCode, TOOL_ERROR_LIMITS, toolErrorPolicy } from "../../utils/toolErrorPolicy.mjs";

const context = Object.freeze({
    diagnosticId: "00000000-0000-4000-8000-000000000001",
    toolName: "pt_snap",
    source: "cli",
    stage: "execute",
    outcome: "failed",
    retryAdvice: "correct_input",
});
const failure = (overrides = {}) => Object.assign(new Error("Query failed."), {
    code: "CLI_EXIT_NONZERO",
    retryable: false,
    details: { exitCode: 2, stderr: "missing required table TASK" },
    ...overrides,
});

test("structured errors preserve their code, permitted details and trusted context", () => {
    const shape = normalizeToolError(failure(), context);
    assert.equal(shape.code, "CLI_EXIT_NONZERO");
    assert.equal(shape.message, "Query failed.");
    assert.equal(shape.retryable, false);
    assert.equal(shape.details.exitCode, 2);
    assert.match(shape.details.stderr, /required table TASK/);
    assert.equal(shape.details.diagnosticId, context.diagnosticId);
    assert.equal(shape.details.source, "cli");
    assert.ok(Object.isFrozen(shape));
    assert.ok(Object.isFrozen(shape.details));
});

test("error text uses the existing JSON contract without a separate text template", () => {
    const shape = normalizeToolError(failure(), context);
    const formatted = formatToolError(shape);
    assert.deepEqual(JSON.parse(formatted), JSON.parse(JSON.stringify(shape)));
    assert.deepEqual(Object.keys(JSON.parse(formatted)).sort(), ["code", "details", "message", "retryable"]);
    assert.doesNotMatch(formatted, /TOOL_ERROR v=|^tool=/);
    assert.equal(JSON.parse(formatted).details.stderr, "missing required table TASK");
    assert.equal(JSON.parse(formatted).details.diagnosticId, context.diagnosticId);
});

test("JSON error text preserves optional state and remains parseable within its budget", () => {
    const shape = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: '"\\'.repeat(600),
        details: { reason: "module_not_active", targetModule: "Timeline" },
        state: { activeModule: "Compute", ready: false },
    }, { ...context, source: "command", outcome: "not_started", retryAdvice: "refresh_context" });
    const formatted = formatToolError(shape);
    assert.ok(formatted.length <= TOOL_ERROR_LIMITS.text);
    assert.deepEqual(JSON.parse(formatted), JSON.parse(JSON.stringify(shape)));
    assert.deepEqual(Object.keys(JSON.parse(formatted)).sort(), ["code", "details", "message", "retryable", "state"]);
});

test("JSON escaping cannot push process diagnostics outside the error text budget", () => {
    const shape = normalizeToolError(failure({
        message: "\uD800".repeat(600),
        details: { exitCode: 2, stderr: "x".repeat(1400) + "\nSPECIFIC_END" },
    }), context);
    assert.ok(JSON.stringify(shape).length > TOOL_ERROR_LIMITS.text);
    const formatted = formatToolError(shape);
    const parsed = JSON.parse(formatted);
    assert.ok(formatted.length <= TOOL_ERROR_LIMITS.text);
    assert.match(parsed.details.stderr, /SPECIFIC_END$/);
    assert.equal(parsed.details.diagnosticId, context.diagnosticId);
    assert.equal(parsed.details.truncation.message, true);
    assert.ok(JSON.stringify(parsed.details).length <= TOOL_ERROR_LIMITS.details);
});

for (const value of [
    { message: "Plain object failure." },
    "String failure.",
    undefined,
    null,
    17,
]) test(`unknown errors stay readable: ${typeof value}`, () => {
    const shape = normalizeToolError(value, context);
    assert.equal(shape.code, "TOOL_EXECUTION_FAILED");
    assert.equal(shape.retryable, false);
    assert.ok(shape.message);
    assert.notEqual(shape.message, "[object Object]");
    if (value?.message) assert.equal(shape.message, value.message);
});

test("truthy strings cannot authorize retries and user cancellation cannot be retryable", () => {
    assert.equal(normalizeToolError(failure({ retryable: "false" }), context).retryable, false);
    assert.equal(normalizeToolError(failure({ code: "COMMAND_CANCELLED", retryable: true }), {
        ...context, source: "command", outcome: "unknown", retryAdvice: "inspect_before_retry",
    }).retryable, false);
    assert.equal(normalizeToolError(failure({ code: "CLI_BUSY", retryable: true }), {
        ...context, outcome: "not_started", retryAdvice: "retry_after_ready",
    }).retryable, true);
});

test("frozen Error and readonly DOMException are not mutated by model projection", () => {
    const original = Object.freeze(failure());
    const projected = toModelFacingToolError(original, context);
    assert.notEqual(projected, original);
    assert.equal(original.message, "Query failed.");
    assert.equal(projected.cause, original);
    assert.match(projected.message, /CLI_EXIT_NONZERO/);
    assert.match(projected.message, /required table TASK/);
    const abort = new DOMException("Stopped by caller.", "AbortError");
    const cancelled = toModelFacingToolError(abort, {
        ...context, source: "runtime", outcome: "unknown", retryAdvice: "do_not_retry",
    });
    assert.equal(cancelled.name, "AbortError");
    assert.equal(cancelled.code, "TOOL_CANCELLED");
    assert.equal(cancelled.retryable, false);
    assert.equal(abort.message, "Stopped by caller.");
    const contractAbort = createToolError(normalizeToolError(abort, context), abort);
    assert.equal(contractAbort.name, "AbortError");
});

test("model projection is idempotent within a call and can be recontextualized", () => {
    const first = toModelFacingToolError(failure(), context);
    assert.equal(toModelFacingToolError(first, context), first);
    const second = toModelFacingToolError(first, {
        ...context, diagnosticId: "00000000-0000-4000-8000-000000000002",
    });
    assert.notEqual(second, first);
    assert.equal(JSON.parse(second.message).message, "Query failed.");
    assert.deepEqual(JSON.parse(second.message), JSON.parse(JSON.stringify(second)));
    assert.ok(!second.message.includes(context.diagnosticId));
    assert.equal(first.details.diagnosticId, context.diagnosticId);
    const changedStage = toModelFacingToolError(first, { ...context, stage: "respond", outcome: "unknown" });
    assert.notEqual(changedStage, first);
    assert.equal(changedStage.details.stage, "respond");
    assert.equal(changedStage.details.outcome, "unknown");
});

test("contract Error serializes safe fields without stack or original cause", () => {
    const original = new Error("private original cause");
    const shape = normalizeToolError(failure(), context);
    const error = createToolError(shape, original);
    assert.equal(error.message, shape.message);
    assert.equal(error.cause, original);
    const encoded = JSON.stringify(error);
    assert.ok(!encoded.includes("private original cause"));
    assert.ok(!encoded.includes("stack"));
    assert.equal(JSON.parse(encoded).code, "CLI_EXIT_NONZERO");
});

test("untrusted accessors and object serialization methods are never invoked", () => {
    let invoked = 0;
    const value = {
        code: "CLI_EXIT_NONZERO",
        get message() { invoked++; throw new Error("getter"); },
        details: {
            get stderr() { invoked++; return "getter"; },
            toJSON() { invoked++; return { secret: "do not serialize" }; },
        },
        toString() { invoked++; return "do not stringify"; },
    };
    const shape = normalizeToolError(value, context);
    assert.equal(invoked, 0);
    assert.ok(shape.message);
    assert.doesNotThrow(() => JSON.stringify(shape));
    const hostileLength = { valueOf() { invoked++; return 100; } };
    normalizeToolError({
        code: "SKILL_UNAVAILABLE", message: "Missing.",
        details: { availableSkills: { length: hostileLength } },
    }, { ...context, source: "skill" });
    assert.equal(invoked, 0);
});

test("throwing and revoked proxies fail closed instead of hiding the original failure", () => {
    const proxy = new Proxy({}, { getOwnPropertyDescriptor() { throw new Error("trap"); } });
    const { proxy: revoked, revoke } = Proxy.revocable({}, {});
    revoke();
    for (const value of [proxy, revoked]) {
        assert.doesNotThrow(() => formatToolError(normalizeToolError(value, context)));
    }
});

test("trusted context wins and unknown diagnostic properties are dropped", () => {
    const shape = normalizeToolError(failure({
        details: {
            diagnosticId: "spoofed",
            source: "spoofed",
            outcome: "not_started",
            stderr: "specific failure",
            authorization: "fixture-secret",
            claimToken: "fixture-secret",
            arbitrary: "fixture-secret",
        },
    }), context);
    assert.equal(shape.details.diagnosticId, context.diagnosticId);
    assert.equal(shape.details.source, context.source);
    assert.equal(shape.details.outcome, "failed");
    assert.equal(shape.details.arbitrary, undefined);
    assert.ok(!JSON.stringify(shape).includes("fixture-secret"));
});

test("known command context survives but unregistered state is explicitly omitted", () => {
    const shape = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: "Module unavailable.", retryable: true,
        details: { reason: "module_not_active", targetModule: "Timeline", activeModule: "Compute" },
        state: { activeModule: "Compute", ready: false, claimToken: "fixture-secret" },
    }, { ...context, source: "command", outcome: "not_started", retryAdvice: "refresh_context" });
    assert.equal(shape.details.reason, "module_not_active");
    assert.equal(shape.state.activeModule, "Compute");
    assert.equal(shape.state.claimToken, undefined);
    const unknown = normalizeToolError({ message: "unknown", state: { private: "fixture-secret" } }, context);
    assert.equal(unknown.state, undefined);
    assert.equal(unknown.details.truncation.stateOmitted, true);
});

test("reason syntax alone is insufficient and RAG never forwards raw dependency text", () => {
    const shape = normalizeToolError({
        code: "RAG_RETRIEVAL_FAILED",
        message: "fixture_private_resource_name",
        details: { reason: "fixture_private_resource_name" },
    }, { ...context, source: "rag", stage: "retrieve" });
    assert.equal(shape.details.reason, "retrieval_failed");
    assert.ok(!formatToolError(shape).includes("fixture_private_resource_name"));
    const known = normalizeToolError({
        code: "RAG_RETRIEVAL_FAILED", message: "private",
        details: { reason: "embedding_unavailable" },
    }, { ...context, source: "rag", stage: "retrieve" });
    assert.equal(known.details.reason, "embedding_unavailable");
});

test("process diagnostics preserve null exit, tails and capture-limit facts", () => {
    const details = buildProcessDetails({
        exitCode: null, signal: "SIGTERM",
        stderr: `${"x".repeat(10000)}\nSPECIFIC_END`,
        stdout: "y".repeat(1000),
        captureLimited: true,
    }, context);
    assert.equal(details.exitCode, null);
    assert.equal(details.signal, "SIGTERM");
    assert.match(details.stderr, /SPECIFIC_END$/);
    assert.equal(details.truncation.stderr, true);
    assert.equal(details.truncation.captureLimited, true);
    assert.ok(JSON.stringify(details).length <= TOOL_ERROR_LIMITS.details);
});

test("model errors keep the same five-field contract when inspected as JSON", () => {
    const shape = normalizeToolError(failure(), context);
    const projected = toModelFacingToolError(createToolError(shape), context);
    const stored = JSON.parse(JSON.stringify(projected));
    assert.equal(stored.message, shape.message);
    assert.equal(stored.code, shape.code);
    assert.deepEqual(Object.keys(stored).sort(), ["code", "details", "message", "retryable"]);
});

test("only known cause codes are extracted without exposing dependency messages", () => {
    const original = Object.assign(new Error("request failed"), {
        code: "CAPABILITY_REQUEST_FAILED",
        cause: Object.assign(new Error("fixture_private_dependency"), { code: "ECONNREFUSED" }),
    });
    const shape = normalizeToolError(original, { ...context, source: "capability_http", stage: "dispatch", outcome: "unknown" });
    assert.equal(shape.details.causeCode, "ECONNREFUSED");
    assert.ok(!formatToolError(shape).includes("fixture_private_dependency"));
});

test("skill candidates and state projection stay independently bounded", () => {
    const shape = normalizeToolError({
        code: "SKILL_UNAVAILABLE", message: "Skill missing.",
        details: { name: "missing", total: 100, availableSkills: Array.from({ length: 100 }, (_, i) => `skill-${i}`) },
    }, { ...context, source: "skill" });
    assert.equal(shape.details.availableSkills.length, 20);
    assert.equal(shape.details.truncation.availableSkills, true);
    assert.equal(shape.details.total, 100);
    const withState = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: "Unavailable.",
        state: { activeModule: "a".repeat(128), targetModule: "b".repeat(128), moduleId: "c".repeat(128), ready: false },
    }, { ...context, source: "command" });
    assert.ok(JSON.stringify(withState.state).length <= TOOL_ERROR_LIMITS.state);
    assert.equal(withState.details.truncation.stateOmitted, true);
});

test("boundary normalization preserves private safe facts, not arbitrary producer metadata", () => {
    const shape = normalizeToolError(failure(), context);
    const error = createToolError(shape);
    assert.equal(normalizeToolError(error), shape);
    const unknown = { ...context, outcome: "unknown", source: "runtime" };
    assert.equal(inheritToolErrorContext(error, unknown).outcome, "failed");
    assert.equal(inheritToolErrorContext({ details: { diagnosticId: context.diagnosticId, outcome: "not_started" } }, unknown).outcome, "unknown");
});

test("all existing frontend and table error codes have explicit policies", async () => {
    for (const file of ["FrontendAgentCommand/errors.ts", "AgentTable/errors.ts"]) {
        const source = await readFile(new URL(`../../../../lib/src/${file}`, import.meta.url), "utf8");
        for (const match of source.matchAll(/'((?:COMMAND|TABLE)_[A-Z_]+)'/g)) {
            assert.ok(isKnownToolErrorCode(match[1]), match[1]);
        }
    }
});

test("table policies keep their intended retry semantics after deduplication", () => {
    for (const code of ["TABLE_TARGET_UNAVAILABLE", "TABLE_NOT_READY", "TABLE_BUSY", "TABLE_STATE_STALE",
        "TABLE_PAGE_OUT_OF_RANGE", "TABLE_COMMAND_SUPERSEDED", "TABLE_COMMAND_TIMEOUT", "TABLE_CONNECTION_LOST"]) {
        assert.equal(toolErrorPolicy(code).retry, true, code);
    }
    for (const code of ["TABLE_CAPABILITY_UNSUPPORTED", "TABLE_COMMAND_INVALID", "TABLE_COLUMN_UNKNOWN",
        "TABLE_ROW_UNKNOWN", "TABLE_TRANSITION_FAILED", "TABLE_COPY_FAILED", "TABLE_COMMAND_CANCELLED"]) {
        assert.equal(toolErrorPolicy(code).retry, false, code);
    }
});
test("existing table revision and memory-snapshot detail fields remain diagnosable", () => {
    const table = normalizeToolError({
        code: "TABLE_STATE_STALE", message: "Table state changed.", retryable: true,
        details: { expectedRevision: 3, currentRevision: 4 },
        state: { query: { page: 2, pageSize: 10, filters: [] }, total: 30, rowCount: 10 },
    }, { ...context, source: "command", outcome: "not_started", retryAdvice: "refresh_context" });
    assert.equal(table.code, "TABLE_STATE_STALE");
    assert.equal(table.details.expectedRevision, 3);
    assert.equal(table.details.currentRevision, 4);
    assert.equal(table.state.query.page, 2);
    assert.equal(table.state.total, 30);
    const memory = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: "Snapshot not ready.",
        details: { eventId: 5, deviceId: "0", sliceIndex: null, phase: "loading", reason: "context_changed" },
    }, { ...context, source: "command" });
    assert.equal(memory.details.eventId, 5);
    assert.equal(memory.details.deviceId, "0");
    assert.equal(memory.details.sliceIndex, null);
    assert.equal(memory.details.phase, "loading");
    assert.equal(memory.details.reason, "context_changed");
});

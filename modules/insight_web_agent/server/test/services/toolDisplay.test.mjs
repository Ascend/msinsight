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
import { test } from "node:test";
import { createChatService, normalizeToolCall } from "../../services/chatService.mjs";
import { createAcpSessionNotifier } from "../../native-agent/acp/sessionUpdates.mjs";
import { processToolDisplay } from "../../utils/toolDisplay.mjs";
import { createRuntimeState, getSessionContext } from "../../state/runtimeState.mjs";
import { buildSessionExport } from "../../services/sessionExport.mjs";
import { formatToolError, normalizeToolError } from "../../utils/toolError.mjs";

test("native JSON error contract survives ACP text projection without breaking escaped fields", () => {
    for (const message of ["Query failed.", 'password="fixture-private-secret"']) {
        const shape = normalizeToolError({
            code: "CLI_EXIT_NONZERO", message,
            details: { exitCode: 2, stderr: 'missing TASK; password="fixture-private-secret"' },
        }, {
            diagnosticId: "00000000-0000-4000-8000-000000000001",
            toolName: "pt_snap", source: "cli", stage: "execute",
            outcome: "failed", retryAdvice: "inspect_before_retry",
        });
        const call = normalizeToolCall({
            toolCallId: "json-contract", name: "pt_snap", status: "failed",
            rawOutput: formatToolError(shape),
        }, undefined, { nativeErrorContract: true });
        assert.deepEqual(JSON.parse(call.output), JSON.parse(JSON.stringify(shape)));
        assert.doesNotMatch(call.output, /fixture-private-secret/);
    }
});

test("native ACP, Host messages and exports retain the same JSON error contract", () => {
    const state = createRuntimeState();
    state.activeAgentName = "msinsight-native";
    getSessionContext(state, "json-session").pendingPrompt = true;
    const events = [];
    const chat = createChatService({ state, eventBus: { broadcast: (event) => events.push(event) } });
    const shape = normalizeToolError({
        code: "SKILL_UNAVAILABLE", message: 'Missing skill; password="fixture-private-secret"',
        details: { name: "missing", availableSkills: Array.from({ length: 30 }, (_, i) => `skill-${i}`), total: 30 },
    }, {
        diagnosticId: "00000000-0000-4000-8000-000000000001",
        toolName: "skill", source: "skill", stage: "validate",
        outcome: "not_started", retryAdvice: "correct_input",
    });
    const output = formatToolError(shape);
    const notifier = createAcpSessionNotifier({
        write: (line) => chat.handleAcpNotification(JSON.parse(line)),
    });
    notifier.sendToolCallUpdate("json-session", "tool_call_update", {
        toolCallId: "json-call", name: "skill", status: "failed", output,
    });
    const messages = state.sessionContexts.get("json-session").messages;
    assert.deepEqual(JSON.parse(messages[0].content[0].toolCall.output), JSON.parse(output));
    const exported = buildSessionExport({ sessionId: "json-session", messages });
    assert.deepEqual(JSON.parse(exported.messages[0].content[0].toolCall.output), JSON.parse(output));
    assert.doesNotMatch(JSON.stringify(events), /fixture-private-secret/);
});

test("missing native failure details use the project contract rather than an ad hoc message", () => {
    const call = normalizeToolCall({
        toolCallId: "missing-native", name: "Bash", status: "failed", progress: "waiting for process",
    }, undefined, { nativeErrorContract: true });
    const parsed = JSON.parse(call.output);
    assert.deepEqual(Object.keys(parsed).sort(), ["code", "details", "message", "retryable"]);
    assert.equal(parsed.code, "TOOL_EXECUTION_FAILED");
    assert.equal(parsed.details.tool, "Bash");
    assert.equal(parsed.details.outcome, "unknown");
    assert.match(parsed.message, /Runtime did not provide a failure reason/);
    assert.match(parsed.message, /Last progress: waiting for process/);
});

test("ACP terminal content is safe output, partial failure retains the earlier cause", () => {
    const prior = { toolCallId: "1", status: "in_progress", input: "input", progress: "reading" };
    const failed = normalizeToolCall({ toolCallId: "1", status: "failed", content: [
        { type: "content", content: { type: "text", text: "missing TASK; token=private-secret" } },
        { type: "resource", resource: { text: "private-document" } },
    ] }, prior);
    assert.match(failed.output, /missing TASK/);
    assert.doesNotMatch(failed.output, /private-secret|private-document|\[object Object\]/);
    assert.match(failed.output, /token=\[REDACTED\]$/);
    assert.equal(normalizeToolCall({ toolCallId: "1", status: "failed" }, failed).output, failed.output);
    const fallback = normalizeToolCall({ toolCallId: "1", status: "failed" }, prior);
    assert.match(fallback.output, /runtime did not provide|Runtime did not provide/);
    assert.match(fallback.output, /Last progress: reading/);
    assert.match(fallback.output, /diagnosticId/);
});

test("process envelope round trips without changing result or interpreting text prefixes", () => {
    const result = { exitCode: 2, signal: null, stdout: "x".repeat(10000), stderr: "missing table" };
    const display = processToolDisplay(result);
    assert.match(display.output.slice(0, 100), /missing table/);
    assert.ok(display.output.length <= 3600);
    assert.deepEqual(display.process, { exitCode: 2 });
    let wire;
    createAcpSessionNotifier({ write: (line) => { wire = JSON.parse(line); } }).sendToolCallUpdate("s", "tool_call_update", {
        toolCallId: "1", name: "Bash", status: "completed", ...display,
    });
    const normalized = normalizeToolCall(wire.params.update);
    assert.equal(normalized.output, display.output);
    assert.deepEqual(normalized.process, display.process);
    assert.equal(normalized.status, "completed");
    assert.equal(normalizeToolCall({ toolCallId: "old", status: "completed", rawOutput: "exitCode: 9" }).process, undefined);
    assert.equal(normalizeToolCall({ toolCallId: "bad", rawOutput: {
        schema: "msinsight.tool-process.v1", text: "bad", process: { exitCode: "0" },
    } }).process, undefined);
    assert.deepEqual(processToolDisplay({ exitCode: null, signal: "SIGTERM" }).process, { exitCode: null, signal: "SIGTERM" });
    assert.equal(result.stdout.length, 10000);
});

test("ACP partial terminal content and outputs are sanitized in the Host, log and export", (t) => {
    const logs = [];
    t.mock.method(console, "warn", (...values) => logs.push(values));
    const state = createRuntimeState();
    getSessionContext(state, "session").pendingPrompt = true;
    const events = [];
    const chat = createChatService({ state, eventBus: { broadcast: (event) => events.push(event) } });
    const update = (patch) => chat.handleAcpNotification({
        method: "session/update",
        params: { sessionId: "session", update: { kind: "tool_call_update", toolCallId: "call", ...patch } },
    });
    const call = () => state.sessionContexts.get("session").messages[0].content[0].toolCall;
    update({ status: "in_progress", rawInput: "original input", content: "reading" });
    update({ status: "failed" });
    update({ content: [{ type: "text", text: 'missing TASK; token="fixtureLateSecret' }] });
    assert.equal(call().status, "failed");
    assert.match(call().output, /missing TASK/);
    assert.doesNotMatch(call().output, /fixtureLateSecret|Runtime did not provide/);
    const specific = call().output;
    update({ progress: "late progress, not a root cause" });
    update({ rawInput: "updated input" });
    assert.equal(call().output, specific);
    update({ rawOutput: "permission denied; password='fixtureOutputSecret" });
    assert.match(call().output, /permission denied/);
    assert.doesNotMatch(call().output, /fixtureOutputSecret/);
    assert.equal(call().input, "updated input");
    assert.equal(state.sessionContexts.get("session").messages[0].content.length, 1);
    const exported = buildSessionExport({ sessionId: "session", messages: state.sessionContexts.get("session").messages });
    for (const channel of [JSON.stringify(events), JSON.stringify(logs), JSON.stringify(exported), exported.markdown]) {
        assert.doesNotMatch(channel, /fixtureLateSecret|fixtureOutputSecret/);
    }
    assert.match(JSON.stringify(logs), /missing TASK/);
    assert.match(exported.markdown, /permission denied/);
});

test("a progress-only failed patch labels progress rather than inventing a failure cause", () => {
    const call = normalizeToolCall({ toolCallId: "progress", status: "failed", progress: "searching" });
    assert.match(call.output, /Runtime did not provide a failure reason/);
    assert.match(call.output, /Last progress: searching/);
});

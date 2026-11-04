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
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { MockLanguageModelV3 } from "ai/test";
import { createRuntime } from "../../native-agent/runtime/aiSdkRuntime.mjs";

const usage = {
    inputTokens: { total: 1, noCache: 1, cacheRead: 0, cacheWrite: 0 },
    outputTokens: { total: 1, text: 1, reasoning: 0 },
};
for (const scenario of ["object", "frozen", "oversized", "invalid_json"]) test(`actual SDK runtime error boundary: ${scenario}`, async (t) => {
    const root = await mkdtemp(join(tmpdir(), "tool-error-boundary-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    let executions = 0;
    let requests = 0;
    const original = scenario === "frozen" ? Object.freeze(Object.assign(new Error("Frozen failure."), {
        code: "COMMAND_UNAVAILABLE", details: { reason: "module_not_active", targetModule: "Timeline" },
    })) : scenario === "oversized" ? {
        code: "CLI_EXIT_NONZERO", message: "x".repeat(10000), details: { stderr: `${"x".repeat(100000)}\nSPECIFIC_END` },
    } : { code: "COMMAND_UNAVAILABLE", message: "Plain object failure.", details: { reason: "module_not_active" } };
    const model = new MockLanguageModelV3({
        doStream: async () => ({
            stream: new ReadableStream({
                start(controller) {
                    controller.enqueue({ type: "stream-start", warnings: [] });
                    if (++requests === 1) {
                        controller.enqueue({ type: "tool-call", toolCallId: "call", toolName: "fixture", input: scenario === "invalid_json" ? "not-json" : "{}" });
                        controller.enqueue({ type: "finish", finishReason: { unified: "tool-calls", raw: "tool_calls" }, usage });
                    } else {
                        controller.enqueue({ type: "text-start", id: "summary" });
                        controller.enqueue({ type: "text-delta", id: "summary", delta: "Fixture result." });
                        controller.enqueue({ type: "text-end", id: "summary" });
                        controller.enqueue({ type: "finish", finishReason: { unified: "stop", raw: "stop" }, usage });
                    }
                    controller.close();
                },
            }),
        }),
    });
    const updates = [];
    const runtime = createRuntime({
        aiSdkStoragePath: root, createModel: () => model,
        modelCatalog: { defaultModelId: "fixture", models: [{ id: "fixture" }] },
        toolRegistry: {
            list: () => [{ name: "fixture", inputSchema: { type: "object", properties: {} } }],
            execute: async () => { executions++; throw original; },
        },
        notifier: {
            sendSessionChunk() {}, sendSessionThinkingChunk() {}, sendAgentActivityUpdate() {},
            sendToolCallUpdate: (_id, kind, toolCall) => updates.push({ kind, ...structuredClone(toolCall) }),
        },
    });
    const session = { sessionId: "fixture", messages: [], hostSystemPrompt: "", primaryAgentBody: "" };
    assert.deepEqual(await runtime.runPrompt({ session, sessionId: "fixture", userText: "fixture", controller: new AbortController() }), { ok: true });
    assert.equal(executions, scenario === "invalid_json" ? 0 : 1);
    assert.equal(requests, 2);
    const value = model.doStreamCalls[1].prompt.find((message) => message.role === "tool").content[0].output.value;
    assert.ok(value && value !== "[object Object]");
    const terminal = updates.find((update) => update.kind === "tool_call_update");
    assert.ok(terminal.output.length <= 3600);
    const contract = JSON.parse(terminal.output);
    assert.deepEqual(Object.keys(contract).sort(), ["code", "details", "message", "retryable"]);
    assert.equal(typeof contract.retryable, "boolean");
    assert.ok(contract.message);
    if (scenario !== "invalid_json") assert.equal(JSON.parse(value).details.diagnosticId, contract.details.diagnosticId);
    if (scenario === "invalid_json") {
        assert.match(terminal.output, /"source":"sdk"/);
        assert.match(terminal.output, /"stage":"validate"/);
        assert.match(terminal.output, /"outcome":"not_started"/);
    }
    if (scenario !== "invalid_json") {
        assert.equal(value, terminal.output);
        assert.ok(value.length <= 3600);
        assert.match(value, scenario === "oversized" ? /SPECIFIC_END/ : /module_not_active/);
    }
    if (scenario === "frozen") assert.equal(original.message, "Frozen failure.");
});

test("user cancellation with no tool receipt keeps one unknown result with the same diagnostic ID", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "tool-error-unsettled-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const controller = new AbortController();
    const updates = [];
    let executions = 0;
    const runtime = createRuntime({
        aiSdkStoragePath: root, createModel: () => ({}),
        modelCatalog: { defaultModelId: "fixture", models: [{ id: "fixture" }] },
        toolRegistry: {
            list: () => [{ name: "fixture", inputSchema: { type: "object" } }],
            execute: async () => { executions++; return "submitted"; },
        },
        notifier: {
            sendSessionChunk() {}, sendSessionThinkingChunk() {}, sendAgentActivityUpdate() {},
            sendToolCallUpdate: (_id, kind, toolCall) => updates.push({ kind, ...structuredClone(toolCall) }),
        },
        streamTextImpl: (options) => ({
            fullStream: {
                async *[Symbol.asyncIterator]() {
                    yield { type: "tool-call", toolCallId: "pending", toolName: "fixture", input: {} };
                    await options.tools.fixture.execute({}, {
                        toolCallId: "pending", abortSignal: options.abortSignal, experimental_context: options.experimental_context,
                    });
                    controller.abort(Object.assign(new Error("User stopped"), { code: "USER_CANCEL" }));
                    throw Object.assign(new Error("fixture_private_dependency"), { code: "ECONNRESET", statusCode: 503 });
                },
            },
            response: Promise.resolve({ messages: [] }),
        }),
    });
    const session = { sessionId: "fixture", messages: [], hostSystemPrompt: "", primaryAgentBody: "" };
    const result = await runtime.runPrompt({ session, sessionId: "fixture", userText: "fixture", controller });
    assert.equal(result.ok, false);
    assert.equal(executions, 1);
    const output = session.runtimeSession.modelMessages.flatMap((message) => Array.isArray(message.content) ? message.content : [])
        .find((part) => part.type === "tool-result").output.value;
    assert.match(output, /TOOL_OUTCOME_UNKNOWN/);
    assert.match(output, /Side effects may have occurred/);
    assert.match(output, /ECONNRESET/);
    assert.match(output, /503/);
    assert.ok(!output.includes("fixture_private_dependency"));
    assert.equal(output, updates.find((update) => update.kind === "tool_call_update").output);
});

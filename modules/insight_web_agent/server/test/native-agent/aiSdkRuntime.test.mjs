/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { createRuntime, createAiSdkModel } from "../../native-agent/runtime/aiSdkRuntime.mjs";

const createNotifier = () => {
    const updates = [];
    return {
        updates,
        sendSessionChunk: (sessionId, text) => updates.push({ type: "text", sessionId, text }),
        sendSessionThinkingChunk: (sessionId, text) => updates.push({ type: "thinking", sessionId, text }),
        sendAgentActivityUpdate: (sessionId, activity) => updates.push({ type: "activity", sessionId, activity }),
        sendToolCallUpdate: (sessionId, kind, toolCall) => updates.push({ type: "tool", sessionId, kind, toolCall: { ...toolCall } }),
    };
};

const createSession = () => ({
    sessionId: "session-1",
    messages: [],
    hostSystemPrompt: "Host rules",
    primaryAgentBody: "Agent rules",
    lastPageObservationFingerprint: undefined,
});

const asAsyncStream = (factory) => ({
    async *[Symbol.asyncIterator]() {
        yield* factory();
    },
});

test("AI SDK runtime streams ordered content, executes native tools, and restores persisted history", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-ai-sdk-runtime-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const notifier = createNotifier();
    const executions = [];
    let capturedOptions;
    const toolRegistry = {
        list: () => [
            { name: "msinsight", description: "Run Insight command", inputSchema: { type: "object", properties: { command: { type: "string" } }, required: ["command"] } },
            { name: "skill", description: "Load Skill", inputSchema: { type: "object", properties: { name: { type: "string" } }, required: ["name"] } },
        ],
        execute: async (name, input, context) => {
            executions.push({ name, input, context });
            return name === "skill" ? { instructions: "workflow" } : { observed: true };
        },
    };
    const responseMessages = [
        { role: "assistant", content: [{ type: "text", text: "before" }, { type: "tool-call", toolCallId: "tool-1", toolName: "msinsight", input: { command: "observe" } }] },
        { role: "tool", content: [{ type: "tool-result", toolCallId: "tool-1", toolName: "msinsight", output: { type: "json", value: { observed: true } } }] },
        { role: "assistant", content: [{ type: "text", text: "after" }] },
    ];
    const streamTextImpl = (options) => {
        capturedOptions = options;
        return {
            fullStream: asAsyncStream(async function* () {
                yield { type: "text-delta", id: "text-1", text: "before" };
                yield { type: "reasoning-delta", id: "reasoning-1", text: "thinking" };
                yield { type: "tool-call", toolCallId: "tool-1", toolName: "msinsight", input: { command: "observe" } };
                const output = await options.tools.msinsight.execute(
                    { command: "observe" },
                    { toolCallId: "tool-1", messages: [], abortSignal: options.abortSignal, experimental_context: options.experimental_context },
                );
                yield { type: "tool-result", toolCallId: "tool-1", toolName: "msinsight", input: { command: "observe" }, output };
                yield { type: "tool-call", toolCallId: "skill-1", toolName: "skill", input: { name: "memory" } };
                const skillOutput = await options.tools.skill.execute(
                    { name: "memory" },
                    { toolCallId: "skill-1", messages: [], abortSignal: options.abortSignal, experimental_context: options.experimental_context },
                );
                yield { type: "tool-result", toolCallId: "skill-1", toolName: "skill", input: { name: "memory" }, output: skillOutput };
                yield { type: "text-delta", id: "text-2", text: "after" };
                yield { type: "finish", finishReason: "stop", totalUsage: {} };
            }),
            response: Promise.resolve({ messages: responseMessages }),
        };
    };
    const runtime = createRuntime({
        env: { MSINSIGHT_NATIVE_MAX_STEPS: "8" },
        aiSdkStoragePath: root,
        toolRegistry,
        notifier,
        streamTextImpl,
        createModel: () => ({ modelId: "test-model" }),
    });
    const session = createSession();
    const controller = new AbortController();

    assert.deepEqual(await runtime.runPrompt({
        session,
        sessionId: session.sessionId,
        userText: "find leaks",
        pageObservation: { collectedAt: 1, blocks: [1] },
        controller,
    }), { ok: true });

    assert.equal(capturedOptions.messages.length, 1);
    assert.equal(capturedOptions.messages[0].role, "user");
    assert.match(capturedOptions.messages[0].content, /<current_user_message>\nfind leaks/);
    assert.equal(capturedOptions.system.includes("Use Read, Glob, and Grep"), false);
    assert.equal(await capturedOptions.stopWhen({ steps: Array.from({ length: 8 }) }), true);
    assert.deepEqual(executions.map(({ name }) => name), ["msinsight", "skill"]);
    assert.equal(executions[0].context.sessionId, session.sessionId);
    assert.equal(executions[0].context.signal, controller.signal);
    assert.deepEqual(session.messages[1].content.map(({ type }) => type), ["text", "thinking", "tool", "tool", "text"]);
    assert.equal(session.messages[1].content[2].toolCall.status, "completed");
    assert.equal(session.messages[1].content[3].toolCall.name, "skill");

    const stored = JSON.parse(await readFile(join(root, "sessions", "session-1.json"), "utf8"));
    assert.deepEqual(stored.modelMessages.slice(1), responseMessages);
    assert.equal(stored.uiMessages[1].content[3].toolCall.name, "skill");

    const restoredSession = createSession();
    const restoredRuntime = createRuntime({
        aiSdkStoragePath: root,
        toolRegistry,
        notifier: createNotifier(),
        streamTextImpl,
        createModel: () => ({ modelId: "test-model" }),
    });
    await restoredRuntime.restoreSession(restoredSession);
    assert.deepEqual(restoredSession.messages, session.messages);
});

test("AI SDK runtime persists interrupted UI output without adding partial model history", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-ai-sdk-cancel-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const notifier = createNotifier();
    const controller = new AbortController();
    const runtime = createRuntime({
        aiSdkStoragePath: root,
        toolRegistry: { list: () => [], execute: async () => undefined },
        notifier,
        createModel: () => ({ modelId: "test-model" }),
        streamTextImpl: () => ({
            fullStream: asAsyncStream(async function* () {
                yield { type: "text-delta", id: "text-1", text: "partial" };
                controller.abort();
                yield { type: "abort", reason: "cancelled" };
            }),
            response: Promise.resolve({ messages: [] }),
        }),
    });
    const session = createSession();

    assert.deepEqual(await runtime.runPrompt({
        session,
        sessionId: session.sessionId,
        userText: "cancel me",
        controller,
    }), { ok: false, reason: "AI SDK runtime was cancelled" });

    const stored = JSON.parse(await readFile(join(root, "sessions", "session-1.json"), "utf8"));
    assert.deepEqual(stored.modelMessages, []);
    assert.equal(stored.uiMessages[0].content[0].text, "cancel me");
    assert.equal(stored.uiMessages[1].content[0].text, "partial");
});

test("AI SDK runtime reports unsupported providers before making a request", () => {
    assert.throws(() => createAiSdkModel({
        MSINSIGHT_NATIVE_PROVIDER: "custom-provider",
        MSINSIGHT_NATIVE_API_KEY: "test-key",
        MSINSIGHT_NATIVE_MODEL: "test-model",
    }, globalThis.fetch), /unsupported_provider:custom-provider/);
    assert.equal(createAiSdkModel({ MSINSIGHT_NATIVE_PROVIDER: "openai" }, globalThis.fetch), undefined);
});

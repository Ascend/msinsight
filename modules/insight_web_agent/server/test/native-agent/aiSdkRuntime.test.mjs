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
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
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
        userText: "Hidden assistant preset\nfind leaks",
        visibleUserText: "find leaks",
        pageObservation: { collectedAt: 1, blocks: [1] },
        controller,
    }), { ok: true });

    assert.equal(capturedOptions.messages.length, 1);
    assert.equal(capturedOptions.messages[0].role, "user");
    assert.match(capturedOptions.messages[0].content, /<current_user_message>\nHidden assistant preset\nfind leaks/);
    assert.equal(session.messages[0].content[0].text, "find leaks");
    assert.equal(capturedOptions.system.includes("Use Read, Glob, and Grep"), false);
    assert.equal(await capturedOptions.stopWhen({ steps: Array.from({ length: 8 }) }), true);
    assert.equal(capturedOptions.prepareStep({ stepNumber: 6 }).system, capturedOptions.system);
    const finalStep = capturedOptions.prepareStep({ stepNumber: 7 });
    assert.equal(finalStep.toolChoice, "none");
    assert.ok(finalStep.system.startsWith(capturedOptions.system));
    assert.match(finalStep.system, /including help/);
    assert.match(finalStep.system, /unfinished work/);
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
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
        toolRegistry,
        notifier: createNotifier(),
        streamTextImpl,
        createModel: () => ({ modelId: "test-model" }),
    });
    await restoredRuntime.restoreSession(restoredSession);
    assert.deepEqual(restoredSession.messages, session.messages);
});

for (const progressText of ["", "正在调整视域。"]) {
    test(`AI SDK runtime keeps tool history without a final answer (progress=${Boolean(progressText)})`, async (t) => {
        const root = await mkdtemp(join(tmpdir(), "msinsight-ai-sdk-no-answer-"));
        t.after(() => rm(root, { recursive: true, force: true }));
        const notifier = createNotifier();
        const input = { command: "Timeline.zoom", args: { direction: "in" } };
        const output = { viewport: { start: 0, end: 600000000, duration: 600000000 } };
        const responseMessages = [
            { role: "assistant", content: [{ type: "tool-call", toolCallId: "zoom-1", toolName: "msinsight", input }] },
            { role: "tool", content: [{ type: "tool-result", toolCallId: "zoom-1", toolName: "msinsight", output: { type: "json", value: output } }] },
        ];
        const runtime = createRuntime({
            aiSdkStoragePath: root,
            modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
            toolRegistry: { list: () => [], execute: async () => undefined },
            notifier,
            createModel: () => ({ modelId: "test-model" }),
            streamTextImpl: () => ({
                fullStream: asAsyncStream(async function* () {
                    if (progressText) yield { type: "text-delta", text: progressText };
                    yield { type: "tool-call", toolCallId: "zoom-1", toolName: "msinsight", input };
                    yield { type: "tool-result", toolCallId: "zoom-1", toolName: "msinsight", input, output };
                    yield { type: "finish", finishReason: "tool-calls", totalUsage: {} };
                }),
                response: Promise.resolve({ messages: responseMessages }),
            }),
        });
        const session = createSession();

        assert.deepEqual(await runtime.runPrompt({
            session, sessionId: session.sessionId, userText: "放大到500ms", controller: new AbortController(),
        }), { ok: true });

        const stored = JSON.parse(await readFile(join(root, "sessions", session.sessionId + ".json"), "utf8"));
        assert.deepEqual(stored.modelMessages.slice(1, 3), responseMessages);
        const summary = stored.modelMessages.at(-1).content;
        assert.match(summary, /无法确认任务已完成/);
        assert.equal(stored.uiMessages[0].content[0].text, "放大到500ms");
        const toolBlock = stored.uiMessages[1].content.find((block) => block.type === "tool");
        assert.equal(toolBlock.toolCall.status, "completed");
        assert.equal(stored.uiMessages[1].content.at(-1).text, summary);
        assert.ok(notifier.updates.some((update) => update.type === "text" && update.text === summary));
        assert.equal(responseMessages.length, 2);
    });
}

test("AI SDK runtime persists interrupted model history with a hidden neutral marker", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-ai-sdk-cancel-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const notifier = createNotifier();
    const controller = new AbortController();
    const runtime = createRuntime({
        aiSdkStoragePath: root,
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
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
        userText: "Hidden assistant preset\ncancel me",
        visibleUserText: "cancel me",
        controller,
    }), { ok: false, reason: "AI SDK runtime was cancelled" });

    const stored = JSON.parse(await readFile(join(root, "sessions", "session-1.json"), "utf8"));
    assert.match(stored.modelMessages[0].content, /Hidden assistant preset\ncancel me/);
    assert.equal(stored.modelMessages[1].content[0].text, "partial");
    assert.equal(stored.modelMessages[2].role, "user");
    assert.match(stored.modelMessages[2].content, /\[Request interrupted\]/);
    assert.doesNotMatch(JSON.stringify(stored.uiMessages), /\[Request interrupted(?: by user)?\]/);
    assert.doesNotMatch(JSON.stringify(notifier.updates), /\[Request interrupted(?: by user)?\]/);
    assert.equal(stored.uiMessages[0].content[0].text, "cancel me");
    assert.equal(stored.uiMessages[1].content[0].text, "partial");
});

test("cancel then restore and continue retains raw completed tools, reconciles pending tools, and hides user interruption", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "native-cancel-tools-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const notifier = createNotifier();
    const controller = new AbortController();
    const output = { data: "x".repeat(20000) };
    let turn = 0;
    let continuedMessages;
    const runtime = createRuntime({
        aiSdkStoragePath: root, notifier, toolRegistry: { list: () => [] },
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
        createModel: () => ({}),
        streamTextImpl: (options) => {
            turn += 1;
            if (turn > 1) {
                continuedMessages = options.messages;
                return {
                    fullStream: asAsyncStream(async function* () { yield { type: "text-delta", text: "continued" }; }),
                    response: Promise.resolve({ messages: [{ role: "assistant", content: "continued" }] }),
                };
            }
            return {
                fullStream: asAsyncStream(async function* () {
                    yield { type: "start-step" };
                    yield { type: "text-delta", text: "before" };
                    yield { type: "tool-call", toolCallId: "done", toolName: "msinsight", input: { command: "observe" } };
                    yield { type: "tool-result", toolCallId: "done", toolName: "msinsight", output };
                    yield { type: "finish-step" };
                    yield { type: "start-step" };
                    yield { type: "text-delta", text: "partial" };
                    yield { type: "tool-call", toolCallId: "pending", toolName: "msinsight", input: { command: "zoom" } };
                    controller.abort(Object.assign(new Error("User cancelled"), { code: "USER_CANCEL" }));
                    // A stream may close silently on abort, without an abort event.
                }),
                response: Promise.resolve({ messages: [] }),
            };
        },
    });
    const session = createSession();
    assert.equal((await runtime.runPrompt({ session, sessionId: session.sessionId, userText: "do it", controller })).ok, false);
    const history = session.runtimeSession.modelMessages;
    assert.deepEqual(history.map((message) => message.role), ["user", "assistant", "tool", "assistant", "tool", "user"]);
    assert.deepEqual(history[2].content[0].output, { type: "json", value: output });
    assert.equal(history[4].content[0].output.type, "error-text");
    assert.match(history[4].content[0].output.value, /Side effects may have occurred/);
    assert.match(history.at(-1).content, /\[Request interrupted by user\]/);
    assert.doesNotMatch(JSON.stringify(session.messages), /\[Request interrupted by user\]/);
    assert.doesNotMatch(JSON.stringify(notifier.updates), /\[Request interrupted by user\]/);
    const restored = createSession();
    await runtime.restoreSession(restored);
    assert.deepEqual(restored.runtimeSession.modelMessages, history);
    assert.deepEqual(restored.messages, session.messages);
    assert.equal((await runtime.runPrompt({ session: restored, sessionId: restored.sessionId, userText: "continue", controller: new AbortController() })).ok, true);
    assert.deepEqual(continuedMessages.slice(0, -1), history);
});

test("cancellation drains a queued tool result without committing successful history twice", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "native-cancel-response-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const controller = new AbortController();
    const runtime = createRuntime({
        aiSdkStoragePath: root, notifier: createNotifier(), toolRegistry: { list: () => [] },
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
        createModel: () => ({}),
        streamTextImpl: () => ({
            fullStream: asAsyncStream(async function* () {
                yield { type: "tool-call", toolCallId: "done", toolName: "msinsight", input: {} };
                controller.abort(Object.assign(new Error("User cancelled"), { code: "USER_CANCEL" }));
                yield { type: "tool-result", toolCallId: "done", toolName: "msinsight", output: "completed before abort" };
            }),
            response: Promise.resolve({ messages: [{ role: "assistant", content: "not committed" }] }),
        }),
    });
    const session = createSession();
    await runtime.runPrompt({ session, sessionId: session.sessionId, userText: "question", controller });
    const history = session.runtimeSession.modelMessages;
    assert.deepEqual(history.map(({ role }) => role), ["user", "assistant", "tool", "user"]);
    assert.deepEqual(history[2].content[0].output, { type: "text", value: "completed before abort" });
    assert.equal(session.messages.length, 2);
    assert.doesNotMatch(JSON.stringify(history), /not committed/);
});

test("ordinary stream errors do not record a user interruption", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "native-stream-error-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const runtime = createRuntime({
        aiSdkStoragePath: root, notifier: createNotifier(), toolRegistry: { list: () => [] },
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
        createModel: () => ({}),
        streamTextImpl: () => ({ fullStream: asAsyncStream(async function* () {
            yield { type: "text-delta", text: "partial" };
            throw new Error("network failure");
        }) }),
    });
    const session = createSession();
    const result = await runtime.runPrompt({ session, sessionId: session.sessionId, userText: "question", controller: new AbortController() });
    assert.match(result.reason, /network failure/);
    assert.deepEqual(session.runtimeSession.modelMessages, []);
    assert.deepEqual(session.messages, []);
});

test("each turn uses the session model and retains independent histories and shared env", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "native-model-isolation-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const env = Object.freeze({ MSINSIGHT_NATIVE_MODEL: "model-a", MSINSIGHT_NATIVE_API_KEY: "shared-key", MSINSIGHT_NATIVE_BASE_URL: "https://example.invalid" });
    const created = [];
    const turns = [];
    const runtime = createRuntime({
        env, aiSdkStoragePath: root, notifier: createNotifier(), toolRegistry: { list: () => [] },
        modelCatalog: { defaultModelId: "model-a", models: [{ id: "model-a", name: "A" }, { id: "model-b", name: "B" }] },
        createModel: (turnEnv) => { created.push(turnEnv); return { modelId: turnEnv.MSINSIGHT_NATIVE_MODEL }; },
        streamTextImpl: (options) => {
            turns.push(options);
            return { fullStream: asAsyncStream(async function* () { yield { type: "text-delta", text: "answer" }; }), response: Promise.resolve({ messages: [{ role: "assistant", content: "answer" }] }) };
        },
    });
    const a = { ...createSession(), sessionId: "a", modelId: "model-a" };
    const b = { ...createSession(), sessionId: "b", modelId: "model-b" };
    const run = (session) => runtime.runPrompt({ session, sessionId: session.sessionId, userText: "question", controller: new AbortController() });
    assert.equal((await run(a)).ok, true);
    assert.equal((await run(b)).ok, true);
    a.modelId = "model-b";
    assert.equal((await run(a)).ok, true);
    assert.deepEqual(turns.map(({ model }) => model.modelId), ["model-a", "model-b", "model-b"]);
    assert.deepEqual(turns.map(({ messages }) => messages.length), [1, 1, 3]);
    assert.equal(b.messages.length, 2);
    assert.equal(env.MSINSIGHT_NATIVE_MODEL, "model-a");
    assert.ok(created.every((copy) => copy !== env && copy.MSINSIGHT_NATIVE_API_KEY === "shared-key"));
    a.modelId = "removed";
    assert.match((await run(a)).reason, /Model is unavailable: removed/);
    assert.equal(created.length, 3);
});

test("rate-limited replies persist only visible user text", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "native-preset-rate-limit-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    t.mock.method(globalThis, "fetch", async () => new Response("rate limited", { status: 429 }));
    let modelFetch;
    const runtime = createRuntime({
        aiSdkStoragePath: root,
        notifier: createNotifier(), toolRegistry: { list: () => [] },
        modelCatalog: { defaultModelId: "test-model", models: [{ id: "test-model", name: "Test" }] },
        createModel: (_, fetch) => { modelFetch = fetch; return {}; },
        streamTextImpl: () => ({
            fullStream: asAsyncStream(async function* () {
                await modelFetch("https://example.invalid");
                throw new Error("rate limited");
            }),
        }),
    });
    const session = createSession();
    assert.deepEqual(await runtime.runPrompt({
        session, sessionId: session.sessionId,
        userText: "Hidden preset\nVisible request", visibleUserText: "Visible request",
        controller: new AbortController(),
    }), { ok: true });
    const restoredSession = createSession();
    await runtime.restoreSession(restoredSession);
    assert.equal(restoredSession.messages[0].content[0].text, "Visible request");
});

test("AI SDK runtime reports unsupported providers before making a request", () => {
    assert.throws(() => createAiSdkModel({
        MSINSIGHT_NATIVE_PROVIDER: "custom-provider",
        MSINSIGHT_NATIVE_API_KEY: "test-key",
        MSINSIGHT_NATIVE_MODEL: "test-model",
    }, globalThis.fetch), /unsupported_provider:custom-provider/);
    assert.equal(createAiSdkModel({ MSINSIGHT_NATIVE_PROVIDER: "openai" }, globalThis.fetch), undefined);
});

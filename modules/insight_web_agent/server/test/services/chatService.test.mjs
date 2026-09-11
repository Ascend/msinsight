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
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { createChatService, createPromptContent } from "../../services/chatService.mjs";
import { createContextAssembler } from "../../services/contextAssembler.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

test("createPromptContent serializes pasted images as ACP image blocks", () => {
    const content = createPromptContent("分析这个图片", [{
        id: "image-1",
        name: "image.png",
        data: "iVBORw0KGgo=",
        mimeType: "image/png",
    }]);

    assert.deepEqual(content, [
        { type: "text", text: "分析这个图片" },
        {
            type: "image",
            data: "iVBORw0KGgo=",
            mimeType: "image/png",
            uri: "zed:///agent/pasted-image?name=image.png",
        },
    ]);
});

test("createPromptContent preserves image mime types", () => {
    const content = createPromptContent("", [
        { id: "image-1", name: "photo.jpg", data: "/9j/4AAQSkZJRg==", mimeType: "image/jpeg" },
        { id: "image-2", name: "image.webp", data: "UklGRiIAAABXRUJQVlA=", mimeType: "image/webp" },
        { id: "image-3", name: "diagram.svg", data: "PHN2Zy8+", mimeType: "image/svg+xml" },
    ]);

    assert.deepEqual(content.map((part) => part.mimeType), [
        "image/jpeg",
        "image/webp",
        "image/svg+xml",
    ]);
});

test("createPromptContent prepends hidden context without changing visible text", () => {
    const content = createPromptContent("visible request", [], {
        projectName: "demo",
        files: ["src/index.ts"],
    });

    assert.deepEqual(content, [
        {
            type: "resource",
            resource: {
                uri: "insight-hidden-context://project",
                mimeType: "application/json",
                text: '<context ref="insight-hidden-context://project"/>\n{"contextPolicy":"replace_previous_hidden_context","instruction":"Use this hidden context as the authoritative project context for this turn and ignore any previous hidden project context in this session.","data":{"projectName":"demo","files":["src/index.ts"]}}',
            },
        },
        { type: "text", text: "visible request" },
    ]);
});

test("createPromptContent injects system prompt as the leading resource block", () => {
    const content = createPromptContent("hello", [], undefined, "你是一个 Ascend 调优助手");

    assert.deepEqual(content, [
        {
            type: "resource",
            resource: {
                uri: "insight-system-prompt://project",
                mimeType: "text/plain",
                text: '<context ref="insight-system-prompt://project"/>\n你是一个 Ascend 调优助手',
            },
        },
        { type: "text", text: "hello" },
    ]);
});

test("createPromptContent skips empty system prompt for backward compatibility", () => {
    const withoutSystem = createPromptContent("hello", []);
    const withEmptySystem = createPromptContent("hello", [], undefined, "   ");
    const withNullSystem = createPromptContent("hello", [], undefined, null);

    assert.deepEqual(withoutSystem, [{ type: "text", text: "hello" }]);
    assert.deepEqual(withEmptySystem, [{ type: "text", text: "hello" }]);
    assert.deepEqual(withNullSystem, [{ type: "text", text: "hello" }]);
});

test("initialize publishes only Skill metadata reported by the Runtime", async () => {
    const state = createRuntimeState();
    const service = createChatService({
        acpClient: {
            request: async (method) => {
                assert.equal(method, "initialize");
                return {
                    agentInfo: { name: "Runtime" },
                    _meta: {
                        "msinsight.dev/skills": [
                            { name: " inspect-memory ", description: " Inspect memory ", instructions: "must not leak" },
                            { description: "missing name" },
                        ],
                    },
                };
            },
        },
        eventBus: { broadcast: () => {} },
        sessionService: { loadConfigOptions: async () => {}, refreshSessions: async () => {}, broadcastState: () => {} },
        state,
    });

    await service.initialize();

    assert.deepEqual(state.availableSkills, [{ name: "inspect-memory", description: "Inspect memory" }]);
});

test("prompt leaves explicit Skill syntax for the Runtime", async () => {
    const { service, calls, state } = createPromptTestService();
    state.availableSkills = [{ name: "inspect-memory", description: "Inspect memory" }];

    await service.prompt("/inspect-memory focus=peak", { sessionId: "session-1" });
    await waitForPromptCall(calls, 1);

    const prompt = calls.find((call) => call.method === "session/prompt").params.prompt;
    assert.equal(prompt.at(-1).text, "/inspect-memory focus=peak");
    assert.doesNotMatch(JSON.stringify(prompt), /<skill/);
});

test("prompt applies requested mode to a new session before sending", async () => {
    const calls = [];
    const state = createRuntimeState();
    state.initialized = true;
    state.agentCapabilities = { session: { setConfigOption: true } };

    const acpClient = {
        async request(method, params) {
            calls.push({ method, params });
            if (method === "session/set_config_option") {
                assert.equal(params.sessionId, "session-1");
                assert.equal(params.configId, "mode");
                assert.equal(params.value, "bypass");
                return { configOptions: [modeConfig("bypass")] };
            }
            if (method === "session/prompt") return { stopReason: "end_turn" };
            throw new Error(`unexpected ACP method: ${method}`);
        },
    };

    const sessionService = {
        async createSessionContext({ mode }) {
            assert.equal(mode, "bypass");
            state.sessionContexts.set("session-1", {
                sessionId: "session-1",
                messages: [],
                pendingPrompt: false,
                configOptions: [modeConfig("default")],
            });
            return "session-1";
        },
        async setMode(mode, sessionId) {
            const response = await acpClient.request("session/set_config_option", {
                sessionId,
                configId: "mode",
                value: mode,
            });
            state.sessionContexts.get(sessionId).configOptions = response.configOptions;
            return { ok: true, configOptions: response.configOptions };
        },
        applyPreferredModel: async () => {},
        broadcastState: () => {},
        refreshSessions: async () => {},
    };

    const service = createChatService({
        acpClient,
        eventBus: { broadcast: () => {} },
        sessionService,
        state,
    });

    const result = await service.prompt("hello", { newSession: true, mode: "bypass" });

    assert.equal(result.sessionId, "session-1");
    assert.deepEqual(calls.map((call) => call.method), ["session/set_config_option", "session/prompt"]);
});

test("prompt sends the configured Host System Prompt as an ACP resource", async () => {
    const { service, calls } = createPromptTestService("Host system instructions");

    await service.prompt("analyze", { sessionId: "session-1" });
    await waitForPromptCall(calls, 1);

    const prompt = calls.find((call) => call.method === "session/prompt").params.prompt;
    assert.equal(prompt[0].resource.uri, "insight-system-prompt://project");
    assert.match(prompt[0].resource.text, /Host system instructions/);
});

test("prompt includes the prompt-scoped page observation in hidden context", async () => {
    const { service, calls } = createPromptTestService();
    const pageObservation = {
        collectedAt: 100,
        module: {
            module: "MemScope",
            supported: true,
            tables: [{ tableKey: "memscope.system.blocks", revision: 2 }],
        },
    };

    await service.prompt("observe", { sessionId: "session-1", pageObservation });
    await waitForPromptCall(calls, 1);

    const promptCall = calls.find((call) => call.method === "session/prompt");
    const hiddenContext = JSON.parse(promptCall.params.prompt[0].resource.text.split("\n").slice(1).join("\n"));
    assert.deepEqual(hiddenContext.data.pageObservation, pageObservation);
});

test("prompt reads raw hidden context from the context assembler", async () => {
    const { service, calls } = createPromptTestService();

    await service.prompt("first", { sessionId: "session-1" });
    await waitForPromptCall(calls, 1);

    await service.prompt("second", { sessionId: "session-1" });
    await waitForPromptCall(calls, 2);

    const promptCalls = calls.filter((call) => call.method === "session/prompt");
    assert.equal(promptCalls[1].params.prompt[0].resource.uri, "insight-hidden-context://project");
    assert.match(promptCalls[1].params.prompt[0].resource.text, /"contentRefs":\{"profileId":"profile-1","activeModule":"Timeline"\}/);
});

test("prompt completion clears the current agent activity", async () => {
    let finishPrompt;
    const promptResult = new Promise((resolve) => {
        finishPrompt = resolve;
    });
    const { service, state, events } = createPromptTestService("", () => promptResult);

    await service.prompt("analyze", { sessionId: "session-1" });
    service.handleAcpNotification({
        method: "session/update",
        params: { sessionId: "session-1", update: { kind: "agent_message_chunk", content: { type: "text", text: "result" } } },
    });
    service.handleAcpNotification({
        method: "session/update",
        params: { sessionId: "session-1", update: { kind: "agent_status_update", activity: "analyzing_tool_results" } },
    });
    assert.equal(state.sessionContexts.get("session-1").messages.at(-1).activity, "analyzing_tool_results");

    finishPrompt({ stopReason: "end_turn" });
    await waitForPromptCompletion(state);

    assert.equal(state.sessionContexts.get("session-1").messages.at(-1).activity, undefined);
    assert.equal(events.filter((event) => event.type === "message_activity").at(-1).activity, undefined);
    assert.equal(events.filter((event) => event.type === "prompt_status").at(-1).completionStatus, "completed");
});

test("prompt and hidden context do not invoke or embed RAG automatically", async () => {
    let retrievals = 0;
    const ragService = { retrieve: async () => { retrievals += 1; } };
    const { service, calls, state } = createPromptTestService("", undefined, ragService);

    await service.prompt("analyze", { sessionId: "session-1" });
    await waitForPromptCall(calls, 1);
    const assembled = await createContextAssembler({ state }).assemble(
        state.sessionContexts.get("session-1"),
        undefined,
        { status: "ok", retrievedChunks: [{ sourceLabel: "must-not-appear" }] },
    );

    assert.equal(retrievals, 0);
    assert.deepEqual(assembled.contextProviders.map(({ name }) => name), ["structured"]);
});

const modeConfig = (currentValue) => ({
    id: "mode",
    name: "Mode",
    category: "mode",
    type: "select",
    currentValue,
    options: [
        { value: "default", name: "Default" },
        { value: "bypass", name: "Bypass Permissions" },
    ],
});

const createPromptTestService = (systemPrompt = "", promptRequest, ragService, memoryTuningPrompt = "") => {
    const calls = [];
    const events = [];
    const state = createRuntimeState();
    state.initialized = true;
    state.activeContext = { profileId: "profile-1", activeModule: "Timeline" };
    state.sessionContexts.set("session-1", {
        sessionId: "session-1",
        agentId: "mock-agent",
        runtime: "stdio",
        mode: "free_chat",
        messages: [],
        pendingPrompt: false,
        configOptions: [],
    });

    const acpClient = {
        async request(method, params) {
            calls.push({ method, params });
            if (method === "session/prompt") return promptRequest ? promptRequest(params) : { stopReason: "end_turn" };
            throw new Error(`unexpected ACP method: ${method}`);
        },
    };

    const service = createChatService({
        acpClient,
        eventBus: { broadcast: (event) => events.push(event) },
        sessionService: {
            applyPreferredModel: async () => {},
            refreshSessions: async () => {},
        },
        state,
        contextAssembler: createContextAssembler({ state }),
        ragService,
        systemPrompt,
        memoryTuningPrompt,
    });

    return { service, calls, events, state };
};

const waitForPromptCompletion = async (state) => {
    for (let index = 0; index < 10; index += 1) {
        if (!state.sessionContexts.get("session-1").pendingPrompt) return;
        await new Promise((resolve) => setTimeout(resolve, 0));
    }
};

const waitForPromptCall = async (calls, count) => {
    for (let index = 0; index < 10; index += 1) {
        if (calls.filter((call) => call.method === "session/prompt").length >= count) return;
        await new Promise((resolve) => setTimeout(resolve, 0));
    }
};

test("memory preset attaches the Markdown to hidden system context without exposing it in messages", async () => {
    const memoryPrompt = readFileSync(new URL("../../../prompts/memory-tuning-assistant.md", import.meta.url), "utf8").trim();
    const { service, calls, events, state } = createPromptTestService("Host instructions", undefined, undefined, memoryPrompt);
    const text = "使用内存分析助手帮我分析当前数据";

    const result = await service.prompt(text, { sessionId: "session-1", promptPreset: "memory-tuning-assistant" });
    assert.equal(result.ok, true);
    await waitForPromptCompletion(state);

    const prompt = calls.find((call) => call.method === "session/prompt").params.prompt;
    assert.equal(prompt[0].resource.uri, "insight-system-prompt://project");
    assert.equal(prompt[0].resource.text, `<context ref="insight-system-prompt://project"/>\nHost instructions\n\n${memoryPrompt}`);
    assert.equal(prompt[1].resource.uri, "insight-hidden-context://project");
    assert.deepEqual(prompt.filter((block) => block.type === "text"), [{ type: "text", text }]);
    const user = events.find((event) => event.type === "message_added" && event.message.role === "user").message;
    assert.equal(user.content.length, 1);
    assert.equal(user.content[0].text, text);
    assert.equal(JSON.stringify(events).includes("pt-snap"), false);
    assert.equal(JSON.stringify(state.sessionContexts.get("session-1").messages).includes("pt-snap"), false);

    await service.prompt("ordinary follow-up", { sessionId: "session-1" });
    await waitForPromptCompletion(state);
    const next = calls.filter((call) => call.method === "session/prompt")[1].params.prompt;
    assert.equal(next[0].resource.text, '<context ref="insight-system-prompt://project"/>\nHost instructions');
});

test("rejects unknown presets and missing memory instructions before sending a prompt", async () => {
    const { service, calls, events } = createPromptTestService();
    for (const promptPreset of ["../../private", "memory-tuning-assistant"]) {
        const result = await service.prompt("analyze", { sessionId: "session-1", promptPreset });
        assert.equal(result.status, promptPreset === "memory-tuning-assistant" ? 503 : 400);
    }
    assert.equal(calls.length, 0);
    assert.equal(events.length, 0);
});

test("immediate runtime configuration errors end the prompt and clear agent activity", async () => {
    const error = "AI SDK runtime is not configured. Set MSINSIGHT_NATIVE_API_KEY.";
    const { service, events, state } = createPromptTestService("", async () => { throw new Error(error); });

    await service.prompt("analyze data", { sessionId: "session-1" });
    await waitForPromptCompletion(state);

    const session = state.sessionContexts.get("session-1");
    assert.equal(session.pendingPrompt, false);
    const assistant = session.messages.find((message) => message.role === "assistant");
    assert.equal(assistant.activity, undefined);
    assert.equal(assistant.content[0].text, `Error: ${error}`);
    assert.equal(assistant.completionStatus, "failed");
    assert.equal(events.filter((event) => event.type === "prompt_status").at(-1).completionStatus, "failed");
    assert.deepEqual(events.filter((event) => event.type === "prompt_status").map((event) => event.pendingPrompt), [true, false]);
});

test("cancelling a turn preserves its stopped state when the old request finishes during a new turn", async () => {
    const finish = [];
    const { service, state, events } = createPromptTestService("", () => new Promise((resolve) => finish.push(resolve)));
    await service.prompt("first", { sessionId: "session-1" });
    service.handleAcpNotification({ method: "session/update", params: { sessionId: "session-1", update: {
        kind: "agent_message_chunk", content: { type: "text", text: "Inspecting files" },
    } } });
    const firstAssistant = state.sessionContexts.get("session-1").messages.at(-1);
    await service.cancel("session-1");
    assert.equal(firstAssistant.completionStatus, "cancelled");
    assert.equal(events.filter((event) => event.type === "prompt_status").at(-1).completionStatus, "cancelled");

    await service.prompt("second", { sessionId: "session-1" });
    finish[0]({ stopReason: "cancelled" });
    await new Promise((resolve) => setImmediate(resolve));
    assert.equal(state.sessionContexts.get("session-1").pendingPrompt, true);
    assert.equal(events.filter((event) => event.type === "prompt_status").at(-1).pendingPrompt, true);
    assert.equal(firstAssistant.completionStatus, "cancelled");
    finish[1]({ stopReason: "end_turn" });
    await waitForPromptCompletion(state);
});

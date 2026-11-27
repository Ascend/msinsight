/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import { filterHistoryMessages, filterHistoryText } from "../../services/historyContentFilter.mjs";
import { createSessionService } from "../../services/sessionService.mjs";
import { createChatService } from "../../services/chatService.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const preset = "Private one-shot instructions";
const presetText = `<context ref="insight-assistant-preset://project"/>\n${preset}`;
const hiddenText = '<context ref="insight-hidden-context://project"/>\n'
    + JSON.stringify({ contextPolicy: "replace_previous_hidden_context", data: { text: 'escaped " and } {', nested: { value: true } } });

test("filters all Host resource types without mutating their input", () => {
    const input = [{ role: "user", content: [
        ...["assistant-preset", "system-prompt", "hidden-context"].map((kind) => ({
            type: "resource", resource: { uri: `insight-${kind}://project`, text: "private" },
        })),
        { type: "text", text: "visible" },
    ] }];
    const original = structuredClone(input);
    assert.deepEqual(filterHistoryMessages(input)[0].content, [{ type: "text", text: "visible" }]);
    assert.deepEqual(input, original);
});

test("known preset and hidden JSON can share a replay message with visible user input", () => {
    assert.equal(filterHistoryText(`${presetText}\n${hiddenText}\nAnalyze rank 3`, [preset]), "Analyze rank 3");
    assert.equal(filterHistoryText(`${hiddenText}\nCompare rank 0`), "Compare rank 0");
});

test("paired context tags preserve surrounding visible text even after a long prefix", () => {
    const prefix = "Visible explanation. ".repeat(30);
    assert.equal(filterHistoryText(`${prefix}<context ref="insight-assistant-preset://project">private</context>done`), `${prefix}done`);
});

test("plain URI references later in normal text are not removed", () => {
    const text = `${"Visible text. ".repeat(30)}insight-assistant-preset://project is a URI`;
    assert.equal(filterHistoryText(text), text);
    assert.equal(filterHistoryText(`${"Visible text. ".repeat(30)}[insight-assistant-preset://project] is a URI`), `${"Visible text. ".repeat(30)}[insight-assistant-preset://project] is a URI`);
    assert.equal(filterHistoryText("normal text"), "normal text");
});

test("legacy and unknown unbounded hidden bodies are excluded rather than guessed", () => {
    for (const ref of ["assistant-preset", "system-prompt", "hidden-context"]) {
        assert.equal(filterHistoryText(`[insight-${ref}://project]\nprivate body`), "");
        assert.equal(filterHistoryText(`<context ref="insight-${ref}://project"/>\nprivate body`), "");
    }
});

test("every split position in the marker and body is filtered before publication", () => {
    for (const text of [presetText, hiddenText]) {
        for (let split = 1; split < text.length; split++) {
            const input = [{ role: "user", content: [text.slice(0, split), text.slice(split)] }];
            assert.deepEqual(filterHistoryMessages(input, [preset]), []);
        }
    }
});

test("different block IDs do not expose a split hidden marker or payload", () => {
    for (const type of ["text", "thinking"]) {
        for (const text of [presetText, hiddenText, '<context ref="insight-system-prompt://project">private</context>']) {
            for (let split = 1; split < text.length; split++) {
                const input = [{ role: "user", content: [
                    { id: "first", type, text: text.slice(0, split) },
                    { id: "second", type, text: text.slice(split) },
                    { id: "visible", type, text: "\nQuestion" },
                ] }];
                const original = structuredClone(input);
                assert.deepEqual(filterHistoryMessages(input, [preset])[0].content, [{ id: "visible", type, text: "Question" }]);
                assert.deepEqual(input, original);
            }
        }
    }
});

test("visible block identities survive filtering, and unbounded hidden tails cannot escape by changing IDs", () => {
    const visible = [{ id: "a", type: "text", text: "First" }, { id: "b", type: "text", text: "Second" }];
    assert.deepEqual(filterHistoryMessages([{ role: "user", content: visible }])[0].content, visible);
    assert.deepEqual(filterHistoryMessages([{ role: "user", content: [
        { id: "a", type: "text", text: '<context ref="insight-assistant-preset://project"/>\nUnknown' },
        { id: "b", type: "text", text: " hidden tail" },
    ] }]), []);
    assert.equal(filterHistoryMessages([{ role: "user", content: [{ type: "text" }, { type: "text", text: "Visible" }] }])[0].content[0].text, "Visible");
});

test("preserves independent messages, images, external resources, thinking, tools and empty messages", () => {
    const input = [
        { role: "user", content: [{ type: "text", text: presetText }] },
        { role: "user", content: [{ id: "visible", type: "text", text: "question" }, { type: "image", data: "a", mimeType: "image/png" }] },
        { role: "assistant", content: [{ type: "thinking", text: "reason" }, { type: "tool", toolCall: { toolCallId: "t", output: "private-looking tool output" } }, { type: "text", text: "answer" }] },
        { role: "user", content: [{ type: "resource", resource: { uri: "file:///data", text: "resource body" } }] },
        { role: "assistant", content: [] },
    ];
    assert.deepEqual(filterHistoryMessages(input, [preset]), input.slice(1));
    assert.deepEqual(filterHistoryMessages([{ role: "user", content: "Visible string" }])[0].content, [{ type: "text", text: "Visible string" }]);
});

for (const refreshConfigOnly of [false, true]) {
    test(`real session/load filters fragmented history without broadcasting hidden content: configOnly=${refreshConfigOnly}`, async () => {
        const state = createRuntimeState();
        state.agentCapabilities = { loadSession: true };
        const events = [];
        const eventBus = { broadcast: (event) => events.push(structuredClone(event)) };
        let chatService;
        const sessionService = createSessionService({
            state, eventBus, config: { cwd: "/test", slowCardAnalysisPrompt: preset },
            acpClient: { async request(method, { sessionId }) {
                assert.equal(method, "session/load");
                for (const text of [presetText.slice(0, 20), presetText.slice(20), hiddenText.slice(0, 45), hiddenText.slice(45), "\nAnalyze rank 3"]) {
                    chatService.handleAcpNotification({ method: "session/update", params: {
                        sessionId, update: { sessionUpdate: "user_message_chunk", content: { type: "text", text } },
                    } });
                }
                chatService.handleAcpNotification({ method: "session/update", params: {
                    sessionId, update: { sessionUpdate: "agent_message_chunk", content: { type: "text", text: "answer" } },
                } });
                assert.equal(events.some((event) => event.type.startsWith("message_")), false);
                assert.equal((await sessionService.exportSessionById(sessionId)).error, "session_busy");
                return { configOptions: [] };
            } },
        });
        chatService = createChatService({ state, eventBus, sessionService, slowCardAnalysisPrompt: preset });
        const result = await sessionService.loadSessionById("s", { refreshConfigOnly });
        assert.equal(result.ok, true);
        assert.deepEqual(result.messages.map((message) => message.content.map((block) => block.text).join("")), ["Analyze rank 3", "answer"]);
        assert.equal(state.sessionContexts.get("s").collectingHistory, false);
    });
}

test("failed replay publishes no partial hidden history and clears collection state", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { loadSession: true };
    const events = [];
    let context;
    const service = createSessionService({
        state, config: {}, eventBus: { broadcast: (event) => events.push(event) },
        acpClient: { async request() {
            context = state.sessionContexts.get("s");
            context.messages.push({ role: "user", content: [{ type: "text", text: presetText }] });
            throw new Error("load failed");
        } },
    });
    assert.equal((await service.loadSessionById("s")).error, "session_load_failed");
    assert.deepEqual(context.messages, []);
    assert.equal(context.collectingHistory, false);
    assert.deepEqual(events, []);
});

test("overlapping loads cannot reset history or clear the active collection flag", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { loadSession: true };
    let finish;
    let requests = 0;
    const service = createSessionService({
        state, config: {}, eventBus: { broadcast() {} },
        acpClient: { async request() {
            requests++;
            await new Promise((resolve) => { finish = resolve; });
            return {};
        } },
    });
    const first = service.loadSessionById("s");
    const context = state.sessionContexts.get("s");
    context.messages.push({ role: "user", content: [{ type: "text", text: "Question" }] });
    try {
        assert.equal((await service.loadSessionById("s", { refreshConfigOnly: true })).error, "session_busy");
        assert.equal(requests, 1);
        assert.equal(context.collectingHistory, true);
        assert.equal(context.messages.length, 1);
        assert.equal((await service.exportSessionById("s")).error, "session_busy");
    } finally {
        finish();
        await first;
    }
    assert.equal(context.collectingHistory, false);
    assert.equal(context.messages[0].content[0].text, "Question");
});

test("explicit replay message boundaries protect the next user input and normal streaming remains immediate", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { loadSession: true };
    const events = [];
    const eventBus = { broadcast: (event) => events.push(structuredClone(event)) };
    let chatService;
    const send = (messageId, id, text) => chatService.handleAcpNotification({ method: "session/update", params: {
        sessionId: "s", update: { sessionUpdate: "user_message_chunk", messageId, content: { id, type: "text", text } },
    } });
    const sessionService = createSessionService({
        state, eventBus, config: {}, acpClient: { async request() {
            send("private", "hidden", '<context ref="insight-assistant-preset://project"/>\nOld unknown instructions');
            send("base", "hidden", '<context ref="insight-system-prompt://project"/>\nOld base rules');
            send("question", "visible", "Question");
            return {};
        } },
    });
    chatService = createChatService({ state, eventBus, sessionService });
    const result = await sessionService.loadSessionById("s");
    assert.equal(result.messages.length, 1);
    assert.equal(result.messages[0].id, "question");
    assert.equal(result.messages[0].content[0].text, "Question");
    state.sessionContexts.get("s").pendingPrompt = true;
    chatService.handleAcpNotification({ method: "session/update", params: {
        sessionId: "s", update: { sessionUpdate: "agent_message_chunk", content: { type: "text", text: "Live answer" } },
    } });
    assert.ok(events.some((event) => event.type === "message_content_added" && event.block.text === "Live answer"));
});

test("replay tool without message ID and following identified answer share one assistant message", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { loadSession: true };
    const events = [];
    const eventBus = { broadcast: (event) => events.push(structuredClone(event)) };
    let chatService;
    const sessionService = createSessionService({
        state, eventBus, config: {}, acpClient: { async request() {
            for (const update of [
                { sessionUpdate: "tool_call", toolCallId: "call", title: "Tool", status: "in_progress" },
                { sessionUpdate: "agent_message_chunk", messageId: "answer", content: { type: "text", text: "Result" } },
                { sessionUpdate: "tool_call_update", toolCallId: "call", status: "completed" },
            ]) {
                chatService.handleAcpNotification({ method: "session/update", params: { sessionId: "s", update } });
            }
            return {};
        } },
    });
    chatService = createChatService({ state, eventBus, sessionService });
    const result = await sessionService.loadSessionById("s");
    assert.equal(result.messages.length, 1);
    assert.equal(result.messages[0].id, "answer");
    assert.deepEqual(result.messages[0].content.map((block) => block.type), ["tool", "text"]);
    assert.equal(result.messages[0].content[0].toolCall.status, "completed");
    assert.equal(events.some((event) => event.type.startsWith("message_")), false);
});

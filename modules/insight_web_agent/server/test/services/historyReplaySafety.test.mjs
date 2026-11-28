/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import { setImmediate as yieldTurn } from "node:timers/promises";
import { filterHistoryMessages, filterHistoryText } from "../../services/historyContentFilter.mjs";
import { createChatService } from "../../services/chatService.mjs";
import { createSessionService } from "../../services/sessionService.mjs";
import { createSessionManager } from "../../services/sessionManager.mjs";
import { isBusy } from "../../services/agentConfigService.mjs";
import { isRuntimeBusy, withRuntimeReload } from "../../services/runtimeOperationService.mjs";
import { createRuntimeState, snapshotRuntimeState, restoreRuntimeState } from "../../state/runtimeState.mjs";

const deferred = () => {
    let resolve;
    let reject;
    const promise = new Promise((success, failure) => {
        resolve = success;
        reject = failure;
    });
    return { promise, resolve, reject };
};

const fixture = ({ request = async () => ({}), contextAssembler, preset = "" } = {}) => {
    const state = createRuntimeState();
    state.initialized = true;
    state.agentCapabilities = { loadSession: true };
    const events = [];
    const eventBus = { broadcast: (event) => events.push(structuredClone(event)) };
    const adapter = { request };
    const sessionService = createSessionService({ state, eventBus, acpClient: adapter, config: { slowCardAnalysisPrompt: preset } });
    const chat = createChatService({ state, eventBus, acpClient: adapter, sessionService, contextAssembler, slowCardAnalysisPrompt: preset });
    const notify = (update) => chat.handleAcpNotification({ method: "session/update", params: { sessionId: "s", update } });
    return { state, events, adapter, sessionService, chat, notify };
};

test("long bracketed preset cannot expose subsequent hidden JSON at any block split", () => {
    const preset = "private preset ".repeat(30).trim();
    const source = `[insight-assistant-preset://project]\n${preset}\n[insight-hidden-context://project]\n`
        + '{"contextPolicy":"replace_previous_hidden_context","data":{"secret":"PROJECT_SECRET"}}\nQuestion';
    assert.equal(filterHistoryText(source, [preset]), "Question");
    for (let split = 1; split < source.length; split++) {
        const messages = [{ role: "user", content: [
            { id: "one", type: "text", text: source.slice(0, split) },
            { id: "two", type: "text", text: source.slice(split) },
        ] }];
        assert.equal(filterHistoryMessages(messages, [preset])[0].content.map((block) => block.text).join(""), "Question");
    }
});

test("short ordinary URI references survive history and real-time display", () => {
    const f = fixture();
    f.state.sessionContexts.set("s", { messages: [], pendingPrompt: true });
    for (const kind of ["assistant-preset", "hidden-context", "system-prompt"]) {
        for (const reference of [`insight-${kind}://project`, `[insight-${kind}://project]`]) {
            const text = `Please explain ${reference} and its purpose.`;
            assert.equal(filterHistoryText(text), text);
            assert.equal(filterHistoryMessages([{ role: "assistant", content: text }])[0].content[0].text, text);
            f.notify({ sessionUpdate: "agent_message_chunk", content: { type: "text", text } });
            assert.ok(f.events.some((event) => event.block?.text === text || event.delta === text));
        }
    }
});

test("legacy bare headers require recognizable payloads rather than a URI mention", () => {
    const source = 'insight-hidden-context://project\n{"data":{"secret":"PRIVATE"}}\nQuestion';
    assert.equal(filterHistoryText(source), "Question");
    assert.equal(filterHistoryText("insight-assistant-preset://project\nKnown instructions\nQuestion", ["Known instructions"]), "Question");
    const explanation = "insight-assistant-preset://project\nThis URI identifies a one-shot preset.";
    assert.equal(filterHistoryText(explanation), explanation);
});

test("standalone unknown bracketed resource headers remain private beyond character 200", () => {
    const prefix = "Visible explanation. ".repeat(30);
    assert.equal(filterHistoryText(`${prefix}\n[insight-assistant-preset://project]\nUnknown private instructions`), `${prefix}\n`);
    assert.equal(filterHistoryText(`${prefix}[insight-assistant-preset://project] is a URI`), `${prefix}[insight-assistant-preset://project] is a URI`);
});

test("delayed tool updates retain their original message after a second identified answer", async () => {
    let f;
    f = fixture({ request: async () => {
        for (const update of [
            { sessionUpdate: "agent_message_chunk", messageId: "first", content: { type: "text", text: "First" } },
            { sessionUpdate: "tool_call", toolCallId: "call", title: "Original tool", status: "in_progress", rawInput: { key: "value" } },
            { sessionUpdate: "agent_message_chunk", messageId: "second", content: { type: "text", text: "Second" } },
            { sessionUpdate: "tool_call_update", toolCallId: "call", status: "completed", rawOutput: "Done" },
        ]) {
            f.notify(update);
        }
        return {};
    } });
    const result = await f.sessionService.loadSessionById("s");
    const blocks = result.messages.flatMap((message) => message.content.filter((block) => block.type === "tool"));
    assert.equal(blocks.length, 1);
    assert.equal(blocks[0].toolCall.status, "completed");
    assert.equal(blocks[0].toolCall.name, "Original tool");
    assert.equal(blocks[0].toolCall.output, "Done");
    assert.equal(result.messages[0].id, "first");
    assert.ok(result.messages[0].content.includes(blocks[0]));
    assert.equal(result.messages[1].id, "second");
});

test("history load owns the session until success or failure and rejects prompts", async () => {
    for (const fail of [false, true]) {
        const gate = deferred();
        const f = fixture({ request: async (method) => {
            assert.equal(method, "session/load");
            await gate.promise;
            return {};
        } });
        const loading = f.sessionService.loadSessionById("s");
        assert.equal((await f.chat.prompt("Question", { sessionId: "s" })).status, 409);
        assert.equal(isBusy(f.state), true);
        await assert.rejects(withRuntimeReload(f.state, async () => assert.fail("must not switch")), { code: "agent_busy" });
        assert.deepEqual(f.events, []);
        if (fail) {
            gate.reject(new Error("load failed"));
        } else {
            gate.resolve();
        }
        const result = await loading;
        assert.equal(Boolean(result.error), fail);
        assert.equal(isRuntimeBusy(f.state), false);
        assert.equal(await withRuntimeReload(f.state, async () => "ready"), "ready");
    }
});

test("prompt owns the session before asynchronous preparation and rejects load and reload", async () => {
    const preparation = deferred();
    const completion = deferred();
    const f = fixture({ contextAssembler: { assemble: async () => preparation.promise }, request: async (method) => {
        assert.equal(method, "session/prompt");
        await completion.promise;
        return {};
    } });
    const preparing = f.chat.prompt("Question", { sessionId: "s" });
    assert.equal(f.state.sessionContexts.get("s").pendingPrompt, true);
    assert.equal((await f.sessionService.loadSessionById("s")).status, 409);
    assert.equal((await f.chat.prompt("Second", { sessionId: "s" })).status, 409);
    await assert.rejects(withRuntimeReload(f.state, async () => assert.fail("must not reload")), { code: "agent_busy" });
    preparation.resolve();
    assert.equal((await preparing).ok, true);
    assert.equal((await f.sessionService.loadSessionById("s")).status, 409);
    f.notify({ sessionUpdate: "agent_message_chunk", content: { type: "text", text: "Live reply" } });
    assert.ok(f.events.some((event) => event.block?.text === "Live reply"));
    completion.resolve();
    await yieldTurn();
    assert.equal(isRuntimeBusy(f.state), false);
});

test("failed prompt preparation releases only its own pending state", async () => {
    const f = fixture({ contextAssembler: { assemble: async () => { throw new Error("preparation failed"); } } });
    assert.equal((await f.chat.prompt("Question", { sessionId: "s" })).error, "prompt_setup_failed");
    assert.equal(isRuntimeBusy(f.state), false);
    assert.equal((await f.sessionService.loadSessionById("s")).ok, true);
});

test("runtime reload keeps its claim through rollback and releases it on failure", async () => {
    const gate = deferred();
    const f = fixture();
    const reload = withRuntimeReload(f.state, async () => {
        const snapshot = snapshotRuntimeState(f.state);
        await gate.promise;
        restoreRuntimeState(f.state, snapshot);
        assert.equal(f.state.runtimeReloading, true);
        throw new Error("reload failed");
    });
    assert.equal((await f.sessionService.loadSessionById("s")).status, 409);
    assert.equal((await f.chat.prompt("Question", { sessionId: "s" })).status, 409);
    await assert.rejects(withRuntimeReload(f.state, async () => assert.fail("must not reload twice")), { code: "agent_busy" });
    gate.resolve();
    await assert.rejects(reload, /reload failed/);
    assert.equal(f.state.runtimeReloading, false);
    assert.equal((await f.sessionService.loadSessionById("s")).ok, true);
});

test("snapshots never restore operation-owned history flags", async () => {
    const gate = deferred();
    const f = fixture({ request: async () => { await gate.promise; return {}; } });
    const loading = f.sessionService.loadSessionById("s");
    const original = f.state.sessionContexts.get("s");
    const snapshot = snapshotRuntimeState(f.state);
    restoreRuntimeState(f.state, snapshot);
    gate.resolve();
    await loading;
    assert.equal(original.collectingHistory, false);
    const restored = f.state.sessionContexts.get("s");
    assert.notEqual(original, restored);
    for (const key of ["collectingHistory", "replayingHistory", "refreshingConfigOnly"]) {
        assert.equal(restored[key], false);
    }
    assert.equal((await f.sessionService.exportSessionById("s")).ok, true);
});

test("session manager also rejects prompts during history replay or runtime reload", async () => {
    const f = fixture({ request: async () => assert.fail("must not prompt") });
    const manager = createSessionManager({ adapter: f.adapter, state: f.state, config: {} });
    f.state.sessionContexts.set("s", { messages: [], collectingHistory: true });
    await assert.rejects(manager.pushUserMessage("s", { text: "Question" }), /another session operation/);
    f.state.sessionContexts.get("s").collectingHistory = false;
    f.state.runtimeReloading = true;
    await assert.rejects(manager.pushUserMessage("s", { text: "Question" }), /another session operation/);
});

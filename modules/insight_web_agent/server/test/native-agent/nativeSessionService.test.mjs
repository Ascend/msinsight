/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { createNativeSessionService } from "../../native-agent/session/sessionService.mjs";

const agents = [
    agent("general", "all"),
    agent("memory-tuning-assistant", "primary"),
    agent("worker", "subagent"),
];

const agentRegistry = {
    list: () => agents.map(({ body: _body, bashRules: _bashRules, fingerprint: _fingerprint, ...definition }) => definition),
    getPrimary: (id) => agents.find((definition) => definition.id === id && definition.mode !== "subagent"),
    diagnostics: () => [],
};

const createContext = (sessions = new Map()) => {
    const saves = [];
    const replayed = [];
    const restoreCalls = [];
    return {
        context: {
            sessions,
            modelCatalog: { defaultModelId: "model-a", models: [{ id: "model-a", name: "Model A" }, { id: "model-b", name: "Model B" }] },
            running: new Map(),
            sessionStore: { save: async () => { saves.push(true); } },
            filesystem: {
                createSessionFilesystemRoots: () => ["D:/workspace"],
                canonicalizeFilesystemRoots: async (roots) => roots,
                updateSessionFilesystemRoots: async () => false,
            },
            aiRuntime: {
                restoreSession: async (session) => { restoreCalls.push(session.sessionId); },
                runPrompt: async () => ({ ok: true }),
                updateFilesystemContext: () => {},
                abortSession: () => {},
                deleteSession: async () => {},
            },
            agentRegistry,
            skillRegistry: { list: () => [], diagnostics: () => [] },
            toolRegistry: { execute: async () => ({}), list: () => [] },
            notifier: {
                sendSessionContentChunk: (...args) => replayed.push(args),
                sendSessionThinkingChunk: (...args) => replayed.push(args),
                sendToolCallUpdate: (...args) => replayed.push(args),
                sendSessionChunk: (...args) => replayed.push(args),
            },
        },
        saves,
        replayed,
        restoreCalls,
    };
};

test("new native sessions default to general and expose Primary Agent options", async () => {
    const fixture = createContext();
    const service = createNativeSessionService(fixture.context);

    const result = await service.handleRequest("session/new", {});
    const session = fixture.context.sessions.get(result.sessionId);
    const option = result.configOptions[0];

    assert.equal(session.primaryAgentId, "general");
    assert.equal(session.promptStarted, false);
    assert.equal(option.id, "primaryAgent");
    assert.equal(option.category, "mode");
    assert.equal(option.currentValue, "general");
    assert.deepEqual(option.options.map(({ value }) => value), ["general", "memory-tuning-assistant"]);
    assert.equal(fixture.saves.length, 1);
});

test("Primary Agent can be bound once before prompting and not after prompt start", async () => {
    const fixture = createContext();
    const service = createNativeSessionService(fixture.context);
    const { sessionId } = await service.handleRequest("session/new", {});

    const result = await service.handleRequest("session/set_config_option", {
        sessionId,
        configId: "primaryAgent",
        value: "memory-tuning-assistant",
    });
    const session = fixture.context.sessions.get(sessionId);

    assert.equal(session.primaryAgentId, "memory-tuning-assistant");
    assert.equal(session.primaryAgentFingerprint, "memory-tuning-assistant-fingerprint");
    assert.equal(result.configOptions[0].currentValue, "memory-tuning-assistant");

    session.promptStarted = true;
    await assert.rejects(service.handleRequest("session/set_config_option", {
        sessionId,
        configId: "primaryAgent",
        value: "general",
    }), /cannot be changed because this session already has conversation history/);
});

test("subagent and unknown IDs cannot be bound as Primary Agents", async () => {
    const fixture = createContext();
    const service = createNativeSessionService(fixture.context);
    const { sessionId } = await service.handleRequest("session/new", {});

    for (const value of ["worker", "missing"]) {
        await assert.rejects(service.handleRequest("session/set_config_option", {
            sessionId,
            configId: "primaryAgent",
            value,
        }), new RegExp(`Primary Agent is unavailable: ${value}`));
    }
});

test("native prompts return ACP stop reasons", async () => {
    const fixture = createContext();
    const service = createNativeSessionService(fixture.context);
    const { sessionId } = await service.handleRequest("session/new", {});

    assert.deepEqual(await service.handleRequest("session/prompt", {
        sessionId,
        prompt: [{ type: "text", text: "analyze" }],
    }), { stopReason: "end_turn" });
});

test("session load replays assistant content blocks in stored order", async () => {
    const toolCall = { toolCallId: "call-1", name: "Read", status: "completed" };
    const session = {
        sessionId: "session-1",
        title: "Stored session",
        messages: [{
            id: "assistant-1",
            role: "assistant",
            content: [
                { id: "text-1", type: "text", text: "before" },
                { id: "call-1", type: "tool", toolCall },
                { id: "text-2", type: "text", text: "after" },
            ],
        }],
        primaryAgentId: "general",
        primaryAgentFingerprint: "general-fingerprint",
        promptStarted: true,
        updatedAt: Date.now(),
    };
    const fixture = createContext(new Map([[session.sessionId, session]]));
    const service = createNativeSessionService(fixture.context);

    await service.handleRequest("session/load", { sessionId: session.sessionId });

    assert.deepEqual(fixture.replayed, [
        [session.sessionId, "agent_message_chunk", "before"],
        [session.sessionId, "tool_call", toolCall],
        [session.sessionId, "agent_message_chunk", "after"],
    ]);
});

test("missing bound Agent allows history load but blocks future prompts", async () => {
    const session = {
        sessionId: "session-1",
        title: "Stored session",
        messages: [{ id: "message-1", role: "user", content: [{ id: "text-1", type: "text", text: "previous question" }] }],
        primaryAgentId: "removed-agent",
        primaryAgentFingerprint: "removed-fingerprint",
        promptStarted: true,
        updatedAt: Date.now(),
    };
    const fixture = createContext(new Map([[session.sessionId, session]]));
    const service = createNativeSessionService(fixture.context);

    const loaded = await service.handleRequest("session/load", { sessionId: session.sessionId });

    assert.equal(loaded.sessionId, session.sessionId);
    assert.deepEqual(loaded.configOptions[0].options.find(({ value }) => value === "removed-agent"), {
        value: "removed-agent",
        name: "Removed Agent [Unavailable]",
        description: "Primary Agent is unavailable: removed-agent",
        _meta: {
            "msinsight.dev/source": { id: "missing", kind: "bundled" },
            "msinsight.dev/available": false,
            "msinsight.dev/diagnostics": [{
                code: "AGENT_MISSING",
                message: "Primary Agent is unavailable: removed-agent",
                resourceId: "removed-agent",
            }],
        },
    });
    assert.equal(fixture.restoreCalls.length, 1);
    assert.deepEqual(fixture.replayed[0], [session.sessionId, "user_message_chunk", "previous question"]);
    await assert.rejects(service.handleRequest("session/prompt", {
        sessionId: session.sessionId,
        prompt: [{ type: "text", text: "continue" }],
    }), /Primary Agent is unavailable: removed-agent/);
});

test("models default, switch with history, load, reject busy and never reset runtime", async () => {
    const { context } = createContext();
    context.aiRuntime.deleteSession = () => assert.fail("model switch must preserve runtime");
    const service = createNativeSessionService(context);
    const first = await service.handleRequest("session/new", {});
    const second = await service.handleRequest("session/new", {});
    assert.equal(first.configOptions[1].currentValue, "model-a");
    assert.deepEqual(first.configOptions[1].options, [{ value: "model-a", name: "Model A" }, { value: "model-b", name: "Model B" }]);
    const session = context.sessions.get(first.sessionId);
    const history = [{ role: "user", content: [] }];
    session.messages = history;
    session.promptStarted = true;
    session.runtimeSession = { retained: true };
    const switched = await service.handleRequest("session/set_config_option", { sessionId: first.sessionId, configId: "model", value: "model-b" });
    assert.equal(switched.configOptions[1].currentValue, "model-b");
    assert.equal(session.messages, history);
    assert.equal(session.runtimeSession.retained, true);
    assert.equal(context.sessions.get(second.sessionId).modelId, "model-a");
    assert.equal((await service.handleRequest("session/load", { sessionId: first.sessionId })).configOptions[1].currentValue, "model-b");
    context.running.set(first.sessionId, {});
    await assert.rejects(service.handleRequest("session/set_config_option", { sessionId: first.sessionId, configId: "model", value: "model-a" }), /while a prompt is running/);
    assert.equal(session.modelId, "model-b");
    context.running.clear();
    await assert.rejects(service.handleRequest("session/set_config_option", { sessionId: first.sessionId, configId: "model", value: "invalid" }), /Model is unavailable/);
    const publicConfig = JSON.stringify(switched);
    for (const key of ["apiKey", "provider", "baseUrl", "MSINSIGHT_NATIVE"]) assert.equal(publicConfig.includes(key), false);
});

test("old session migrates only missing model IDs; deleted IDs fail load and prompt", async () => {
    const { context } = createContext();
    const service = createNativeSessionService(context);
    const { sessionId } = await service.handleRequest("session/new", {});
    const session = context.sessions.get(sessionId);
    delete session.modelId;
    assert.equal((await service.handleRequest("session/load", { sessionId })).configOptions[1].currentValue, "model-a");
    session.modelId = "removed";
    for (const method of ["session/load", "session/prompt"]) {
        await assert.rejects(service.handleRequest(method, { sessionId, prompt: [] }), /Model is unavailable: removed/);
        assert.equal(session.modelId, "removed");
    }
});

test("failed model persistence rolls back and blocks overlapping prompts and switches", async () => {
    const { context } = createContext();
    const service = createNativeSessionService(context);
    const { sessionId } = await service.handleRequest("session/new", {});
    let rejectSave;
    context.sessionStore.save = () => new Promise((_resolve, reject) => { rejectSave = reject; });
    const pending = service.handleRequest("session/set_config_option", { sessionId, configId: "model", value: "model-b" });
    await assert.rejects(service.handleRequest("session/prompt", { sessionId, prompt: [] }), /still being saved/);
    await assert.rejects(service.handleRequest("session/set_config_option", { sessionId, configId: "model", value: "model-a" }), /still being saved/);
    rejectSave(new Error("disk failure"));
    await assert.rejects(pending, /disk failure/);
    assert.equal(context.sessions.get(sessionId).modelId, "model-a");
});

function agent(id, mode) {
    return {
        id,
        mode,
        description: `${id} description`,
        body: `${id} instructions`,
        bashRules: [{ pattern: "*", behavior: "ask" }],
        fingerprint: `${id}-fingerprint`,
        source: { id: "bundled", kind: "bundled" },
        diagnostics: [],
    };
}

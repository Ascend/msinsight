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
import { createRuntimeState } from "../../state/runtimeState.mjs";
import { createSessionService } from "../../services/sessionService.mjs";
import { createChatService } from "../../services/chatService.mjs";

for (const canDelete of [true, false]) {
    test(`welcome configuration loads before initialization completes (session deletion: ${canDelete})`, async () => {
        const state = createRuntimeState();
        const calls = [];
        const events = [];
        let created = 0;
        let currentModel = "model-a";
        let currentMode = "default";
        const configOptions = () => [modeConfig(currentMode), {
            id: "model", category: "model", type: "select", currentValue: currentModel,
            options: [{ value: "model-a", name: "Model A" }, { value: "model-b", name: "Model B" }],
        }];
        const acpClient = {
            async request(method, params) {
                calls.push({ method, params });
                if (method === "initialize") return { agentCapabilities: { sessionCapabilities: { list: true, delete: canDelete, setConfigOption: true } } };
                if (method === "session/new") return { sessionId: `session-${++created}`, configOptions: configOptions() };
                if (method === "session/delete") return {};
                if (method === "session/list") return { sessions: [{ sessionId: "session-1" }] };
                if (method === "session/set_config_option") {
                    if (params.configId === "model") currentModel = params.value;
                    if (params.configId === "mode") currentMode = params.value;
                    return { configOptions: configOptions() };
                }
                if (method === "session/prompt") return { stopReason: "end_turn" };
                throw new Error(`Unexpected method: ${method}`);
            },
        };
        const eventBus = { broadcast: (event) => events.push(event) };
        const sessionService = createSessionService({ acpClient, config: { cwd: "/tmp", defaultModel: "model-b" }, eventBus, state });
        const chatService = createChatService({ acpClient, sessionService, eventBus, state });

        await chatService.initialize();

        assert.equal(state.initialized, true);
        assert.equal(state.configOptions[1].currentValue, "model-b");
        assert.deepEqual(state.sessions, []);
        assert.equal(state.sessionContexts.size, 0);
        assert.equal(calls.some(({ method }) => method === "session/delete"), canDelete);
        assert.equal(calls.some(({ method }) => method === "session/prompt"), false);
        assert.equal(events.find(({ type }) => type === "state").state.configOptions.length, 2);

        await sessionService.setModel("model-b");
        await chatService.prompt("hello", { newSession: true, mode: "bypass" });

        assert.equal(currentModel, "model-b");
        assert.equal(currentMode, "bypass");
        assert.deepEqual(calls.filter(({ method }) => method === "session/set_config_option").map(({ params }) => params), [
            { sessionId: "session-2", configId: "model", value: "model-b" },
            { sessionId: "session-2", configId: "mode", value: "bypass" },
        ]);
        assert.equal(calls.find(({ method }) => method === "session/prompt").params.sessionId, "session-2");
    });
}

test("configuration preload uses the staged adapter without broadcasting during an agent switch", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { session: { delete: true } };
    const methods = [];
    const service = createSessionService({
        acpClient: { request: () => assert.fail("must not call the previous agent") },
        config: {},
        eventBus: { broadcast: () => assert.fail("must not publish staged state") },
        state,
    });

    await service.loadConfigOptions({
        broadcast: false,
        targetAdapter: { request: async (method) => {
            methods.push(method);
            return { sessionId: "temporary", config_options: [modeConfig("default")] };
        } },
    });

    assert.deepEqual(methods, ["session/new", "session/delete"]);
    assert.equal(state.configOptions[0].currentValue, "default");
});

test("configuration preload failure does not prevent agent initialization", async () => {
    const state = createRuntimeState();
    const eventBus = { broadcast: () => {} };
    const acpClient = { request: async (method) => {
        if (method === "initialize") return {};
        throw new Error("Configuration unavailable");
    } };
    const sessionService = createSessionService({ acpClient, config: {}, eventBus, state });
    const service = createChatService({ acpClient, sessionService, eventBus, state });

    await service.initialize();

    assert.equal(state.initialized, true);
    assert.deepEqual(state.configOptions, []);
});

test("setMode sends the session mode config option to ACP", async () => {
    const calls = [];
    const state = createRuntimeState();
    state.agentCapabilities = { session: { setConfigOption: true } };
    state.sessionContexts.set("session-1", {
        sessionId: "session-1",
        messages: [],
        pendingPrompt: false,
        configOptions: [modeConfig("default")],
    });

    const service = createSessionService({
        acpClient: {
            async request(method, params) {
                calls.push({ method, params });
                assert.equal(method, "session/set_config_option");
                assert.deepEqual(params, {
                    sessionId: "session-1",
                    configId: "mode",
                    value: "bypass",
                });
                return { configOptions: [modeConfig("bypass")] };
            },
        },
        config: {},
        eventBus: { broadcast: () => {} },
        state,
    });

    const result = await service.setMode("bypass", "session-1");

    assert.equal(result.ok, true);
    assert.equal(calls.length, 1);
    assert.equal(state.sessionContexts.get("session-1").configOptions[0].currentValue, "bypass");
});

test("loadSessionById injects the global capability MCP server", async () => {
    const calls = [];
    const state = createRuntimeState();
    state.agentCapabilities = { loadSession: true, session: {}, mcp: { http: true } };
    const mcpServers = [{ type: "http", name: "msinsight-capabilities", url: "http://127.0.0.1/mcp/capabilities" }];
    const service = createSessionService({
        acpClient: {
            async request(method, params) {
                calls.push({ method, params });
                return { configOptions: [] };
            },
        },
        capabilitySessionIntegration: { withMcpServers: (operation) => operation(mcpServers) },
        config: { cwd: "/tmp" },
        eventBus: { broadcast: () => {} },
        state,
    });

    await service.loadSessionById("session-1");

    assert.equal(calls[0].method, "session/load");
    assert.equal(calls[0].params.cwd, "/tmp");
    assert.deepEqual(calls[0].params.mcpServers, mcpServers);
});

test("deleteSessionById rejects deletion while a prompt is pending", async () => {
    const state = createRuntimeState();
    state.agentCapabilities = { session: { delete: true } };
    state.sessionContexts.set("session-1", {
        sessionId: "session-1",
        messages: [],
        pendingPrompt: true,
        configOptions: [],
    });
    const service = createSessionService({
        acpClient: { async request() { throw new Error("should not be called"); } },
        config: {},
        eventBus: { broadcast: () => {} },
        state,
    });

    const result = await service.deleteSessionById("session-1");

    assert.equal(result.status, 409);
    assert.equal(result.error, "session_busy");
    assert.equal(result.message, "The session cannot be deleted while a message is being processed");
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

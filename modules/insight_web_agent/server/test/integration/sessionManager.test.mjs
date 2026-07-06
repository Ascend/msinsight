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
import { strict as assert } from "node:assert";
import test from "node:test";
import { createSessionManager } from "../../services/sessionManager.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const createMockEventBus = () => ({ broadcast() {} });
const createConfig = () => ({ cwd: "/tmp/insight-agent", agentServer: { name: "fallback-agent" } });

const createMockAdapter = () => {
    const requests = [];
    return {
        agentId: "mock-agent",
        runtime: "stdio",
        requests,
        async request(method, params) {
            requests.push({ method, params });
            if (method === "session/new") return { sessionId: "session-1", configOptions: [{ id: "mode" }] };
            return { ok: true };
        },
    };
};

test("startSession returns a cloned SessionContext with contract fields", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });

    const context = await manager.startSession({
        agentId: "request-agent",
        mode: "performance_tuning",
        view: "Timeline",
        profileId: "profile-1",
        grants: ["read"],
    });

    assert.equal(context.sessionId, "session-1");
    assert.equal(context.agentId, "request-agent");
    assert.equal(context.runtime, "stdio");
    assert.equal(context.mode, "performance_tuning");
    assert.equal(context.view, "Timeline");
    assert.equal(context.profileId, "profile-1");
    assert.deepEqual([...context.grants], ["read"]);
    assert.notEqual(context, state.sessionContexts.get("session-1"));
});

test("endSession removes the session context", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });
    await manager.startSession({});

    const result = await manager.endSession("session-1");

    assert.deepEqual(result, { ok: true });
    assert.equal(state.sessionContexts.has("session-1"), false);
});

test("endSession rejects pending permissions and clears session allowlist", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const rejected = [];
    const permissionService = {
        rejectSessionRequests(sessionId, reason) {
            rejected.push({ sessionId, reason });
        },
    };
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig(), permissionService });
    await manager.startSession({});

    await manager.endSession("session-1");

    assert.deepEqual(rejected, [{ sessionId: "session-1", reason: "invalidated" }]);
});

test("endSession sends session/cancel before delete when a prompt is pending", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });
    await manager.startSession({});
    state.sessionContexts.get("session-1").pendingPrompt = true;

    await manager.endSession("session-1");

    assert.deepEqual(adapter.requests.slice(1).map((request) => request.method), ["session/cancel", "session/delete"]);
});

test("pushUserMessage sends a session/prompt request", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });
    await manager.startSession({});

    await manager.pushUserMessage("session-1", { text: "hi" });

    const promptRequest = adapter.requests.find((request) => request.method === "session/prompt");
    assert.equal(promptRequest.params.sessionId, "session-1");
    assert.deepEqual(promptRequest.params.prompt, [{ type: "text", text: "hi" }]);
});

test("endSession preserves local state when remote delete fails", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    adapter.request = async (method, params) => {
        adapter.requests.push({ method, params });
        if (method === "session/new") return { sessionId: "session-1" };
        if (method === "session/delete") throw new Error("transport down");
        return { ok: true };
    };
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });
    await manager.startSession({});

    const result = await manager.endSession("session-1");

    assert.equal(result.status, 500);
    assert.equal(state.sessionContexts.has("session-1"), true);
});

test("updateContext rebuilds hidden context for existing sessions", async () => {
    const state = createRuntimeState();
    const adapter = createMockAdapter();
    const contextAssembler = {
        async assemble(context) {
            return { profileId: context.profileId, view: context.view };
        },
    };
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig(), contextAssembler });
    await manager.startSession({ profileId: "old", view: "Timeline" });

    await manager.updateContext({ profileId: "new", activeModule: "Memory" });

    const context = state.sessionContexts.get("session-1");
    assert.equal(context.profileId, "new");
    assert.equal(context.view, "Memory");
    assert.deepEqual(context.hiddenContext, { kind: "AgentContextPacket", packet: { profileId: "new", view: "Memory" } });
});

test("pushUserMessage blocks concurrent prompts and clears pendingPrompt", async () => {
    const state = createRuntimeState();
    let releasePrompt;
    const adapter = createMockAdapter();
    adapter.request = async (method, params) => {
        adapter.requests.push({ method, params });
        if (method === "session/new") return { sessionId: "session-1" };
        if (method === "session/prompt") {
            await new Promise((resolve) => { releasePrompt = resolve; });
        }
        return { ok: true };
    };
    const manager = createSessionManager({ adapter, eventBus: createMockEventBus(), state, config: createConfig() });
    await manager.startSession({});

    const firstPrompt = manager.pushUserMessage("session-1", { text: "first" });
    await new Promise((resolve) => setImmediate(resolve));
    await assert.rejects(() => manager.pushUserMessage("session-1", { text: "second" }), /another prompt is running/);
    releasePrompt();
    await firstPrompt;

    assert.equal(state.sessionContexts.get("session-1").pendingPrompt, false);
});

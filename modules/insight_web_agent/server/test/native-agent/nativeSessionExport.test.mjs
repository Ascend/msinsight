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
import test from "node:test";
import { createNativeSessionService } from "../../native-agent/session/sessionService.mjs";

const forbidden = () => assert.fail("read-only export must not resolve configuration, save, or broadcast");
const fixture = () => {
    const session = { sessionId: "stored", title: "Original title", modelId: "removed-model", primaryAgentId: "removed-primary", primaryAgentFingerprint: "original", primaryAgentBody: "original body", updatedAt: 123, messages: [], hostSystemPrompt: "not public" };
    const uiMessages = [{ id: "m", role: "assistant", completionStatus: "completed", completedAt: 123, content: [{ id: "thinking", type: "thinking", text: "reason" }, { id: "tool", type: "tool", toolCall: { toolCallId: "tc", name: "Read", status: "completed", output: "full", durationMs: 12, custom: true } }, { id: "image", type: "image", mimeType: "image/png", data: "abc" }] }];
    const restores = [];
    const context = {
        sessions: new Map([[session.sessionId, session]]), running: new Map(),
        modelCatalog: { get models() { return forbidden(); } },
        agentRegistry: { getPrimary: forbidden, diagnostics: () => [] },
        skillRegistry: { list: () => [], diagnostics: () => [] },
        sessionStore: { save: forbidden },
        notifier: new Proxy({}, { get: () => forbidden }),
        aiRuntime: { restoreSession: async (snapshot) => { restores.push(snapshot); snapshot.runtimeSession = { kind: "ai-sdk", uiMessages }; snapshot.messages = uiMessages; } },
    };
    return { context, session, uiMessages, restores, service: createNativeSessionService(context) };
};

test("advertises dedicated read-only export and restores exact UI history without a model or Primary Agent", async () => {
    const { service, session, uiMessages, restores } = fixture();
    const before = structuredClone(session);
    const initialized = await service.handleRequest("initialize", {});
    assert.equal(initialized._meta["msinsight.dev/sessionExport"], true);
    const exported = await service.handleRequest("_msinsight/session/export", { sessionId: "stored" });
    assert.deepEqual(exported, { messages: uiMessages });
    assert.deepEqual(session, before);
    assert.equal(restores.length, 1);
    assert.notEqual(restores[0], session);
    assert.equal(restores[0].modelId, "removed-model");
    assert.equal(restores[0].primaryAgentFingerprint, "original");
    exported.messages[0].content[1].toolCall.output = "client mutation";
    assert.equal(uiMessages[0].content[1].toolCall.output, "full");
    assert.equal("hostSystemPrompt" in exported, false);
});

test("busy export refuses restore and never saves or replays notifications", async () => {
    const { service, context, restores } = fixture();
    context.running.set("stored", {});
    await assert.rejects(service.handleRequest("_msinsight/session/export", { sessionId: "stored" }), { code: "session_export_busy" });
    assert.equal(restores.length, 0);
    context.running.clear();
    context.sessions.get("stored").runtimeSession = { activeController: new AbortController() };
    await assert.rejects(service.handleRequest("_msinsight/session/export", { sessionId: "stored" }), { code: "session_export_busy" });
    assert.equal(restores.length, 0);
});

test("read-only export validates session and propagates corrupt restore without mutation", async () => {
    const { service, context, session } = fixture();
    await assert.rejects(service.handleRequest("_msinsight/session/export", {}), /sessionId is required/);
    await assert.rejects(service.handleRequest("_msinsight/session/export", { sessionId: "missing" }), /session not found/);
    const before = structuredClone(session);
    context.aiRuntime.restoreSession = async (snapshot) => { snapshot.title = "changed detached snapshot"; throw new Error("corrupt state"); };
    await assert.rejects(service.handleRequest("_msinsight/session/export", { sessionId: "stored" }), /corrupt state/);
    assert.deepEqual(session, before);
});

/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import { createNativeSessionService } from "../../native-agent/session/sessionService.mjs";

test("assistant preset is user-turn context once, not persistent system instructions", async () => {
    const session = { sessionId: "s", modelId: "default", primaryAgentId: "general", messages: [], title: "Existing session" };
    const turns = [];
    const context = {
        sessions: new Map([["s", session]]),
        running: new Map(),
        modelCatalog: { defaultModelId: "default", models: [{ id: "default" }] },
        agentRegistry: { getPrimary: () => ({ id: "general", body: "General", fingerprint: "v1", bashRules: [] }) },
        filesystem: { updateSessionFilesystemRoots: async () => {} },
        sessionStore: { save: async () => {} },
        aiRuntime: { runPrompt: async ({ userText, visibleUserText, session: current }) => {
            turns.push({ userText, visibleUserText, hostSystemPrompt: current.hostSystemPrompt });
            return { ok: true };
        } },
    };
    const service = createNativeSessionService(context);
    const base = { type: "resource", resource: { uri: "insight-system-prompt://project", text: "Base rules" } };
    const preset = { type: "resource", resource: { uri: "insight-assistant-preset://project", text: '<context ref="insight-assistant-preset://project"/>\nSlow-card instructions' } };
    await service.handleRequest("session/prompt", { sessionId: "s", prompt: [base, preset, { type: "text", text: "Analyze rank 3" }] });
    await service.handleRequest("session/prompt", { sessionId: "s", prompt: [base, { type: "text", text: "Compare rank 0" }] });
    assert.deepEqual(turns, [
        { userText: "Slow-card instructions\nAnalyze rank 3", visibleUserText: "Analyze rank 3", hostSystemPrompt: "Base rules" },
        { userText: "Compare rank 0", visibleUserText: "Compare rank 0", hostSystemPrompt: "Base rules" },
    ]);
});

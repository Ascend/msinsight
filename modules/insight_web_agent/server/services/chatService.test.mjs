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
import { createChatService, createPromptContent } from "./chatService.mjs";
import { createRuntimeState } from "../state/runtimeState.mjs";

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
    const content = createPromptContent("visible request", [], [], {
        projectName: "demo",
        files: ["src/index.ts"],
    });

    assert.deepEqual(content, [
        {
            type: "resource",
            resource: {
                uri: "insight-hidden-context://project",
                mimeType: "application/json",
                text: '{"contextPolicy":"replace_previous_hidden_context","instruction":"Use this hidden context as the authoritative project context for this turn and ignore any previous hidden project context in this session.","data":{"projectName":"demo","files":["src/index.ts"]}}',
            },
        },
        { type: "text", text: "visible request" },
    ]);
});

test("createPromptContent injects system prompt as the leading resource block", () => {
    const content = createPromptContent("hello", [], [], undefined, "你是一个 Ascend 调优助手");

    assert.deepEqual(content, [
        {
            type: "resource",
            resource: {
                uri: "insight-system-prompt://project",
                mimeType: "text/plain",
                text: "你是一个 Ascend 调优助手",
            },
        },
        { type: "text", text: "hello" },
    ]);
});

test("createPromptContent skips empty system prompt for backward compatibility", () => {
    const withoutSystem = createPromptContent("hello", []);
    const withEmptySystem = createPromptContent("hello", [], [], undefined, "   ");
    const withNullSystem = createPromptContent("hello", [], [], undefined, null);

    assert.deepEqual(withoutSystem, [{ type: "text", text: "hello" }]);
    assert.deepEqual(withEmptySystem, [{ type: "text", text: "hello" }]);
    assert.deepEqual(withNullSystem, [{ type: "text", text: "hello" }]);
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
        async createSessionContext() {
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
        skillService: { extractFromPrompt: async (text) => ({ text, skills: [] }) },
        state,
    });

    const result = await service.prompt("hello", { newSession: true, mode: "bypass" });

    assert.equal(result.sessionId, "session-1");
    assert.deepEqual(calls.map((call) => call.method), ["session/set_config_option", "session/prompt"]);
});

test("prompt always overwrites previous hidden context", async () => {
    const { service, calls } = createPromptTestService();

    await service.prompt("first", {
        sessionId: "session-1",
        hiddenContext: { projectRoot: "C:/a", branch: "main" },
    });
    await waitForPromptCall(calls, 1);

    await service.prompt("second", {
        sessionId: "session-1",
        hiddenContext: { projectRoot: "C:/b" },
    });
    await waitForPromptCall(calls, 2);

    const promptCalls = calls.filter((call) => call.method === "session/prompt");
    assert.equal(promptCalls[1].params.prompt[0].resource.text, '{"contextPolicy":"replace_previous_hidden_context","instruction":"Use this hidden context as the authoritative project context for this turn and ignore any previous hidden project context in this session.","data":{"projectRoot":"C:/b"}}');
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

const createPromptTestService = () => {
    const calls = [];
    const state = createRuntimeState();
    state.initialized = true;
    state.sessionContexts.set("session-1", {
        sessionId: "session-1",
        messages: [],
        pendingPrompt: false,
        configOptions: [],
    });

    const acpClient = {
        async request(method, params) {
            calls.push({ method, params });
            if (method === "session/prompt") return { stopReason: "end_turn" };
            throw new Error(`unexpected ACP method: ${method}`);
        },
    };

    const service = createChatService({
        acpClient,
        eventBus: { broadcast: () => {} },
        sessionService: {
            applyPreferredModel: async () => {},
            refreshSessions: async () => {},
        },
        skillService: { extractFromPrompt: async (text) => ({ text, skills: [] }) },
        state,
    });

    return { service, calls };
};

const waitForPromptCall = async (calls, count) => {
    for (let index = 0; index < 10; index += 1) {
        if (calls.filter((call) => call.method === "session/prompt").length >= count) return;
        await new Promise((resolve) => setTimeout(resolve, 0));
    }
};

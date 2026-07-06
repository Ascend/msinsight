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
import { appendContentBlock } from "../../services/messageService.mjs";

const buildSessionContext = () => ({
    sessionId: "session-1",
    messages: [],
    pendingPrompt: false,
    configOptions: [],
});

test("appendContentBlock ignores hidden context resources", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "user", {
        type: "resource",
        resource: {
            uri: "insight-hidden-context://project",
            mimeType: "application/json",
            text: '{"projectName":"demo"}',
        },
    });

    assert.deepEqual(state.sessionContexts.get("session-1").messages, []);
    assert.deepEqual(events, []);
});

test("appendContentBlock ignores any insight- prefixed resource block", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "resource",
        resource: {
            uri: "insight-system://project",
            mimeType: "application/json",
            text: '{"role":"system"}',
        },
    });

    assert.deepEqual(state.sessionContexts.get("session-1").messages, []);
    assert.deepEqual(events, []);
});

test("appendContentBlock does not drop non-insight resource blocks", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "user", {
        type: "resource",
        resource: {
            uri: "file:///workspace/notes.md",
            mimeType: "text/markdown",
            text: "# Notes",
        },
    });

    const messages = state.sessionContexts.get("session-1").messages;
    assert.equal(messages.length, 1);
    assert.equal(messages[0].text, "# Notes");
    assert.ok(events.some((event) => event.type === "message_added"));
    assert.ok(events.some((event) => event.type === "message_delta" && event.field === "text"));
});

test("appendContentBlock keeps regular text blocks visible (regression)", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "text",
        text: "Hello, how can I help?",
    });

    const messages = state.sessionContexts.get("session-1").messages;
    assert.equal(messages.length, 1);
    assert.equal(messages[0].role, "assistant");
    assert.equal(messages[0].text, "Hello, how can I help?");
    assert.ok(events.some((event) => event.type === "message_added"));
});

test("appendContentBlock ignores insight- resources during history replay", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", { ...buildSessionContext(), replayingHistory: true });
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "resource",
        resource: {
            uri: "insight-system://replay",
            mimeType: "application/json",
            text: '{"echo":"system"}',
        },
    });

    assert.deepEqual(state.sessionContexts.get("session-1").messages, []);
    assert.deepEqual(events, []);
});

test("appendContentBlock ignores assistant text blocks whose head echoes insight-hidden-context://", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "text",
        text: "[insight-hidden-context://project]\n{\"contextPolicy\":\"replace_previous_hidden_context\",\"data\":{}}\n",
    });

    assert.deepEqual(state.sessionContexts.get("session-1").messages, []);
    assert.deepEqual(events, []);
});

test("appendContentBlock ignores assistant text blocks whose head echoes insight-system-prompt://", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "text",
        text: "[insight-system-prompt://project]\n你是 MindStudio Insight 的项目上下文助手...\n",
    });

    assert.deepEqual(state.sessionContexts.get("session-1").messages, []);
    assert.deepEqual(events, []);
});

test("appendContentBlock keeps assistant text blocks that only mention insight- URIs later in the message", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", buildSessionContext());
    const events = [];

    appendContentBlock({ eventBus: { broadcast: (event) => events.push(event) }, state }, "session-1", "assistant", {
        type: "text",
        text: "Sure, here is what I found.\n\nReference: insight-hidden-context://project is a system URI; you should not echo it back to the user.",
    });

    const messages = state.sessionContexts.get("session-1").messages;
    assert.equal(messages.length, 1);
    assert.equal(messages[0].role, "assistant");
    assert.match(messages[0].text, /Sure, here is what I found\./);
    assert.match(messages[0].text, /insight-hidden-context:\/\/project/);
    assert.ok(events.some((event) => event.type === "message_added"));
});

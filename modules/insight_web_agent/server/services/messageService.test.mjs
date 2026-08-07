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
import { createRuntimeState } from "../state/runtimeState.mjs";
import { appendContentBlock } from "./messageService.mjs";

test("appendContentBlock ignores hidden context resources", () => {
    const state = createRuntimeState();
    state.sessionContexts.set("session-1", {
        sessionId: "session-1",
        messages: [],
        pendingPrompt: false,
        configOptions: [],
    });
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

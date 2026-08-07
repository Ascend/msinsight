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
import { createSessionService } from "./sessionService.mjs";

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

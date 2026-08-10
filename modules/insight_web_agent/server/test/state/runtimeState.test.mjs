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
import { createRuntimeState, restoreRuntimeState, snapshotRuntimeState } from "../../state/runtimeState.mjs";

test("restoreRuntimeState cancels pending permission requests overwritten by rollback", async () => {
    const state = createRuntimeState();
    const snapshot = snapshotRuntimeState(state);
    let resolved;
    const request = {
        sessionId: "candidate-session",
        requestId: "candidate-request",
        state: "pending",
        timeout: setTimeout(() => {
            state.agentError = "candidate timer fired after rollback";
        }, 10),
        resolve(value) {
            resolved = value;
        },
    };
    state.pendingPermissions.set("candidate-session:candidate-request", request);

    restoreRuntimeState(state, snapshot);
    await new Promise((resolve) => setTimeout(resolve, 25));

    assert.equal(state.pendingPermissions.size, 0);
    assert.equal(request.state, "invalidated");
    assert.deepEqual(resolved, { allowed: false, state: "invalidated", reason: "invalidated" });
    assert.equal(state.agentError, undefined);
});

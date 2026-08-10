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
import { createContextAssembler } from "../../services/contextAssembler.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const createSessionContext = () => ({
    sessionId: "session-1",
    agentId: "agent-1",
    runtime: "stdio",
    mode: "free_chat",
    view: "Timeline",
});

test("assemble preserves the outer context packet when active context is empty", async () => {
    const state = createRuntimeState();
    const assembler = createContextAssembler({ state });

    const packet = await assembler.assemble(createSessionContext());

    assert.equal(packet.schemaVersion, "1.0");
    assert.deepEqual(packet.contextProviders, [{ name: "structured", schemaVersion: "1.0", contentRefs: undefined }]);
});

test("assemble returns the raw active context payload as structured content refs", async () => {
    const state = createRuntimeState();
    state.activeContext = { profileId: "profile-1", activeModule: "Timeline", custom: { value: 1 } };
    const assembler = createContextAssembler({ state });

    const hiddenContext = await assembler.assemble(createSessionContext());

    assert.deepEqual(hiddenContext, {
        schemaVersion: "1.0",
        session: {
            id: "session-1",
            agentId: "agent-1",
            agentRuntime: "stdio",
            mode: "free_chat",
        },
        contextProviders: [{ name: "structured", schemaVersion: "1.0", contentRefs: state.activeContext }],
        hands: {
            skills: [],
            tools: ["msinsight_observe", "msinsight_listActions"],
            actions: [],
            permissions: { msinsight_invokeAction: "approval_required" },
        },
    });
});

test("assemble reflects updated raw active context", async () => {
    const state = createRuntimeState();
    const assembler = createContextAssembler({ state });
    state.activeContext = { profileId: "old", activeModule: "Timeline" };
    await assembler.assemble(createSessionContext());

    state.activeContext.profileId = "xxx";
    const hiddenContext = await assembler.assemble(createSessionContext());

    assert.equal(hiddenContext.contextProviders[0].contentRefs.profileId, "xxx");
});

test("assemble includes the active project root when available", async () => {
    const state = createRuntimeState();
    state.activeContext = { projectRoot: "D:/workspace/project" };
    const assembler = createContextAssembler({ state });

    const packet = await assembler.assemble(createSessionContext());

    assert.equal(packet.projectRoot, "D:/workspace/project");
});

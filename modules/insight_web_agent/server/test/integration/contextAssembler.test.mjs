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

test("assemble returns a structured context provider", async () => {
    const state = createRuntimeState();
    const assembler = createContextAssembler({ state });

    const packet = await assembler.assemble(createSessionContext());

    assert.equal(packet.contextProviders[0].name, "structured");
});

test("assemble reflects updated active context profile refs", async () => {
    const state = createRuntimeState();
    const assembler = createContextAssembler({ state });
    state.activeContext = { profileId: "old", activeModule: "Timeline" };
    await assembler.assemble(createSessionContext());

    state.activeContext.profileId = "xxx";
    const packet = await assembler.assemble(createSessionContext());

    assert.ok(packet.contextProviders[0].contentRefs.includes("insight://profile/xxx"));
});

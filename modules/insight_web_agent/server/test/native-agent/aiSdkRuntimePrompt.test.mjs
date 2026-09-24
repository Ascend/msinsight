/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { createNativeSystemPrompt } from "../../native-agent/runtime/aiSdkRuntime.mjs";
import { resolveShellRuntime } from "../../native-agent/tools/shellRuntime.mjs";

const session = {
    hostSystemPrompt: "Host guidance",
    primaryAgentBody: "Agent guidance",
};

test("native system prompt instructs Windows agents to use PowerShell syntax", () => {
    const prompt = createNativeSystemPrompt(session, resolveShellRuntime({ platform: "win32", env: {} }));

    assert.match(prompt, /compatibility tool name "Bash"/);
    assert.match(prompt, /Windows PowerShell/);
    assert.match(prompt, /PowerShell syntax/);
    assert.match(prompt, /Windows path conventions/);
});

test("native system prompt instructs Unix-like agents to use Bash syntax", () => {
    const prompt = createNativeSystemPrompt(session, resolveShellRuntime({ platform: "linux", env: {} }));

    assert.doesNotMatch(prompt, /compatibility tool name/);
    assert.match(prompt, /Bash syntax/);
});

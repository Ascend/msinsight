/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { strict as assert } from "node:assert";
import test from "node:test";
import { agentLaunchKey, agentWorkspaceKey, withAgentIdentity } from "../../services/agentIdentityService.mjs";

test("launch identity ignores display name and environment", () => {
    const left = { name: "OpenCode", command: "opencode", args: ["acp"], env: { TOKEN: "one" } };
    const right = { name: "OpenCode Alias", command: "opencode", args: ["acp"], env: { TOKEN: "two" } };

    assert.equal(agentLaunchKey(left), agentLaunchKey(right));
    assert.equal(agentWorkspaceKey(left), agentWorkspaceKey(right));
});

test("generic argument variants use distinct workspaces", () => {
    assert.notEqual(
        agentWorkspaceKey({ command: "agent", args: ["serve"] }),
        agentWorkspaceKey({ command: "agent", args: ["acp"] }),
    );
});

test("the built-in agent always uses its fixed workspace", () => {
    const builtin = withAgentIdentity({ name: "msinsight-native", command: "node", args: ["entry.mjs"], env: {} }, "builtin");

    assert.equal(builtin.workspaceKey, "msinsight-native");
});

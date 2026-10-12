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
import { mkdtemp, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { bootstrapAgentServersConfig } from "../../config/bootstrap.mjs";

const writeJson = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");

test("merges missing packaged agents by stable name without changing user config or active selection", async () => {
    const rootDir = await mkdtemp(join(tmpdir(), "agent-user-"));
    const resourceDir = await mkdtemp(join(tmpdir(), "agent-package-"));
    await writeJson(join(resourceDir, "agent-servers.json"), {
        activeAgent: "Native",
        agentServers: [
            { name: "OpenCode", command: "packaged-opencode", args: [] },
            { name: "Native", command: "node", args: ["native.mjs"], env: {} },
        ],
    });
    await writeJson(join(rootDir, "agent-servers.json"), {
        activeAgent: "Custom",
        customSetting: true,
        agentServers: [
            { name: "OpenCode", command: "user-opencode", args: ["acp", "--custom"] },
            { name: "Custom", command: "custom", args: [] },
        ],
    });

    bootstrapAgentServersConfig(rootDir, resourceDir);
    bootstrapAgentServersConfig(rootDir, resourceDir);
    const result = JSON.parse(await readFile(join(rootDir, "agent-servers.json"), "utf8"));

    assert.equal(result.activeAgent, "Custom");
    assert.equal(result.customSetting, true);
    assert.deepEqual(result.agentServers.map(({ name }) => name), ["OpenCode", "Custom", "Native"]);
    assert.equal(result.agentServers[0].command, "user-opencode");
});

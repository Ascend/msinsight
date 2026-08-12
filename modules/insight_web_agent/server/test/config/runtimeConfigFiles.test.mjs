/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { strict as assert } from "node:assert";
import { execFile } from "node:child_process";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);
const moduleRoot = fileURLToPath(new URL("../../..", import.meta.url));
const configModuleUrl = new URL("../../config/index.mjs", import.meta.url).href;

const loadConfigInChild = (rootDir) => execFileAsync(process.execPath, [
    "--input-type=module",
    "--eval",
    "const { config } = await import(process.env.TEST_CONFIG_MODULE_URL); console.log(config.activeAgentName);",
], {
    cwd: dirname(fileURLToPath(import.meta.url)),
    env: {
        ...process.env,
        ACP_ROOT: rootDir,
        ACP_RESOURCE_ROOT: moduleRoot,
        TEST_CONFIG_MODULE_URL: configModuleUrl,
    },
});

test("Node startup creates missing ACP configuration files", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-runtime-config-"));
    t.after(() => rm(rootDir, { recursive: true, force: true }));

    await loadConfigInChild(rootDir);

    const agentConfig = JSON.parse(await readFile(join(rootDir, "agent-servers.json"), "utf8"));
    const nativeConfig = JSON.parse(await readFile(join(rootDir, "msinsight-native.json"), "utf8"));
    assert.deepEqual(agentConfig, { agentServers: [] });
    assert.equal(nativeConfig.name, undefined);
    assert.equal(nativeConfig.model, "cx/gpt-5.5");
});

test("Node startup never overwrites existing ACP configuration", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-runtime-config-existing-"));
    t.after(() => rm(rootDir, { recursive: true, force: true }));
    const existing = { activeAgent: "Custom", agentServers: [{ name: "Custom", command: "custom-acp", args: [], env: {} }] };
    await writeFile(join(rootDir, "agent-servers.json"), `${JSON.stringify(existing)}\n`, "utf8");

    await loadConfigInChild(rootDir);

    assert.deepEqual(JSON.parse(await readFile(join(rootDir, "agent-servers.json"), "utf8")), existing);
});

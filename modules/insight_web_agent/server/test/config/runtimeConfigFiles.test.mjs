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
import { chmod, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);
const moduleRoot = fileURLToPath(new URL("../../..", import.meta.url));
const configModuleUrl = new URL("../../config/index.mjs", import.meta.url).href;

const loadConfigInChild = (rootDir, env = {}, resourceDir = moduleRoot) => execFileAsync(process.execPath, [
    "--input-type=module",
    "--eval",
    "const { config } = await import(process.env.TEST_CONFIG_MODULE_URL); process.stdout.write(JSON.stringify({ activeAgentName: config.activeAgentName, configuredCapabilities: config.configuredCapabilities }));",
], {
    cwd: dirname(fileURLToPath(import.meta.url)),
    env: {
        ...process.env,
        ACP_ROOT: rootDir,
        ACP_RESOURCE_ROOT: resourceDir,
        ACP_CAPABILITY_TOKEN: "test-capability-token",
        TEST_CONFIG_MODULE_URL: configModuleUrl,
        ...env,
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
    const bundledNativeConfig = JSON.parse(await readFile(join(moduleRoot, "msinsight-native.json"), "utf8"));
    assert.equal(nativeConfig.model, bundledNativeConfig.model);
});

test("Node startup resolves configured CLI capabilities from PATH", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-runtime-config-cli-"));
    const resourceDir = await mkdtemp(join(tmpdir(), "insight-runtime-resource-"));
    const binDir = await mkdtemp(join(tmpdir(), "insight-runtime-bin-"));
    const command = process.platform === "win32" ? "custom-cli.exe" : "custom-cli";
    const executable = join(binDir, command);
    t.after(() => Promise.all([
        rm(rootDir, { recursive: true, force: true }),
        rm(resourceDir, { recursive: true, force: true }),
        rm(binDir, { recursive: true, force: true }),
    ]));
    await writeFile(executable, "fake", "utf8");
    if (process.platform !== "win32") await chmod(executable, 0o700);
    await writeFile(join(resourceDir, "capability-center.json"), `${JSON.stringify({
        schemaVersion: 1,
        capabilities: [{ type: "cli", name: "custom_cli", executable: command }],
    })}\n`, "utf8");

    const { stdout } = await loadConfigInChild(rootDir, { PATH: binDir }, resourceDir);
    const [{ name, executable: resolved }] = JSON.parse(stdout.trim()).configuredCapabilities;
    assert.equal(name, "custom_cli");
    assert.equal(resolved, executable);
});

test("Node startup never overwrites existing ACP configuration", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-runtime-config-existing-"));
    t.after(() => rm(rootDir, { recursive: true, force: true }));
    const existing = { activeAgent: "Custom", agentServers: [{ name: "Custom", command: "custom-acp", args: [], env: {} }] };
    await writeFile(join(rootDir, "agent-servers.json"), `${JSON.stringify(existing)}\n`, "utf8");

    await loadConfigInChild(rootDir);

    assert.deepEqual(JSON.parse(await readFile(join(rootDir, "agent-servers.json"), "utf8")), existing);
});

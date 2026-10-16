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
import { chmod, mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
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
    "const { config } = await import(process.env.TEST_CONFIG_MODULE_URL); process.stdout.write(JSON.stringify({ activeAgentName: config.activeAgentName, configuredCapabilities: config.configuredCapabilities, memoryTuningPrompt: config.memoryTuningPrompt }));",
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

test("loads the memory assistant prompt from the runtime resource directory", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-user-"));
    const resourceDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-resource-"));
    t.after(() => Promise.all([rootDir, resourceDir].map((path) => rm(path, { recursive: true, force: true }))));
    await mkdir(join(resourceDir, "prompts"));
    await writeFile(join(resourceDir, "prompts", "memory-tuning-assistant.md"), "\ufeffMemory assistant instructions\r\n", "utf8");
    const { stdout } = await loadConfigInChild(rootDir, {}, resourceDir);
    assert.equal(JSON.parse(stdout.trim()).memoryTuningPrompt, "Memory assistant instructions");
});

test("strips leading YAML frontmatter from the memory assistant prompt", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-user-"));
    const resourceDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-resource-"));
    t.after(() => Promise.all([rootDir, resourceDir].map((path) => rm(path, { recursive: true, force: true }))));
    await mkdir(join(resourceDir, "prompts"));
    await writeFile(
        join(resourceDir, "prompts", "memory-tuning-assistant.md"),
        "\ufeff---\r\ndescription: test\r\npermission:\r\n  bash:\r\n    \"*\": ask\r\n---\r\n\r\nMemory body\r\n\r\n---\r\n\r\nTail\r\n",
        "utf8",
    );
    const { stdout } = await loadConfigInChild(rootDir, {}, resourceDir);
    assert.equal(JSON.parse(stdout.trim()).memoryTuningPrompt, "Memory body\r\n\r\n---\r\n\r\nTail");
});

test("bundled memory assistant prompt uses pt-snap 0.4.1 command syntax", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-bundled-"));
    t.after(() => rm(rootDir, { recursive: true, force: true }));
    const { stdout } = await loadConfigInChild(rootDir);
    const prompt = JSON.parse(stdout.trim()).memoryTuningPrompt;
    assert.ok(prompt.length > 0);
    assert.equal(prompt.startsWith("---"), false);
    assert.equal(prompt.includes("permission:"), false);
    assert.equal(/--db\b/.test(prompt.replace("不存在 `--db` 选项", "")), false);
    assert.ok(prompt.includes("pt_snap"));
    assert.ok(prompt.includes("pt-snap 必须从 MindStudio Insight 能力中心指定的内置环境加载"));
    assert.equal(prompt.includes("{{PT_SNAP_COMMAND}}"), false);
    assert.equal(prompt.includes("必须且只能"), false);
    assert.equal(prompt.includes("pt-snap-fragmentation-forensics"), false);
});

test("injects the capability-center pt-snap command into the memory assistant prompt", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-memory-prompt-user-"));
    const resourceDir = await mkdtemp(join(tmpdir(), "insight memory prompt resource "));
    t.after(() => Promise.all([rootDir, resourceDir].map((path) => rm(path, { recursive: true, force: true }))));
    const pythonName = process.platform === "win32" ? "python.exe" : "python3";
    const python = join(resourceDir, "python dir", pythonName);
    await mkdir(join(resourceDir, "prompts"));
    await mkdir(dirname(python));
    await writeFile(python, "fake", "utf8");
    if (process.platform !== "win32") await chmod(python, 0o700);
    await writeFile(join(resourceDir, "capability-center.json"), `${JSON.stringify({
        schemaVersion: 1,
        capabilities: [{
            type: "cli",
            name: "pt_snap",
            command: { default: [{ executable: `./python dir/${pythonName}`, argsPrefix: ["-m", "pt_snap_cli.cli"] }] },
        }],
    })}\n`, "utf8");
    await writeFile(join(resourceDir, "prompts", "memory-tuning-assistant.md"), "Env:\n\n{{PT_SNAP_COMMAND}}\n", "utf8");

    const { stdout } = await loadConfigInChild(rootDir, {}, resourceDir);
    const prompt = JSON.parse(stdout.trim()).memoryTuningPrompt;
    const quote = process.platform === "win32" ? "\"" : "'";
    assert.ok(prompt.includes(`${quote}${python}${quote} -m pt_snap_cli.cli`));
    assert.equal(prompt.includes("{{PT_SNAP_COMMAND}}"), false);
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

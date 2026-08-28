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
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { strict as assert } from "node:assert";
import test from "node:test";
import { createRuntimeState, getSessionContext } from "../../state/runtimeState.mjs";
import { createAgentConfigService } from "../../services/agentConfigService.mjs";

const createFixture = async () => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-agent-config-"));
    await writeJson(join(rootDir, "agent-servers.json"), {
        activeAgent: "OpenCode",
        agentServers: [
            { name: "OpenCode", command: "opencode", args: ["acp"], env: { ACP_DEBUG: "1" } },
            { name: "Claude", command: "claude", args: [], env: {} },
        ],
    });
    await writeJson(join(rootDir, "acp-session-conf.json"), {
        requestTimeoutMs: 1000,
        promptRequestTimeoutMs: 2000,
        permissionRequestTimeoutMs: 3000,
        defaultAllowlist: {
            includeDocsRoot: true,
            includeAgentWorkspaceRoot: false,
            includeProjectRoot: true,
            extraPaths: ["missing/path"],
        },
    });
    await writeJson(join(rootDir, "msinsight-native.json"), {
        schemaVersion: 1,
        provider: "openai",
        model: "cx/gpt-5.5",
        baseUrl: "http://127.0.0.1:19099/v1",
        apiKey: "",
    });
    const state = createRuntimeState();
    state.activeAgentName = "OpenCode";
    const reloads = [];
    const service = createAgentConfigService({
        rootDir,
        state,
        reloadRuntime: async (snapshot) => {
            reloads.push(snapshot);
            return { snapshot };
        },
    });
    return { rootDir, state, reloads, service };
};

const writeJson = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
const readJson = async (path) => JSON.parse(await readFile(path, "utf8"));
const readText = (path) => readFile(path, "utf8");

const validSnapshot = () => ({
    activeAgentName: "OpenCode",
    agentServers: [
        { name: "OpenCode", command: "opencode", args: ["acp", "--debug"], env: { ACP_DEBUG: "" } },
        { name: "Claude", command: "claude", args: [], env: {} },
        { name: "NewAgent", command: "new-agent", args: ["serve"], env: { TOKEN: "abc" } },
    ],
    builtinAgent: {
        schemaVersion: 1,
        name: "msinsight-native",
        provider: "openai",
        model: "cx/gpt-5.5",
        baseUrl: "http://127.0.0.1:19099/v1",
        apiKey: "secret",
    },
    sessionConfig: {
        requestTimeoutMs: 5000,
        promptRequestTimeoutMs: 6000,
        permissionRequestTimeoutMs: 7000,
        defaultAllowlist: {
            includeDocsRoot: false,
            includeAgentWorkspaceRoot: true,
            includeProjectRoot: false,
            extraPaths: ["does/not/exist", ""],
        },
    },
});

const agentConfigFrom = (snapshot) => ({
    activeAgentName: snapshot.activeAgentName,
    agentServers: snapshot.agentServers,
});

test("reads a normalized agent and session config snapshot", async () => {
    const fixture = await createFixture();

    const snapshot = await fixture.service.readSnapshot();

    assert.deepEqual(snapshot, {
        activeAgentName: "OpenCode",
        agentServers: [
            { name: "OpenCode", command: "opencode", args: ["acp"], env: { ACP_DEBUG: "1" } },
            { name: "Claude", command: "claude", args: [], env: {} },
        ],
        builtinAgent: {
            schemaVersion: 1,
            name: "msinsight-native",
            provider: "openai",
            model: "cx/gpt-5.5",
            baseUrl: "http://127.0.0.1:19099/v1",
            apiKey: "",
        },
        sessionConfig: {
            requestTimeoutMs: 1000,
            promptRequestTimeoutMs: 2000,
            permissionRequestTimeoutMs: 3000,
            defaultAllowlist: {
                includeDocsRoot: true,
                includeAgentWorkspaceRoot: false,
                includeProjectRoot: true,
                extraPaths: ["missing/path"],
            },
        },
    });
});

test("normalizes an omitted docs allowlist flag to false", async () => {
    const fixture = await createFixture();
    await writeJson(join(fixture.rootDir, "acp-session-conf.json"), {
        requestTimeoutMs: 1000,
        promptRequestTimeoutMs: 2000,
        permissionRequestTimeoutMs: 3000,
        defaultAllowlist: {
            includeAgentWorkspaceRoot: true,
            includeProjectRoot: true,
            extraPaths: [],
        },
    });

    const snapshot = await fixture.service.readSnapshot();

    assert.equal(snapshot.sessionConfig.defaultAllowlist.includeDocsRoot, false);
});

test("saves each configuration section independently", async () => {
    const fixture = await createFixture();
    const snapshot = validSnapshot();

    const builtinResult = await fixture.service.saveBuiltinAgent(snapshot.builtinAgent);
    const sessionResult = await fixture.service.saveSessionConfig(snapshot.sessionConfig);
    const agentResult = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(builtinResult.ok, true);
    assert.equal(sessionResult.ok, true);
    assert.equal(agentResult.ok, true);
    assert.equal(fixture.reloads.length, 3);
    assert.deepEqual(await readJson(join(fixture.rootDir, "agent-servers.json")), {
        activeAgent: "OpenCode",
        agentServers: snapshot.agentServers,
    });
    assert.deepEqual(await readJson(join(fixture.rootDir, "acp-session-conf.json")), {
        requestTimeoutMs: 5000,
        promptRequestTimeoutMs: 6000,
        permissionRequestTimeoutMs: 7000,
        defaultAllowlist: {
            includeDocsRoot: false,
            includeAgentWorkspaceRoot: true,
            includeProjectRoot: false,
            extraPaths: ["does/not/exist"],
        },
    });
    assert.deepEqual(await readJson(join(fixture.rootDir, "msinsight-native.json")), {
        schemaVersion: 1,
        provider: "openai",
        model: "cx/gpt-5.5",
        baseUrl: "http://127.0.0.1:19099/v1",
        apiKey: "secret",
    });
});

test("rejects duplicate ACP launch commands with a concrete conflict", async () => {
    const fixture = await createFixture();
    const snapshot = validSnapshot();
    snapshot.agentServers.push({ name: "OpenCode Copy", command: "opencode", args: ["acp", "--debug"], env: { OTHER: "value" } });

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.error, "duplicate_agent_launch");
    assert.equal(result.status, 409);
    assert.equal(result.conflictingAgent, "OpenCode");
    assert.equal(result.duplicateAgent, "OpenCode Copy");
});

test("supports the built-in agent as active with no generic agents", async () => {
    const fixture = await createFixture();
    await writeJson(join(fixture.rootDir, "agent-servers.json"), { activeAgent: "msinsight-native", agentServers: [] });
    const snapshot = validSnapshot();
    snapshot.activeAgentName = "msinsight-native";
    snapshot.agentServers = [];

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.ok, true);
    assert.equal(result.snapshot.activeAgentName, "msinsight-native");
    assert.deepEqual(result.snapshot.agentServers, []);
});

test("removes matching transient agents before runtime reload", async () => {
    const fixture = await createFixture();
    const calls = [];
    const service = createAgentConfigService({
        rootDir: fixture.rootDir,
        state: fixture.state,
        beforeReload: async (snapshot) => {
            calls.push(`before:${snapshot.agentServers.at(-1).name}`);
        },
        reloadRuntime: async () => {
            calls.push("reload");
        },
    });

    const result = await service.saveAgentServers(agentConfigFrom(validSnapshot()));

    assert.equal(result.ok, true);
    assert.deepEqual(calls, ["before:NewAgent", "reload"]);
});

test("adds a new agent and switches active agent only when requested by the snapshot", async () => {
    const fixture = await createFixture();
    const withoutSwitch = validSnapshot();

    await fixture.service.saveAgentServers(agentConfigFrom(withoutSwitch));
    assert.equal((await readJson(join(fixture.rootDir, "agent-servers.json"))).activeAgent, "OpenCode");

    const withSwitch = { ...withoutSwitch, activeAgentName: "NewAgent" };
    await fixture.service.saveAgentServers(agentConfigFrom(withSwitch));

    assert.equal((await readJson(join(fixture.rootDir, "agent-servers.json"))).activeAgent, "NewAgent");
});

test("allows adding a generic agent when the unchanged built-in agent is not configured", async () => {
    const fixture = await createFixture();
    await writeJson(join(fixture.rootDir, "msinsight-native.json"), {
        schemaVersion: 1,
        provider: "openai",
        model: "cx/gpt-5.5",
        baseUrl: "",
        apiKey: "",
    });
    const snapshot = await fixture.service.readSnapshot();
    snapshot.agentServers.push({ name: "NewAgent", command: "new-agent", args: ["serve"], env: {} });

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.ok, true);
    assert.equal(result.snapshot.agentServers.at(-1).name, "NewAgent");
});

test("rejects invalid config snapshots without writing or reloading", async () => {
    const fixture = await createFixture();
    const beforeAgentConfig = await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8");
    const beforeSessionConfig = await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8");

    const cases = [
        { name: "duplicate names", patch: { agentServers: [{ name: "A", command: "a", args: [], env: {} }, { name: "A", command: "b", args: [], env: {} }], activeAgentName: "A" } },
        { name: "built-in reserved name", patch: { agentServers: [{ name: "msinsight-native", command: "other", args: [], env: {} }], activeAgentName: "msinsight-native" } },
        { name: "auto reserved suffix", patch: { agentServers: [{ name: "Other(auto)", command: "other", args: [], env: {} }], activeAgentName: "Other(auto)" } },
        { name: "empty command", patch: { agentServers: [{ name: "A", command: "", args: [], env: {} }], activeAgentName: "A" } },
        { name: "empty arg", patch: { agentServers: [{ name: "A", command: "a", args: [""], env: {} }], activeAgentName: "A" } },
        { name: "empty env key", patch: { agentServers: [{ name: "A", command: "a", args: [], env: { "": "value" } }], activeAgentName: "A" } },
        { name: "non-positive timeout", patch: { sessionConfig: { ...validSnapshot().sessionConfig, requestTimeoutMs: 0 } } },
        { name: "missing active agent", patch: { activeAgentName: "Missing" } },
    ];

    for (const item of cases) {
        const snapshot = { ...validSnapshot(), ...item.patch };
        const result = item.name === "non-positive timeout"
            ? await fixture.service.saveSessionConfig(snapshot.sessionConfig)
            : await fixture.service.saveAgentServers(agentConfigFrom(snapshot));
        assert.equal(result.error, "validation_failed", item.name);
    }
    assert.equal(fixture.reloads.length, 0);
    assert.equal(await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8"), beforeAgentConfig);
    assert.equal(await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8"), beforeSessionConfig);
});

test("rejects trim-empty args without writing or reloading", async () => {
    const fixture = await createFixture();
    const beforeAgentConfig = await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8");
    const beforeSessionConfig = await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8");
    const snapshot = validSnapshot();
    snapshot.agentServers[0].args = ["   "];

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.error, "validation_failed");
    assert.equal(result.message, "agent args cannot be empty");
    assert.equal(fixture.reloads.length, 0);
    assert.equal(await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8"), beforeAgentConfig);
    assert.equal(await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8"), beforeSessionConfig);
});

test("rejects trim-empty env keys without writing or reloading", async () => {
    const fixture = await createFixture();
    const beforeAgentConfig = await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8");
    const beforeSessionConfig = await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8");
    const snapshot = validSnapshot();
    snapshot.agentServers[0].env = { "   ": "value" };

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.error, "validation_failed");
    assert.equal(result.message, "env keys cannot be empty");
    assert.equal(fixture.reloads.length, 0);
    assert.equal(await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8"), beforeAgentConfig);
    assert.equal(await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8"), beforeSessionConfig);
});

test("rejects env keys that collide after trimming without writing or reloading", async () => {
    const fixture = await createFixture();
    const beforeAgentConfig = await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8");
    const beforeSessionConfig = await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8");
    const snapshot = validSnapshot();
    snapshot.agentServers[0].env = { TOKEN: "one", " TOKEN ": "two" };

    const result = await fixture.service.saveAgentServers(agentConfigFrom(snapshot));

    assert.equal(result.error, "validation_failed");
    assert.equal(result.message, "env keys must be unique");
    assert.equal(fixture.reloads.length, 0);
    assert.equal(await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8"), beforeAgentConfig);
    assert.equal(await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8"), beforeSessionConfig);
});

test("blocks saves while a prompt or permission is pending without writing or reloading", async () => {
    for (const busy of ["prompt", "permission"]) {
        const fixture = await createFixture();
        const beforeAgentConfig = await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8");
        const beforeSessionConfig = await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8");
        if (busy === "prompt") {
            getSessionContext(fixture.state, "session-1").pendingPrompt = true;
        } else {
            fixture.state.pendingPermissions.set("session-1:req-1", { sessionId: "session-1", requestId: "req-1", state: "pending" });
        }

        const result = await fixture.service.saveAgentServers(agentConfigFrom(validSnapshot()));

        assert.equal(result.error, "agent_busy");
        assert.equal(result.status, 409);
        assert.equal(fixture.reloads.length, 0);
        assert.equal(await readFile(join(fixture.rootDir, "agent-servers.json"), "utf8"), beforeAgentConfig);
        assert.equal(await readFile(join(fixture.rootDir, "acp-session-conf.json"), "utf8"), beforeSessionConfig);
    }
});

test("returns reload_failed after a successful file save without rolling config back", async () => {
    const fixture = await createFixture();
    fixture.service = createAgentConfigService({
        rootDir: fixture.rootDir,
        state: fixture.state,
        reloadRuntime: async () => {
            throw new Error("adapter failed");
        },
    });

    const result = await fixture.service.saveAgentServers(agentConfigFrom(validSnapshot()));

    assert.equal(result.error, "reload_failed");
    assert.match(result.message, /adapter failed/);
    assert.equal((await readJson(join(fixture.rootDir, "agent-servers.json"))).activeAgent, "OpenCode");
});

test("returns config_write_failed without reload or single-file replacement when a temp write fails", async () => {
    const fixture = await createFixture();
    const beforeAgentConfig = await readText(join(fixture.rootDir, "agent-servers.json"));
    const beforeSessionConfig = await readText(join(fixture.rootDir, "acp-session-conf.json"));
    await mkdir(join(fixture.rootDir, `acp-session-conf.json.${process.pid}.session-temp.tmp`));
    const service = createAgentConfigService({
        rootDir: fixture.rootDir,
        state: fixture.state,
        tempId: () => "session-temp",
        reloadRuntime: async (snapshot) => {
            fixture.reloads.push(snapshot);
            return { snapshot };
        },
    });

    const result = await service.saveSessionConfig(validSnapshot().sessionConfig);

    assert.equal(result.error, "config_write_failed");
    assert.match(result.message, /EISDIR|EPERM|EACCES/);
    await rm(join(fixture.rootDir, `acp-session-conf.json.${process.pid}.session-temp.tmp`), { recursive: true, force: true });
    assert.equal(fixture.reloads.length, 0);
    assert.equal(await readText(join(fixture.rootDir, "agent-servers.json")), beforeAgentConfig);
    assert.equal(await readText(join(fixture.rootDir, "acp-session-conf.json")), beforeSessionConfig);
});

test("returns config_write_failed when output files cannot be written and leaves existing files intact", async () => {
    if (process.platform === "win32") return;
    const fixture = await createFixture();
    const beforeAgentConfig = await readText(join(fixture.rootDir, "agent-servers.json"));
    const beforeSessionConfig = await readText(join(fixture.rootDir, "acp-session-conf.json"));
    const blockedRoot = join(fixture.rootDir, "blocked");
    await mkdir(blockedRoot, { recursive: true });
    await writeFile(join(blockedRoot, "agent-servers.json"), beforeAgentConfig, "utf8");
    await writeFile(join(blockedRoot, "acp-session-conf.json"), beforeSessionConfig, "utf8");
    const { chmod } = await import("node:fs/promises");
    await chmod(blockedRoot, 0o555);
    const blockedService = createAgentConfigService({
        rootDir: blockedRoot,
        state: fixture.state,
        reloadRuntime: async (snapshot) => {
            blockedService.reloads.push(snapshot);
            return { snapshot };
        },
    });
    blockedService.reloads = [];

    let result;
    try {
        result = await blockedService.saveSessionConfig(validSnapshot().sessionConfig);
    } finally {
        await chmod(blockedRoot, 0o755);
    }

    assert.equal(result.error, "config_write_failed");
    assert.equal(blockedService.reloads.length, 0);
    assert.equal(await readText(join(fixture.rootDir, "agent-servers.json")), beforeAgentConfig);
    assert.equal(await readText(join(fixture.rootDir, "acp-session-conf.json")), beforeSessionConfig);
});

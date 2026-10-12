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
import { mkdtemp } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { strict as assert } from "node:assert";
import test from "node:test";
import { ACP_AGENT_CATALOG, agentConfigForLog, discoverAgents, mergeAgentServers, presentCatalogAgents, presentListedAgentServers, presentRunnableAgentServers, resolveAgentServer, sameAgentLaunch } from "../../services/agentDiscoveryService.mjs";
import { agentLaunchKey } from "../../services/agentIdentityService.mjs";

const catalog = [
    { id: "existing", config: { name: "Existing", command: "existing", args: ["acp"], env: {} } },
    { id: "new", config: { name: "New", command: "new", args: [], env: {} } },
    { id: "missing", config: { name: "Missing", command: "missing", args: [], env: {} } },
];

test("probes candidates in parallel and returns available agents without persisting them", async () => {
    const cwd = await mkdtemp(join(tmpdir(), "insight-agent-discovery-"));
    const started = [];
    const disconnected = [];
    let release;
    const gate = new Promise((resolve) => { release = resolve; });
    const adapterFactory = ({ agentServer, requestTimeoutMs, forwardStderr }) => ({
        async request(method, params) {
            started.push(agentServer.name);
            assert.equal(method, "initialize");
            assert.equal(params.protocolVersion, 1);
            assert.equal(requestTimeoutMs, 1234);
            assert.equal(forwardStderr, false);
            if (started.length === catalog.length) release();
            await gate;
            if (agentServer.name === "Missing") throw Object.assign(new Error("spawn ENOENT"), { code: "ENOENT" });
            return { agentInfo: { name: agentServer.name } };
        },
        async disconnect() {
            disconnected.push(agentServer.name);
        },
    });

    const result = await discoverAgents({ adapterFactory, catalog, cwd, timeoutMs: 1234 });

    assert.deepEqual(new Set(started), new Set(["Existing", "New", "Missing"]));
    assert.deepEqual(new Set(disconnected), new Set(["Existing", "New", "Missing"]));
    assert.deepEqual(result.agentServers.map(({ name, command, args, env }) => ({ name, command, args, env })), [catalog[0].config, catalog[1].config]);
    assert.equal(result.results.find(({ candidate }) => candidate.id === "missing").reason, "not_found");
});

test("places discovered agents first and hides configured entries with the same launch command", () => {
    const configured = [
        { name: "Existing Alias", command: "existing", args: ["acp"], env: {} },
        { name: "Configured", command: "configured", args: [], env: {} },
    ];

    assert.deepEqual(mergeAgentServers([catalog[0].config, catalog[1].config], configured), [
        catalog[0].config,
        catalog[1].config,
        configured[1],
    ]);
});

test("reports configured launch commands as skipped without probing them", async () => {
    const cwd = await mkdtemp(join(tmpdir(), "insight-agent-discovery-skipped-"));
    const excludedLaunchKeys = new Set([agentLaunchKey(catalog[0].config)]);
    const probed = [];
    const adapterFactory = ({ agentServer }) => ({
        async request() {
            probed.push(agentServer.name);
            return { agentInfo: { name: agentServer.name } };
        },
        async disconnect() {},
    });

    const result = await discoverAgents({ adapterFactory, catalog, cwd, excludedLaunchKeys });
    const skipped = result.results.find(({ candidate }) => candidate.id === "existing");

    assert.equal(skipped.reason, "skipped_configured");
    assert.equal(skipped.elapsedMs, 0);
    assert.equal(probed.includes("Existing"), false);
});

test("compares complete ACP launch settings", () => {
    assert.equal(sameAgentLaunch(catalog[0].config, { ...catalog[0].config }), true);
    assert.equal(sameAgentLaunch(catalog[0].config, { ...catalog[0].config, args: [] }), false);
});

test("marks every built-in discovered agent as automatic", () => {
    assert.equal(ACP_AGENT_CATALOG.every(({ config }) => config.name.endsWith("(auto)")), true);
});

test("always presents catalog agents and marks missing ones unavailable", () => {
    const discovered = [ACP_AGENT_CATALOG[0].config];
    const configured = [{ name: "OpenCode", command: "opencode", args: ["acp"], env: {} }];

    assert.deepEqual(presentCatalogAgents({ discovered }).map(({ name, available }) => ({ name, available })), [
        { name: "OpenCode(auto)", available: true },
        { name: "Claude Code(auto)", available: false },
        { name: "Codex(auto)", available: false },
        { name: "Trae(auto)", available: false },
    ]);
    assert.equal(presentCatalogAgents({ discovered, configured }).find(({ name }) => name === "OpenCode(auto)").available, true);
    assert.equal(presentCatalogAgents({ discovered: [], configured }).find(({ name }) => name === "OpenCode(auto)").available, true);
    assert.equal(presentCatalogAgents({
        discovered: [],
        configured: [{ name: "Custom", command: "custom-acp", args: [], env: {} }],
    }).find(({ name }) => name === "OpenCode(auto)").available, false);
});

test("lists catalog agents before configured agents for the picker", () => {
    const listed = presentListedAgentServers({
        discovered: [ACP_AGENT_CATALOG[1].config],
        configured: [
            { name: "msinsight-native", command: "node", args: [], env: {}, kind: "builtin" },
            { name: "Custom", command: "custom-acp", args: [], env: {}, kind: "configured" },
        ],
    });

    assert.deepEqual(listed.map(({ name, available, kind }) => ({ name, available, kind })), [
        { name: "OpenCode(auto)", available: false, kind: "discovered" },
        { name: "Claude Code(auto)", available: true, kind: "discovered" },
        { name: "Codex(auto)", available: false, kind: "discovered" },
        { name: "Trae(auto)", available: false, kind: "discovered" },
        { name: "msinsight-native", available: true, kind: "builtin" },
        { name: "Custom", available: true, kind: "configured" },
    ]);
});

test("keeps a catalog agent available when a configured agent uses the same launch", () => {
    const listed = presentListedAgentServers({
        discovered: [],
        configured: [
            { name: "msinsight-native", command: "node", args: [], env: {}, kind: "builtin" },
            { name: "OpenCode", command: "opencode", args: ["acp"], env: {}, kind: "configured" },
        ],
    });

    assert.deepEqual(listed.map(({ name, available, kind }) => ({ name, available, kind })), [
        { name: "OpenCode(auto)", available: true, kind: "discovered" },
        { name: "Claude Code(auto)", available: false, kind: "discovered" },
        { name: "Codex(auto)", available: false, kind: "discovered" },
        { name: "Trae(auto)", available: false, kind: "discovered" },
        { name: "msinsight-native", available: true, kind: "builtin" },
        { name: "OpenCode", available: true, kind: "configured" },
    ]);
});

test("keeps catalog and configured agents independently runnable when they share a launch", () => {
    const configured = { name: "OpenCode", command: "opencode", args: ["acp"], env: { ACP_DEBUG: "1" } };
    const runnable = presentRunnableAgentServers({
        discovered: [],
        configured: [
            { name: "msinsight-native", command: "node", args: [], env: {}, kind: "builtin" },
            configured,
        ],
    });

    assert.deepEqual(runnable.map(({ name, command, args, env }) => ({ name, command, args, env })), [
        { name: "OpenCode(auto)", command: "opencode", args: ["acp"], env: {} },
        { name: "msinsight-native", command: "node", args: [], env: {} },
        { name: "OpenCode", command: "opencode", args: ["acp"], env: { ACP_DEBUG: "1" } },
    ]);
    assert.equal(resolveAgentServer("OpenCode(auto)", runnable)?.name, "OpenCode(auto)");
    assert.equal(resolveAgentServer("OpenCode", runnable)?.name, "OpenCode");
});

test("does not resolve a catalog agent onto a configured agent with the same launch", () => {
    const configured = { name: "OpenCode", command: "opencode", args: ["acp"], env: {} };
    assert.equal(resolveAgentServer("OpenCode(auto)", [configured]), undefined);
    assert.equal(resolveAgentServer("Custom", [configured]), undefined);
    assert.equal(resolveAgentServer("OpenCode", [configured])?.name, "OpenCode");
});

test("logs only ACP launch shape without credentials, arguments, endpoints, or paths", () => {
    assert.deepEqual(agentConfigForLog({
        name: "Secure(auto)",
        command: "C:\\sensitive\\secure-acp.exe",
        args: ["serve", "--endpoint=https://credential.example"],
        env: { API_KEY: "key-value", ACCESS_TOKEN: "token-value", ENDPOINT: "https://example.test" },
    }), {
        name: "Secure(auto)",
        commandName: "secure-acp.exe",
        argCount: 2,
        envKeys: ["ACCESS_TOKEN", "API_KEY", "ENDPOINT"],
    });
});

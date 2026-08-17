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
import { ACP_AGENT_CATALOG, agentConfigForLog, discoverAgents, mergeAgentServers, sameAgentLaunch } from "../../services/agentDiscoveryService.mjs";
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

test("redacts credentials when discovered configurations are written to logs", () => {
    assert.deepEqual(agentConfigForLog({
        name: "Secure(auto)",
        command: "secure-acp",
        args: ["serve"],
        env: { API_KEY: "key-value", ACCESS_TOKEN: "token-value", ENDPOINT: "https://example.test" },
    }), {
        name: "Secure(auto)",
        command: "secure-acp",
        args: ["serve"],
        env: { API_KEY: "***", ACCESS_TOKEN: "***", ENDPOINT: "https://example.test" },
    });
});

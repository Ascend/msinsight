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
import { createCapabilitySessionIntegration } from "../../capability-center/sessionIntegration.mjs";

const fixture = ({ activeAgentName = "OpenCode", agentInfoName = "OpenCode" } = {}) => {
    let connectionVersion = 0;
    let connected = false;
    const state = {
        activeAgentName,
        agentInfo: { name: agentInfoName },
        agentCapabilities: { mcp: { http: true } },
    };
    const integration = createCapabilitySessionIntegration({
        baseUrl: "http://127.0.0.1:9090",
        accessToken: "capability-token",
        state,
        connectionVersion: () => connectionVersion,
        hasConnections: () => connected,
    });
    return {
        integration,
        state,
        connect: () => {
            connectionVersion += 1;
            connected = true;
        },
        disconnect: () => { connected = false; },
    };
};

const expectedMcpServer = {
    type: "http",
    name: "msinsight-capabilities",
    url: "http://127.0.0.1:9090/mcp/capabilities",
    headers: [{ name: "Authorization", value: "Bearer capability-token" }],
};

test("OpenCode receives the global MCP configuration once per agent process", async () => {
    const { integration, connect } = fixture();
    const calls = [];

    await integration.withMcpServers(async (servers) => {
        calls.push(servers);
        connect();
    });
    await integration.withMcpServers(async (servers) => { calls.push(servers); });
    integration.reset();
    await integration.withMcpServers(async (servers) => { calls.push(servers); });

    assert.deepEqual(calls, [[expectedMcpServer], [], [expectedMcpServer]]);
});

test("OpenCode retries MCP injection when no MCP connection was established", async () => {
    const { integration } = fixture();
    const calls = [];

    await integration.withMcpServers(async (servers) => { calls.push(servers); });
    await integration.withMcpServers(async (servers) => { calls.push(servers); });

    assert.deepEqual(calls, [[expectedMcpServer], [expectedMcpServer]]);
});

test("OpenCode reinjects the MCP server after its connection is lost", async () => {
    const { integration, connect, disconnect } = fixture();
    const calls = [];

    await integration.withMcpServers(async (servers) => {
        calls.push(servers);
        connect();
    });
    disconnect();
    await integration.withMcpServers(async (servers) => { calls.push(servers); });

    assert.deepEqual(calls, [[expectedMcpServer], [expectedMcpServer]]);
});

test("other HTTP MCP agents receive the stable configuration for every session", async () => {
    const { integration } = fixture({ activeAgentName: "Codex", agentInfoName: "Codex" });
    const calls = [];

    await integration.withMcpServers(async (servers) => { calls.push(servers); });
    await integration.withMcpServers(async (servers) => { calls.push(servers); });

    assert.deepEqual(calls, [[expectedMcpServer], [expectedMcpServer]]);
});

test("session integration skips agents without HTTP MCP and the native agent", async () => {
    const unsupported = fixture();
    unsupported.state.agentCapabilities.mcp.http = false;
    assert.deepEqual(await unsupported.integration.withMcpServers(async (servers) => servers), []);

    const native = fixture({ activeAgentName: "msinsight-native", agentInfoName: "msinsight-native" });
    assert.deepEqual(await native.integration.withMcpServers(async (servers) => servers), []);
});

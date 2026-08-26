/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { once } from "node:events";
import { resolve } from "node:path";
import test from "node:test";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { createApp } from "../../app.mjs";
import { createHttpMcpAdapter } from "../../capability-adapters/http-mcp/adapter.mjs";
import { createCliCapability } from "../../capability-center/cliCapability.mjs";
import { createCapabilityCenter } from "../../capability-center/service.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";

const startFixture = async () => {
    const requests = [];
    const frontendCommandService = {
        async request(request) {
            requests.push(request);
            return { activeModule: "Timeline", command: request.command };
        },
    };
    const capabilityCenter = createCapabilityCenter({ frontendCommandService });
    capabilityCenter.register(createCliCapability({
        name: "pt_snap",
        executable: resolve("pt-snap"),
        spawnProcess: () => { throw new Error("not used in this test"); },
    }));
    const accessToken = "global-capability-token";
    const httpMcpAdapter = createHttpMcpAdapter({ capabilityCenter, accessToken });
    const server = createApp({
        capabilityCenter,
        httpMcpAdapter,
        state: createRuntimeState(),
        allowedOrigins: [],
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    return { accessToken, httpMcpAdapter, requests, server, url: `http://127.0.0.1:${server.address().port}` };
};

test("HTTP MCP adapter exposes and invokes global capability center tools", async (t) => {
    const fixture = await startFixture();
    t.after(async () => {
        await fixture.httpMcpAdapter.close();
        fixture.server.close();
        await once(fixture.server, "close");
    });
    const client = new Client({ name: "test-client", version: "1.0.0" });
    const transport = new StreamableHTTPClientTransport(
        new URL("/mcp/capabilities", fixture.url),
        { requestInit: { headers: { Authorization: `Bearer ${fixture.accessToken}` } } },
    );
    t.after(() => client.close());
    await client.connect(transport);

    const listed = await client.listTools();
    const result = await client.callTool({ name: "msinsight", arguments: { command: "observe", args: {} } });

    assert.deepEqual(listed.tools.map(({ name }) => name), ["msinsight", "pt_snap"]);
    assert.equal(result.isError, undefined);
    assert.deepEqual(result.structuredContent, { activeModule: "Timeline", command: "observe" });
    assert.equal(fixture.requests[0].sessionId, "");
});

test("HTTP MCP adapter rejects missing bearer credentials", async (t) => {
    const fixture = await startFixture();
    t.after(async () => {
        await fixture.httpMcpAdapter.close();
        fixture.server.close();
        await once(fixture.server, "close");
    });

    const response = await fetch(`${fixture.url}/mcp/capabilities`, { method: "POST" });

    assert.equal(response.status, 401);
});

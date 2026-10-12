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
    const ragRequests = [];
    const capabilityCenter = createCapabilityCenter({
        frontendCommandService,
        ragService: {
            isEnabled: () => true,
            getStatus: () => ({ enabled: true }),
            async retrieve(query) {
                ragRequests.push(query);
                return {
                    status: "ok",
                    kbId: "mindstudio-insight",
                    kbVersion: "26.1.3",
                    retrievedChunks: [{ sourceLabel: "用户指南", knowledgeText: "检索结果" }],
                };
            },
        },
    });
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
    return { accessToken, httpMcpAdapter, ragRequests, requests, server, url: `http://127.0.0.1:${server.address().port}` };
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
    const ragResult = await client.callTool({ name: "rag_retrieve", arguments: { query: "如何导入数据" } });

    assert.deepEqual(listed.tools.map(({ name }) => name), ["msinsight", "rag_retrieve", "pt_snap"]);
    assert.equal(result.isError, undefined);
    assert.deepEqual(result.structuredContent, { activeModule: "Timeline", command: "observe" });
    assert.equal(ragResult.isError, undefined);
    assert.equal(ragResult.structuredContent.status, "ok");
    assert.equal(ragResult.structuredContent.sources[0].sourceLabel, "用户指南");
    assert.deepEqual(fixture.ragRequests, ["如何导入数据"]);
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

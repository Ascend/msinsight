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
import { strict as assert } from "node:assert";
import { Readable } from "node:stream";
import test from "node:test";
import { createRouter } from "../../http/router.mjs";
import { createAgentConfigController } from "../../controllers/agentConfigController.mjs";

const createResponse = () => ({
    status: undefined,
    headers: undefined,
    body: undefined,
    writeHead(status, headers) {
        this.status = status;
        this.headers = headers;
    },
    end(body) {
        this.body = JSON.parse(body);
    },
});

const createJsonRequest = (method, path, body) => {
    const req = Readable.from(body === undefined ? [] : [JSON.stringify(body)]);
    req.method = method;
    req.url = path;
    req.headers = { host: "127.0.0.1" };
    return req;
};

const createTestRouter = (agentConfigService) => createRouter({
    agentController: {},
    chatController: {},
    sessionController: {},
    eventController: {},
    permissionController: {},
    agentConfigController: createAgentConfigController({ agentConfigService }),
});

test("GET /api/agent-config returns the normalized settings snapshot", async () => {
    const snapshot = { activeAgentName: "OpenCode", agentServers: [], sessionConfig: {} };
    const router = createTestRouter({ readSnapshot: async () => snapshot });
    const res = createResponse();

    await router(createJsonRequest("GET", "/api/agent-config"), res);

    assert.equal(res.status, 200);
    assert.deepEqual(res.body, { snapshot });
});

test("configuration sections use independent save endpoints", async () => {
    const calls = [];
    const service = {
        saveAgentServers: async (body) => {
            calls.push(["servers", body]);
            return { error: "agent_busy", message: "Agent is busy", status: 409 };
        },
        saveBuiltinAgent: async (body) => {
            calls.push(["builtin", body]);
            return { ok: true };
        },
        saveSessionConfig: async (body) => {
            calls.push(["session", body]);
            return { ok: true };
        },
    };
    const router = createTestRouter(service);
    const serverConfig = { activeAgentName: "OpenCode", agentServers: [] };
    const builtinConfig = { provider: "openai" };
    const sessionConfig = { requestTimeoutMs: 1000 };
    const serverResponse = createResponse();

    await router(createJsonRequest("PUT", "/api/agent-config/servers", serverConfig), serverResponse);
    await router(createJsonRequest("PUT", "/api/agent-config/builtin", builtinConfig), createResponse());
    await router(createJsonRequest("PUT", "/api/agent-config/session", sessionConfig), createResponse());

    assert.deepEqual(calls, [
        ["servers", serverConfig],
        ["builtin", builtinConfig],
        ["session", sessionConfig],
    ]);
    assert.equal(serverResponse.status, 409);
    assert.deepEqual(serverResponse.body, {
        error: "agent_busy",
        code: "agent_busy",
        message: "Agent is busy",
    });
});

test("GET /api/agent-config returns a specific configuration read error", async () => {
    const router = createTestRouter({
        readSnapshot: async () => {
            throw new Error("invalid JSON in agent-servers.json");
        },
    });
    const res = createResponse();

    await router(createJsonRequest("GET", "/api/agent-config"), res);

    assert.equal(res.status, 500);
    assert.equal(res.body.error, "agent_config_read_failed");
    assert.equal(res.body.message, "Agent settings could not be read from the local configuration files");
    assert.deepEqual(res.body.details, { cause: "invalid JSON in agent-servers.json" });
});

test("legacy aggregate save endpoint is not available", async () => {
    const calls = [];
    const router = createTestRouter({
        saveAgentServers: async (body) => {
            calls.push(body);
            return { ok: true };
        },
    });
    const res = createResponse();

    await router(createJsonRequest("PUT", "/api/agent-config", {}), res);

    assert.deepEqual(calls, []);
    assert.equal(res.status, 404);
});

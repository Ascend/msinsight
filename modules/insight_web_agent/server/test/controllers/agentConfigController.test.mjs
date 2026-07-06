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
    contextController: {},
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

test("PUT /api/agent-config saves the submitted snapshot and returns structured errors", async () => {
    const submitted = { activeAgentName: "OpenCode" };
    const calls = [];
    const router = createTestRouter({
        saveSnapshot: async (body) => {
            calls.push(body);
            return { error: "agent_busy", message: "Agent is busy", status: 409 };
        },
    });
    const res = createResponse();

    await router(createJsonRequest("PUT", "/api/agent-config", submitted), res);

    assert.deepEqual(calls, [submitted]);
    assert.equal(res.status, 409);
    assert.deepEqual(res.body, { error: "agent_busy", message: "Agent is busy" });
});

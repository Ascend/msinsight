/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { EventEmitter } from "node:events";
import test from "node:test";
import { createCapabilityController } from "../../controllers/capabilityController.mjs";

const createRequest = (token, { query = false } = {}) => Object.assign(new EventEmitter(), {
    url: query ? `/api/capabilities/invoke?capabilityToken=${encodeURIComponent(token)}` : "/api/capabilities/invoke",
    headers: query
        ? { host: "127.0.0.1" }
        : { host: "127.0.0.1", authorization: `Bearer ${token}` },
});
const createResponse = () => Object.assign(new EventEmitter(), {
    destroyed: false,
    status: undefined,
    body: undefined,
    writeHead(status) { this.status = status; },
    end(body) { this.body = JSON.parse(body); },
});

test("Native capability controller rejects the frontend API token", async () => {
    let invocations = 0;
    const controller = createCapabilityController({
        capabilityCenter: { invoke: async () => { invocations += 1; } },
        accessToken: "native-only",
    });
    const res = createResponse();

    await controller.invoke(createRequest("frontend-token"), res, { name: "pt_snap" });

    assert.equal(res.status, 401);
    assert.equal(invocations, 0);
});

test("Native capability controller maps capability errors to specific HTTP statuses", async () => {
    const cases = [
        ["COMMAND_NOT_FOUND", 404],
        ["COMMAND_INVALID", 400],
        ["COMMAND_PERMISSION_DENIED", 403],
        ["COMMAND_TIMEOUT", 408],
        ["COMMAND_BUSY", 409],
        ["COMMAND_UNAVAILABLE", 422],
        ["COMMAND_CONNECTION_LOST", 503],
        ["CAPABILITY_EXECUTION_FAILED", 500],
    ];

    for (const [code, status] of cases) {
        const controller = createCapabilityController({
            capabilityCenter: {
                invoke: async () => {
                    throw Object.assign(new Error(`failed with ${code}`), { code, retryable: false });
                },
            },
            accessToken: "native-only",
        });
        const res = createResponse();

        await controller.invoke(createRequest("native-only"), res, { name: "msinsight" });

        assert.equal(res.status, status, code);
        assert.equal(res.body.code, code, code);
        assert.equal(res.body.error, code, code);
        assert.equal(res.body.message, `failed with ${code}`, code);
    }
});

test("Native capability controller accepts its process token", async () => {
    const requests = [];
    const controller = createCapabilityController({
        capabilityCenter: {
            async invoke(request) {
                requests.push(request);
                return { ok: true };
            },
        },
        accessToken: "native-only",
    });
    const res = createResponse();

    await controller.invoke(createRequest("native-only"), res, { name: "pt_snap", input: { args: ["query"] } });

    assert.equal(res.status, 200);
    assert.equal(requests[0].name, "pt_snap");
    assert.deepEqual(requests[0].input, { args: ["query"] });
});

test("Native capability controller rejects process tokens in the query string", async () => {
    const controller = createCapabilityController({
        capabilityCenter: { invoke: async () => ({ ok: true }) },
        accessToken: "native-only",
    });
    const res = createResponse();

    await controller.invoke(createRequest("native-only", { query: true }), res, { name: "pt_snap" });

    assert.equal(res.status, 401);
});

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

const createRequest = (token) => Object.assign(new EventEmitter(), {
    url: `/api/capabilities/invoke?capabilityToken=${encodeURIComponent(token)}`,
    headers: { host: "127.0.0.1" },
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

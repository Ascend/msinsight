/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { strict as assert } from "node:assert";
import test from "node:test";
import { createFrontendCommandController } from "../../controllers/frontendCommandController.mjs";

const createResponse = () => ({
    status: undefined,
    body: undefined,
    writeHead(status) {
        this.status = status;
    },
    end(body) {
        this.body = JSON.parse(body);
    },
});

test("frontend command failures keep their code and readable message", async () => {
    const controller = createFrontendCommandController({
        frontendCommandService: {
            request: async () => {
                throw Object.assign(new Error("command is required"), {
                    code: "COMMAND_INVALID",
                    retryable: false,
                });
            },
        },
    });
    const res = createResponse();

    await controller.request(undefined, res, {});

    assert.equal(res.status, 400);
    assert.equal(res.body.error, "command_invalid");
    assert.equal(res.body.code, "command_invalid");
    assert.equal(res.body.message, "command is required");
    assert.equal(res.body.retryable, false);
});

test("frontend command failures map command semantics to HTTP status codes", async () => {
    const cases = [
        ["COMMAND_NOT_FOUND", 404],
        ["COMMAND_PERMISSION_DENIED", 403],
        ["COMMAND_TIMEOUT", 408],
        ["COMMAND_BUSY", 409],
        ["COMMAND_UNAVAILABLE", 422],
        ["COMMAND_CONNECTION_LOST", 503],
        ["COMMAND_EXECUTION_FAILED", 500],
    ];

    for (const [code, status] of cases) {
        const controller = createFrontendCommandController({
            frontendCommandService: {
                request: async () => {
                    throw Object.assign(new Error(`failed with ${code}`), { code, retryable: false });
                },
            },
        });
        const res = createResponse();

        await controller.request(undefined, res, { command: "Timeline.zoom" });

        assert.equal(res.status, status, code);
        assert.equal(res.body.code, code.toLowerCase(), code);
    }
});

test("frontend command claim validation explains the missing field", () => {
    const controller = createFrontendCommandController({ frontendCommandService: {} });
    const res = createResponse();

    controller.claim(undefined, res, {});

    assert.equal(res.status, 400);
    assert.equal(res.body.error, "request_id_required");
    assert.equal(res.body.message, "requestId is required to claim a frontend command");
});

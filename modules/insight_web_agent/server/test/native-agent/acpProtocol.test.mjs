/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { PassThrough } from "node:stream";
import test from "node:test";
import { createAcpProtocolServer } from "../../native-agent/acp/protocol.mjs";

test("ACP protocol executes notifications without writing responses", async () => {
    const input = new PassThrough();
    const requests = [];
    const output = [];
    const server = createAcpProtocolServer({
        input,
        writeJson: (message) => output.push(message),
        handleRequest: async (method, params) => { requests.push({ method, params }); },
    });
    await server.start();

    input.write(`${JSON.stringify({ jsonrpc: "2.0", method: "session/cancel", params: { sessionId: "session-1" } })}\n`);
    await new Promise((resolve) => setImmediate(resolve));

    assert.deepEqual(requests, [{ method: "session/cancel", params: { sessionId: "session-1" } }]);
    assert.deepEqual(output, []);
    input.end();
});

test("ACP protocol sends results only for requests", async () => {
    const input = new PassThrough();
    const output = [];
    const server = createAcpProtocolServer({
        input,
        writeJson: (message) => output.push(message),
        handleRequest: async () => ({ stopReason: "cancelled" }),
    });
    await server.start();

    input.write(`${JSON.stringify({ jsonrpc: "2.0", id: 1, method: "session/prompt", params: {} })}\n`);
    await new Promise((resolve) => setImmediate(resolve));

    assert.deepEqual(output, [{ jsonrpc: "2.0", id: 1, result: { stopReason: "cancelled" } }]);
    input.end();
});

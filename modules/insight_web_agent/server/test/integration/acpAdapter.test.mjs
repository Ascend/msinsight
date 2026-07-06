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

import { EventEmitter } from "node:events";
import { test } from "node:test";
import assert from "node:assert/strict";
import { createAcpAdapter } from "../../infrastructure/acpAdapter.mjs";

const createMockChild = () => {
    const child = new EventEmitter();
    child.stdout = new EventEmitter();
    child.stdout.setEncoding = () => {};
    child.stderr = new EventEmitter();
    child.stderr.setEncoding = () => {};
    child.stdin = {
        writes: [],
        write(line) {
            this.writes.push(JSON.parse(line));
        },
    };
    child.kill = () => queueMicrotask(() => child.emit("exit", 0, null));
    return child;
};

const createAdapter = (options = {}) => {
    const children = [];
    const adapter = createAcpAdapter({
        agentServer: { name: "mock-agent", command: "mock", args: [] },
        cwd: "/tmp",
        spawnProcess: () => {
            const child = createMockChild();
            children.push(child);
            return child;
        },
        requestTimeoutMs: options.requestTimeoutMs ?? 30000,
        promptRequestTimeoutMs: options.promptRequestTimeoutMs ?? 300000,
    });
    return { adapter, children };
};

const emitLine = (child, message) => child.stdout.emit("data", `${JSON.stringify(message)}\n`);

const sleep = (ms = 0) => new Promise((resolve) => setTimeout(resolve, ms));

test("createAcpAdapter returns public adapter shape", () => {
    const { adapter } = createAdapter();
    assert.equal(adapter.runtime, "stdio");
    assert.equal(adapter.agentId, "mock-agent");
    for (const key of ["request", "notify", "registerHandler", "unregisterHandler", "connect", "disconnect", "send", "onMessage"]) {
        assert.equal(typeof adapter[key], "function", key);
    }
});

test("onMessage unsubscribe prevents further notification delivery", async () => {
    const { adapter, children } = createAdapter();
    adapter.connect();
    const messages = [];
    const unsubscribe = adapter.onMessage((message) => messages.push(message));
    emitLine(children[0], { jsonrpc: "2.0", method: "session/update", params: { step: 1 } });
    unsubscribe();
    emitLine(children[0], { jsonrpc: "2.0", method: "session/update", params: { step: 2 } });
    await sleep();
    assert.deepEqual(messages.map((message) => message.params.step), [1]);
});

test("registered ping handler responds with result", async () => {
    const { adapter, children } = createAdapter();
    adapter.connect();
    adapter.registerHandler("ping", () => ({ result: { pong: true } }));
    emitLine(children[0], { jsonrpc: "2.0", id: 1, method: "ping" });
    await sleep();
    assert.deepEqual(children[0].stdin.writes.at(-1), { jsonrpc: "2.0", id: 1, result: { pong: true } });
});

test("registered host handler overrides M0 default stub", async () => {
    const { adapter, children } = createAdapter();
    adapter.connect();
    adapter.registerHandler("fs/read_text_file", () => ({ result: { content: "ok" } }));

    emitLine(children[0], { jsonrpc: "2.0", id: 3, method: "fs/read_text_file", params: { sessionId: "s1", path: "/tmp/a" } });
    await sleep();

    assert.deepEqual(children[0].stdin.writes.at(-1), { jsonrpc: "2.0", id: 3, result: { content: "ok" } });
});

test("unregistered host request returns method_not_found error", async () => {
    const { adapter, children } = createAdapter();
    adapter.connect();
    emitLine(children[0], { jsonrpc: "2.0", id: 2, method: "missing/method" });
    await sleep();
    assert.deepEqual(children[0].stdin.writes.at(-1), {
        jsonrpc: "2.0",
        id: 2,
        error: { code: -32601, message: "method_not_found in M0" },
    });
});

test("request rejects after timeout", async () => {
    const { adapter } = createAdapter({ requestTimeoutMs: 1 });
    adapter.connect();
    await assert.rejects(adapter.request("slow", {}), /ACP request timed out: slow/);
});

test("session/prompt uses the longer prompt timeout", async () => {
    const { adapter, children } = createAdapter({ requestTimeoutMs: 1, promptRequestTimeoutMs: 50 });
    adapter.connect();

    const pending = adapter.request("session/prompt", { sessionId: "s1" });
    await sleep(5);
    emitLine(children[0], { jsonrpc: "2.0", id: 1, result: { ok: true } });

    await assert.doesNotReject(pending);
});

test("disconnect rejects pending requests and cleans up", async () => {
    const { adapter } = createAdapter();
    adapter.connect();
    const pending = adapter.request("never", {});
    await adapter.disconnect();
    await assert.rejects(pending, /ACP adapter disconnected/);
});

test("unexpected process exit notifies subscribers", async () => {
    const { adapter, children } = createAdapter();
    const messages = [];
    adapter.onMessage((message) => messages.push(message));
    adapter.connect();

    children[0].emit("exit", 1, null);
    await sleep();

    assert.equal(messages[0].kind, "transport_error");
    assert.match(messages[0].error.message, /ACP server exited/);
});

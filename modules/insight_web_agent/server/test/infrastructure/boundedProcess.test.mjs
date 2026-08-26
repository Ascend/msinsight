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
import { PassThrough } from "node:stream";
import test from "node:test";
import { resolve } from "node:path";
import { runBoundedProcess } from "../../infrastructure/boundedProcess.mjs";

const createChild = () => {
    const child = new EventEmitter();
    child.pid = 123;
    child.stdout = new PassThrough();
    child.stderr = new PassThrough();
    child.kill = () => {};
    return child;
};

test("bounded process terminates on output overflow", async () => {
    const calls = [];
    const child = createChild();
    const spawnProcess = (command, args) => {
        calls.push({ command, args });
        if (command === "taskkill") return new EventEmitter();
        queueMicrotask(() => {
            child.stdout.write("12345");
            child.emit("close", 0, null);
        });
        return child;
    };

    await assert.rejects(runBoundedProcess({
        executable: resolve("tool"),
        args: [],
        signal: undefined,
        timeoutMs: 1000,
        maxOutputBytes: 4,
        spawnProcess,
    }), { code: "CLI_OUTPUT_LIMIT" });
    if (process.platform === "win32") assert.equal(calls.some(({ command }) => command === "taskkill"), true);
});

test("bounded process propagates cancellation", async () => {
    const controller = new AbortController();
    const child = createChild();
    const spawnProcess = (command) => {
        if (command === "taskkill") return new EventEmitter();
        queueMicrotask(() => {
            controller.abort(new Error("cancelled by test"));
            child.emit("close", null, "SIGKILL");
        });
        return child;
    };

    await assert.rejects(runBoundedProcess({
        executable: resolve("tool"),
        args: [],
        signal: controller.signal,
        timeoutMs: 1000,
        maxOutputBytes: 100,
        spawnProcess,
    }), { code: "CLI_CANCELLED", message: "cancelled by test" });
});

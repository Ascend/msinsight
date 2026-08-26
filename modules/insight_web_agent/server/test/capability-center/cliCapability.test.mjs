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
import { createCliCapability } from "../../capability-center/cliCapability.mjs";

const createChild = () => {
    const child = new EventEmitter();
    child.pid = 123;
    child.stdout = new PassThrough();
    child.stderr = new PassThrough();
    child.kill = () => {};
    return child;
};

test("CLI capability passes structured argv without a shell", async () => {
    const calls = [];
    const spawnProcess = (command, args, options) => {
        calls.push({ command, args, options });
        const child = createChild();
        queueMicrotask(() => {
            child.stdout.end("ok");
            child.emit("close", 0, null);
        });
        return child;
    };
    const executable = resolve("product", process.platform === "win32" ? "pt-snap.exe" : "pt-snap");
    const cwd = resolve("workspace");
    const capability = createCliCapability({
        name: "pt_snap",
        executable,
        cwd,
        env: { PATH: "C:\\Windows", API_TOKEN: "secret", HTTP_PROXY: "http://user:pass@proxy" },
        spawnProcess,
    });

    const result = await capability.execute({ args: ["query", "--list"] }, {});

    assert.deepEqual(result, { exitCode: 0, signal: undefined, stdout: "ok", stderr: "" });
    assert.equal(calls[0].command, executable);
    assert.deepEqual(calls[0].args, ["query", "--list"]);
    assert.equal(calls[0].options.shell, false);
    assert.equal(calls[0].options.cwd, cwd);
    assert.deepEqual(calls[0].options.env, { PATH: "C:\\Windows" });
});

test("CLI capability enforces its concurrency limit", async () => {
    let release;
    const waiting = new Promise((resolve) => { release = resolve; });
    const spawnProcess = () => {
        const child = createChild();
        void waiting.then(() => child.emit("close", 0, null));
        return child;
    };
    const capability = createCliCapability({
        name: "pt_snap",
        executable: resolve("product", process.platform === "win32" ? "pt-snap.exe" : "pt-snap"),
        spawnProcess,
    });
    const first = capability.execute({ args: ["query"] }, {});

    await assert.rejects(capability.execute({ args: ["metadata"] }, {}), { code: "CLI_BUSY" });
    release();
    await first;
});

test("CLI capability rejects malformed input and non-zero exits", async () => {
    const spawnProcess = () => {
        const child = createChild();
        queueMicrotask(() => {
            child.stderr.end("failed");
            child.emit("close", 2, null);
        });
        return child;
    };
    const capability = createCliCapability({
        name: "pt_snap",
        executable: resolve("product", process.platform === "win32" ? "pt-snap.exe" : "pt-snap"),
        spawnProcess,
    });

    assert.throws(() => capability.validate({ args: "query" }), { code: "CAPABILITY_INVALID_ARGUMENT" });
    assert.throws(() => capability.validate({ args: ["query\0bad"] }), { code: "CAPABILITY_INVALID_ARGUMENT" });
    assert.throws(() => capability.validate({ args: ["query"], cwd: resolve("other") }), { code: "CAPABILITY_INVALID_ARGUMENT" });
    await assert.rejects(capability.execute({ args: ["query"] }, {}), { code: "CLI_EXIT_NONZERO" });
});

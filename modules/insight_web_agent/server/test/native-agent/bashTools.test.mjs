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
import { mkdtemp, realpath, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { PassThrough } from "node:stream";
import test from "node:test";
import { createBashTools } from "../../native-agent/tools/bashTools.mjs";

const createSession = async (root, rules) => ({
    sessionId: "session-1",
    primaryAgentId: "general",
    primaryAgentBashRules: rules,
    canonicalFilesystemRoots: [await realpath(root)],
});

const createChild = () => {
    const child = new EventEmitter();
    child.pid = 123;
    child.stdout = new PassThrough();
    child.stderr = new PassThrough();
    child.kill = () => {};
    return child;
};

const bashRuntime = {
    kind: "bash",
    executable: "bash",
    commandArgs: ["-lc"],
    displayName: "Bash",
    description: "Run one foreground, non-interactive Bash command.",
    prepareCommand: command => command,
};

test("native Bash executes an Agent-allowed command and returns bounded process output", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-bash-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "printf *", behavior: "allow" }]);
    const calls = [];
    const spawnProcess = (command, args, options) => {
        calls.push({ command, args, options });
        const child = createChild();
        queueMicrotask(() => {
            child.stdout.end("hello");
            child.stderr.end("warning");
            child.emit("close", 0, null);
        });
        return child;
    };
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: { request: async () => { throw new Error("approval should not be requested"); } },
        cwd: root,
        spawnProcess,
        shellRuntime: bashRuntime,
    });

    assert.deepEqual(await bash.execute({ command: "printf hello" }, { sessionId: session.sessionId }), {
        exitCode: 0,
        signal: undefined,
        stdout: "hello",
        stderr: "warning",
    });
    assert.equal(calls[0].command, "bash");
    assert.deepEqual(calls[0].args, ["-lc", "printf hello"]);
    assert.equal(calls[0].options.cwd, root);
});

test("native Bash compatibility tool executes PowerShell commands on Windows", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-powershell-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "Get-ChildItem", behavior: "allow" }]);
    const calls = [];
    const spawnProcess = (command, args, options) => {
        calls.push({ command, args, options });
        const child = createChild();
        queueMicrotask(() => child.emit("close", 0, null));
        return child;
    };
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: { request: async () => { throw new Error("approval should not be requested"); } },
        cwd: root,
        spawnProcess,
        shellRuntime: {
            kind: "powershell",
            executable: "powershell.exe",
            commandArgs: ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command"],
            displayName: "PowerShell",
            description: "Run one foreground, non-interactive PowerShell command.",
            prepareCommand: command => `encoding; ${command}`,
        },
    });

    await bash.execute({ command: "Get-ChildItem" }, { sessionId: session.sessionId });

    assert.equal(bash.name, "Bash");
    assert.match(bash.description, /PowerShell/);
    assert.equal(calls[0].command, "powershell.exe");
    assert.deepEqual(calls[0].args, [
        "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", "encoding; Get-ChildItem",
    ]);
});

test("native Bash asks the ACP host before executing unmatched commands", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-bash-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "*", behavior: "ask" }]);
    const approvals = [];
    const spawnProcess = () => {
        const child = createChild();
        queueMicrotask(() => child.emit("close", 0, null));
        return child;
    };
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: {
            request: async (method, params) => {
                approvals.push({ method, params });
                return { outcome: { optionId: "allow_once" } };
            },
        },
        cwd: root,
        spawnProcess,
        shellRuntime: bashRuntime,
    });

    await bash.execute({ command: "python -V" }, { sessionId: session.sessionId });

    assert.equal(approvals[0].method, "session/request_permission");
    assert.equal(approvals[0].params.kind, "bash");
    assert.equal(approvals[0].params.rememberKey, "bash:general:python -V");
    assert.equal(approvals[0].params.title, "Run Bash command");
});

test("native Bash compatibility tool identifies PowerShell in approval requests", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-powershell-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "*", behavior: "ask" }]);
    const approvals = [];
    const spawnProcess = () => {
        const child = createChild();
        queueMicrotask(() => child.emit("close", 0, null));
        return child;
    };
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: {
            request: async (method, params) => {
                approvals.push({ method, params });
                return { outcome: { optionId: "allow_once" } };
            },
        },
        cwd: root,
        spawnProcess,
        shellRuntime: {
            kind: "powershell",
            executable: "powershell.exe",
            commandArgs: ["-Command"],
            displayName: "PowerShell",
            description: "Run one foreground, non-interactive PowerShell command.",
            prepareCommand: command => `encoding; ${command}`,
        },
    });

    await bash.execute({ command: "Get-ChildItem" }, { sessionId: session.sessionId });

    assert.equal(approvals[0].params.kind, "bash");
    assert.equal(approvals[0].params.title, "Run PowerShell command");
    assert.equal(approvals[0].params.target, "Get-ChildItem");
    assert.equal(approvals[0].params.rememberKey, "bash:general:Get-ChildItem");
});

test("native Bash rejects product-denied commands without requesting approval or spawning", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-bash-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "*", behavior: "allow" }]);
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: { request: async () => { throw new Error("approval should not be requested"); } },
        cwd: root,
        spawnProcess: () => { throw new Error("process should not be spawned"); },
        shellRuntime: bashRuntime,
    });

    await assert.rejects(
        bash.execute({ command: "sudo reboot" }, { sessionId: session.sessionId }),
        /denied by the product Bash policy/,
    );
});

test("native Bash reserves the session while approval is pending", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-native-bash-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = await createSession(root, [{ pattern: "*", behavior: "ask" }]);
    let approve;
    const approval = new Promise((resolve) => { approve = resolve; });
    const spawnProcess = () => {
        const child = createChild();
        queueMicrotask(() => child.emit("close", 0, null));
        return child;
    };
    const [bash] = createBashTools({
        sessions: new Map([[session.sessionId, session]]),
        hostClient: { request: () => approval },
        cwd: root,
        spawnProcess,
        shellRuntime: bashRuntime,
    });
    const first = bash.execute({ command: "python -V" }, { sessionId: session.sessionId });

    await assert.rejects(
        bash.execute({ command: "python -m pip --version" }, { sessionId: session.sessionId }),
        /already running/,
    );
    approve({ outcome: { optionId: "allow_once" } });
    await first;
});

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
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdir, mkdtemp, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { createServer } from "node:net";
import { strict as assert } from "node:assert";
import test from "node:test";

const serverEntry = fileURLToPath(new URL("../../index.mjs", import.meta.url));

const fakeAgentSource = `
import { appendFileSync, existsSync, unlinkSync } from "node:fs";
import readline from "node:readline";

const mode = process.argv[2] ?? "ok";
const logPath = process.env.FAKE_AGENT_LOG;
const notifyFlagPath = process.env.FAKE_AGENT_NOTIFY_FLAG;
const oldRuntimeCommandName = process.env.FAKE_AGENT_OLD_COMMAND ?? "stable-live";
const brokenInitDelayMs = Number(process.env.BROKEN_INIT_DELAY_MS ?? 0);
const log = (message) => {
    if (logPath) appendFileSync(logPath, message + "\\n");
};

log("start:" + mode + ":" + process.pid);
let initialized = false;
const notifyFlagInterval = setInterval(() => {
    if (mode !== "ok" || !initialized || !notifyFlagPath || !existsSync(notifyFlagPath)) return;
    unlinkSync(notifyFlagPath);
    notify("session/update", {
        update: {
            sessionUpdate: "available_commands_update",
            availableCommands: [{ name: oldRuntimeCommandName, description: "old runtime command" }],
        },
    });
}, 5);
process.on("SIGTERM", () => {
    log("signal:" + mode + ":SIGTERM");
    exitAfterShutdownNotification();
});
process.on("exit", (code) => {
    clearInterval(notifyFlagInterval);
    log("exit:" + mode + ":" + code);
});
process.stdin.on("end", () => {
    log("stdin_end:" + mode);
    exitAfterShutdownNotification();
});

const send = (id, payload) => process.stdout.write(JSON.stringify({ jsonrpc: "2.0", id, ...payload }) + "\\n");
const notify = (method, params) => process.stdout.write(JSON.stringify({ jsonrpc: "2.0", method, params }) + "\\n");
const exitAfterShutdownNotification = () => {
    if (mode !== "fail") {
        process.exit(0);
        return;
    }
    notify("session/update", {
        update: {
            sessionUpdate: "available_commands_update",
            availableCommands: [{ name: "broken-only", description: "failed runtime command" }],
        },
    });
    setImmediate(() => process.exit(0));
};
const lineReader = readline.createInterface({ input: process.stdin });
lineReader.on("line", (line) => {
    const message = JSON.parse(line);
    if (mode === "fail" && message.method === "initialize") {
        log("initialize:" + mode);
        setTimeout(() => {
            send(message.id, { error: { code: -32000, message: "failed initialize for Broken" } });
        }, brokenInitDelayMs);
        return;
    }
    if (message.method === "initialize") {
        log("initialize:" + mode);
        initialized = true;
        send(message.id, {
            result: {
                agentInfo: { name: "Stable", version: "test" },
                agentCapabilities: { session: { list: true, delete: true } },
            },
        });
        return;
    }
    if (message.method === "session/list") {
        send(message.id, { result: { sessions: [] } });
        return;
    }
    if (message.method === "session/new") {
        send(message.id, { result: { sessionId: "stable-session-" + message.id, configOptions: [] } });
        return;
    }
    if (message.method === "session/delete" || message.method === "session/prompt") {
        send(message.id, { result: {} });
        return;
    }
    send(message.id, { result: null });
});
`;

test("settings-triggered reload failure keeps the previous runtime usable and disconnects the failed adapter", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-index-reload-"));
    const workspaceDir = join(rootDir, "agent-workspace");
    const fakeAgentPath = join(rootDir, "fake-agent.mjs");
    const logPath = join(rootDir, "fake-agent.log");
    const port = await getFreePort();

    await mkdir(join(rootDir, "prompts"), { recursive: true });
    await writeFile(fakeAgentPath, fakeAgentSource, "utf8");
    await writeJson(join(rootDir, "agent-servers.json"), {
        activeAgent: "Stable",
        agentServers: [stableAgent(fakeAgentPath)],
    });
    await writeJson(join(rootDir, "acp-session-conf.json"), sessionConfig());

    const server = spawn(process.execPath, [serverEntry, "--path", rootDir, "--port", String(port)], {
        cwd: rootDir,
        env: {
            ...process.env,
            ACP_CWD: workspaceDir,
            FAKE_AGENT_LOG: logPath,
        },
        stdio: ["ignore", "pipe", "pipe"],
    });
    let serverOutput = "";
    server.stdout.setEncoding("utf8");
    server.stderr.setEncoding("utf8");
    server.stdout.on("data", (chunk) => { serverOutput += chunk; });
    server.stderr.on("data", (chunk) => { serverOutput += chunk; });
    t.after(() => stopProcess(server));

    const initialState = await waitFor(async () => {
        const response = await requestJson(port, "/api/state");
        return response.status === 200 && response.body.initialized ? response.body : undefined;
    }, { timeoutMs: 5000, errorMessage: () => `server did not initialize\n${serverOutput}` });
    assert.equal(initialState.activeAgentName, "Stable");

    const initialObservation = await requestJson(port, "/api/page/observation");
    assert.deepEqual(initialObservation, { status: 200, body: { observation: null, updatedAt: null } });
    const updatedObservation = await requestJson(port, "/api/page/observation", {
        method: "POST",
        body: { observation: { route: "timeline" } },
    });
    assert.equal(updatedObservation.status, 200);
    assert.deepEqual(updatedObservation.body.observation, { route: "timeline" });
    assert.deepEqual((await requestJson(port, "/api/page/observation")).body.observation, { route: "timeline" });

    const reloadResult = await requestJson(port, "/api/agent-config", {
        method: "PUT",
        body: {
            activeAgentName: "Broken",
            agentServers: [stableAgent(fakeAgentPath), brokenAgent(fakeAgentPath)],
            sessionConfig: sessionConfig(),
        },
    });

    assert.equal(reloadResult.status, 500);
    assert.equal(reloadResult.body.error, "reload_failed");
    assert.equal(reloadResult.body.saved, true);
    assert.equal(JSON.parse(await readFile(join(rootDir, "agent-servers.json"), "utf8")).activeAgent, "Broken");

    const failedDisconnected = await waitFor(async () => {
        const log = await readText(logPath);
        return /signal:fail:SIGTERM|stdin_end:fail/.test(log) ? true : undefined;
    }, { timeoutMs: 500, throwOnTimeout: false });
    const stateAfterFailure = (await requestJson(port, "/api/state")).body;

    assert.deepEqual({
        initialized: stateAfterFailure.initialized,
        activeAgentName: stateAfterFailure.activeAgentName,
        agentInfoName: stateAfterFailure.agentInfo?.name,
        agentServerNames: stateAfterFailure.agentServers.map(({ name }) => name),
        availableCommands: stateAfterFailure.availableCommands,
        failedDisconnected,
    }, {
        initialized: true,
        activeAgentName: "Stable",
        agentInfoName: "Stable",
        agentServerNames: ["Stable"],
        availableCommands: [],
        failedDisconnected: true,
    });
});

test("failed settings reload preserves old-runtime notifications and never broadcasts the broken candidate", async (t) => {
    const rootDir = await mkdtemp(join(tmpdir(), "insight-index-reload-staged-"));
    const workspaceDir = join(rootDir, "agent-workspace");
    const fakeAgentPath = join(rootDir, "fake-agent.mjs");
    const logPath = join(rootDir, "fake-agent.log");
    const notifyFlagPath = join(rootDir, "notify-old-runtime");
    const port = await getFreePort();

    await mkdir(join(rootDir, "prompts"), { recursive: true });
    await writeFile(fakeAgentPath, fakeAgentSource, "utf8");
    await writeJson(join(rootDir, "agent-servers.json"), {
        activeAgent: "Stable",
        agentServers: [stableAgent(fakeAgentPath)],
    });
    await writeJson(join(rootDir, "acp-session-conf.json"), sessionConfig());

    const server = spawn(process.execPath, [serverEntry, "--path", rootDir, "--port", String(port)], {
        cwd: rootDir,
        env: {
            ...process.env,
            ACP_CWD: workspaceDir,
            FAKE_AGENT_LOG: logPath,
            FAKE_AGENT_NOTIFY_FLAG: notifyFlagPath,
            FAKE_AGENT_OLD_COMMAND: "stable-live",
            BROKEN_INIT_DELAY_MS: "200",
        },
        stdio: ["ignore", "pipe", "pipe"],
    });
    let serverOutput = "";
    server.stdout.setEncoding("utf8");
    server.stderr.setEncoding("utf8");
    server.stdout.on("data", (chunk) => { serverOutput += chunk; });
    server.stderr.on("data", (chunk) => { serverOutput += chunk; });
    t.after(() => stopProcess(server));

    await waitFor(async () => {
        const response = await requestJson(port, "/api/state");
        return response.status === 200 && response.body.initialized ? response.body : undefined;
    }, { timeoutMs: 5000, errorMessage: () => `server did not initialize\n${serverOutput}` });

    const recorder = await recordStateEvents(port);
    t.after(() => recorder.stop());
    await recorder.ready;

    const reloadPromise = requestJson(port, "/api/agent-config", {
        method: "PUT",
        body: {
            activeAgentName: "Broken",
            agentServers: [stableAgent(fakeAgentPath), brokenAgent(fakeAgentPath)],
            sessionConfig: sessionConfig(),
        },
    });
    await waitFor(async () => {
        const log = await readText(logPath);
        return log.includes("initialize:fail") ? true : undefined;
    }, { timeoutMs: 1000, errorMessage: () => `failed runtime was not initialized\n${serverOutput}` });
    await writeFile(notifyFlagPath, "notify", "utf8");

    const reloadResult = await reloadPromise;
    assert.equal(reloadResult.status, 500);
    assert.equal(reloadResult.body.error, "reload_failed");

    const stateAfterFailure = (await requestJson(port, "/api/state")).body;
    assert.deepEqual(stateAfterFailure.availableCommands, [{ name: "stable-live", description: "old runtime command" }]);
    assert.equal(recorder.states.some((eventState) => eventState.activeAgentName === "Broken" || eventState.initialized === false), false);
});

const stableAgent = (fakeAgentPath) => ({
    name: "Stable",
    command: process.execPath,
    args: [fakeAgentPath, "ok"],
    env: {},
});

const brokenAgent = (fakeAgentPath) => ({
    name: "Broken",
    command: process.execPath,
    args: [fakeAgentPath, "fail"],
    env: {},
});

const sessionConfig = () => ({
    requestTimeoutMs: 1000,
    promptRequestTimeoutMs: 1000,
    permissionRequestTimeoutMs: 1000,
    defaultAllowlist: {
        includeDocsRoot: true,
        includeAgentWorkspaceRoot: true,
        includeProjectRoot: true,
        extraPaths: [],
    },
});

const writeJson = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");

const readText = async (path) => {
    try {
        return await readFile(path, "utf8");
    } catch (error) {
        if (error.code === "ENOENT") return "";
        throw error;
    }
};

const requestJson = async (port, path, { method = "GET", body } = {}) => {
    const response = await fetch(`http://127.0.0.1:${port}${path}`, {
        method,
        headers: body === undefined ? undefined : { "content-type": "application/json" },
        body: body === undefined ? undefined : JSON.stringify(body),
    });
    return { status: response.status, body: await response.json() };
};

const recordStateEvents = async (port) => {
    const controller = new AbortController();
    const states = [];
    let resolveReady;
    const ready = new Promise((resolve) => { resolveReady = resolve; });
    const response = await fetch(`http://127.0.0.1:${port}/api/events`, { signal: controller.signal });
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";
    let pumpError;
    const pump = (async () => {
        try {
            while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                buffer += decoder.decode(value, { stream: true });
                for (let index = buffer.indexOf("\n\n"); index !== -1; index = buffer.indexOf("\n\n")) {
                    const rawEvent = buffer.slice(0, index);
                    buffer = buffer.slice(index + 2);
                    for (const line of rawEvent.split("\n")) {
                        if (!line.startsWith("data: ")) continue;
                        const event = JSON.parse(line.slice("data: ".length));
                        if (event.type !== "state") continue;
                        states.push(event.state);
                        resolveReady();
                    }
                }
            }
        } catch (error) {
            if (!isClosedSseError(error)) pumpError = error;
        }
    })();
    return {
        ready,
        states,
        async stop() {
            controller.abort();
            await pump;
            if (pumpError) throw pumpError;
        },
    };
};

const isClosedSseError = (error) => error.name === "AbortError" || error.message === "terminated";

const waitFor = async (probe, { timeoutMs = 1000, intervalMs = 25, throwOnTimeout = true, errorMessage } = {}) => {
    const deadline = Date.now() + timeoutMs;
    let lastError;
    while (Date.now() < deadline) {
        try {
            const result = await probe();
            if (result) return result;
        } catch (error) {
            lastError = error;
        }
        await sleep(intervalMs);
    }
    if (!throwOnTimeout) return false;
    throw lastError ?? new Error(typeof errorMessage === "function" ? errorMessage() : errorMessage ?? "timed out waiting for condition");
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const getFreePort = () => new Promise((resolve, reject) => {
    const server = createServer();
    server.on("error", reject);
    server.listen(0, "127.0.0.1", () => {
        const { port } = server.address();
        server.close(() => resolve(port));
    });
});

const stopProcess = async (child) => {
    if (child.exitCode !== null || child.signalCode !== null) return;
    child.kill("SIGTERM");
    const killTimer = setTimeout(() => child.kill("SIGKILL"), 1000);
    try {
        await once(child, "exit");
    } finally {
        clearTimeout(killTimer);
    }
};

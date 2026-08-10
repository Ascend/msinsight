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

const METHOD_NOT_FOUND = { code: -32601, message: "method_not_found in M0" };
const DEFAULT_HOST_METHODS = [
    "session/request_permission",
    "fs/read_text_file",
    "fs/write_text_file",
    "terminal/create",
    "terminal/output",
    "terminal/release",
    "terminal/wait_for_exit",
    "terminal/kill",
];

const DEFAULT_REQUEST_TIMEOUT_MS = 30000;
const DEFAULT_PROMPT_TIMEOUT_MS = 5 * 60 * 1000;

export const createAcpAdapter = ({
    agentServer,
    cwd,
    debug,
    spawnProcess = spawn,
    requestTimeoutMs = DEFAULT_REQUEST_TIMEOUT_MS,
    promptRequestTimeoutMs = DEFAULT_PROMPT_TIMEOUT_MS,
}) => {
    const { command, args } = resolveCommand(agentServer);
    const subscribers = new Set();
    const pending = new Map();
    const pendingHostRequests = new Map();
    const hostHandlers = new Map();
    let child;
    let disconnecting = false;
    let buffer = "";
    let nextId = 1;

    hostHandlers.set("ping", () => ({ result: { pong: true } }));
    for (const method of DEFAULT_HOST_METHODS) {
        hostHandlers.set(method, () => ({ error: METHOD_NOT_FOUND }));
    }

    const notifySubscribers = (message) => {
        for (const subscriber of subscribers) subscriber(message);
    };

    const connect = () => {
        if (child) return;
        buffer = "";
        child = spawnProcess(command, args, {
            cwd,
            env: { ...process.env, ...(agentServer.env ?? {}) },
            stdio: ["pipe", "pipe", "pipe"],
            windowsHide: process.platform === "win32",
            detached: process.platform !== "win32",
        });

        child.stderr?.setEncoding?.("utf8");
        child.stderr?.on?.("data", (chunk) => process.stderr.write(chunk));
        child.on?.("error", (error) => {
            notifySubscribers({ kind: "transport_error", error });
            rejectPending(pending, error);
            rejectPending(pendingHostRequests, error);
        });
        child.on?.("exit", (code, signal) => {
            if (disconnecting) return;
            const error = new Error(`ACP server exited code=${code} signal=${signal}`);
            notifySubscribers({ kind: "transport_error", error });
            rejectPending(pending, error);
            rejectPending(pendingHostRequests, error);
            child = undefined;
        });

        child.stdout?.setEncoding?.("utf8");
        child.stdout?.on?.("data", (chunk) => {
            buffer += chunk;
            while (true) {
                const newline = buffer.indexOf("\n");
                if (newline === -1) break;
                const line = buffer.slice(0, newline).trim();
                buffer = buffer.slice(newline + 1);
                if (line) handleAcpLine({ child, debug, line, hostHandlers, notifySubscribers, pending });
            }
        });
    };

    const send = (message) => {
        if (!child) connect();
        writeJson(child, message);
    };

    return {
        runtime: "stdio",
        agentId: agentServer.name,
        request(method, params) {
            const id = nextId++;
            send({ jsonrpc: "2.0", id, method, params });
            return new Promise((resolve, reject) => {
                const timeoutMs = method === "session/prompt" ? promptRequestTimeoutMs : requestTimeoutMs;
                const timeout = setTimeout(() => {
                    pending.delete(id);
                    reject(new Error(`ACP request timed out: ${method}`));
                }, timeoutMs);
                pending.set(id, { resolve, reject, method, timeout });
            });
        },
        notify(method, params) {
            send({ jsonrpc: "2.0", method, params });
        },
        registerHandler(method, handler) {
            hostHandlers.set(method, handler);
        },
        unregisterHandler(method) {
            hostHandlers.delete(method);
        },
        connect,
        async disconnect() {
            const current = child;
            child = undefined;
            disconnecting = true;
            if (current) {
                await new Promise((resolve) => {
                    const timeout = setTimeout(() => {
                        terminateProcessTree(current, "SIGKILL");
                        resolve();
                    }, 1000);
                    current.once?.("exit", () => {
                        clearTimeout(timeout);
                        resolve();
                    });
                    terminateProcessTree(current, "SIGTERM");
                });
            }
            rejectPending(pending, new Error("ACP adapter disconnected"));
            rejectPending(pendingHostRequests, new Error("ACP adapter disconnected"));
            buffer = "";
            disconnecting = false;
        },
        send,
        onMessage(handler) {
            subscribers.add(handler);
            return () => subscribers.delete(handler);
        },
    };
};

const terminateProcessTree = (child, signal) => {
    if (!child?.pid) return;
    try {
        if (process.platform === "win32") {
            spawn("taskkill", ["/pid", String(child.pid), "/t", "/f"], {
                stdio: "ignore",
                windowsHide: true,
            }).unref();
            return;
        }
        if (process.platform !== "win32") {
            process.kill(-child.pid, signal);
            return;
        }
    } catch (error) {
        if (error.code !== "ESRCH") child.kill?.(signal);
    }
};

const handleAcpLine = async ({ child, debug, line, hostHandlers, notifySubscribers, pending }) => {
    let message;
    try {
        message = JSON.parse(line);
    } catch (_error) {
        if (debug) console.error(`Ignoring non-ACP stdout: ${line}`);
        return;
    }

    if (message.method && message.id === undefined) {
        if (debug) console.error("ACP notification", JSON.stringify(message));
        notifySubscribers(message);
        return;
    }

    if (message.method && message.id !== undefined) {
        const handler = hostHandlers.get(message.method);
        if (!handler) {
            writeJson(child, { jsonrpc: "2.0", id: message.id, error: METHOD_NOT_FOUND });
            return;
        }
        try {
            const response = await handler(message.params);
            if (response?.error) {
                writeJson(child, { jsonrpc: "2.0", id: message.id, error: response.error });
            } else if (response && Object.hasOwn(response, "result")) {
                writeJson(child, { jsonrpc: "2.0", id: message.id, result: response.result });
            } else {
                writeJson(child, { jsonrpc: "2.0", id: message.id, result: response ?? null });
            }
        } catch (error) {
            writeJson(child, { jsonrpc: "2.0", id: message.id, error: { code: -32603, message: error.message } });
        }
        return;
    }

    const waiter = pending.get(message.id);
    if (!waiter) return;
    pending.delete(message.id);
    clearTimeout(waiter.timeout);

    if (message.error) {
        const errorDetail = message.error.data?.details ?? message.error.message ?? JSON.stringify(message.error);
        waiter.reject(new Error(errorDetail));
    } else {
        waiter.resolve(message.result ?? null);
    }
};

const writeJson = (child, value) => {
    child.stdin.write(`${JSON.stringify(value)}\n`);
};

const rejectPending = (pendingRequests, error) => {
    for (const waiter of pendingRequests.values()) {
        clearTimeout(waiter.timeout);
        waiter.reject(error);
    }
    pendingRequests.clear();
};

const resolveCommand = (agentServer) => {
    if (process.platform === "win32") {
        return { command: "cmd.exe", args: ["/c", agentServer.command, ...agentServer.args] };
    }
    return { command: agentServer.command, args: agentServer.args };
};

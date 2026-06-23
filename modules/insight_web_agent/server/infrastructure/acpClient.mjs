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

export const createAcpClient = ({ agentServer, cwd, debug, onNotification }) => {
    const { command, args } = resolveCommand(agentServer);
    const child = spawn(command, args, {
        cwd,
        env: { ...process.env, ...(agentServer.env ?? {}) },
        stdio: ["pipe", "pipe", "pipe"],
        windowsHide: process.platform === "win32",
    });

    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk) => process.stderr.write(chunk));
    child.on("error", (error) => {
        console.error(`Failed to start ACP server: ${error.message}`);
        rejectPending(pending, error);
    });
    child.on("exit", (code, signal) => {
        console.error(`ACP server exited code=${code} signal=${signal}`);
        rejectPending(pending, new Error(`ACP server exited code=${code} signal=${signal}`));
    });

    let nextId = 1;
    const pending = new Map();
    let buffer = "";

    child.stdout.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
        buffer += chunk;
        while (true) {
            const newline = buffer.indexOf("\n");
            if (newline === -1) break;
            const line = buffer.slice(0, newline).trim();
            buffer = buffer.slice(newline + 1);
            if (line) handleAcpLine({ child, debug, line, onNotification, pending });
        }
    });

    return {
        request(method, params) {
            const id = nextId++;
            writeJson(child, { jsonrpc: "2.0", id, method, params });
            return new Promise((resolve, reject) => {
                const timeout = setTimeout(() => {
                    pending.delete(id);
                    reject(new Error(`ACP request timed out: ${method}`));
                }, 30000);
                pending.set(id, { resolve, reject, method, timeout });
            });
        },
        dispose() {
            child.kill();
            for (const waiter of pending.values()) {
                clearTimeout(waiter.timeout);
                waiter.reject(new Error("ACP client disposed"));
            }
            pending.clear();
        },
    };
};

const handleAcpLine = ({ child, debug, line, onNotification, pending }) => {
    let message;
    try {
        message = JSON.parse(line);
    } catch (_error) {
        if (debug) console.error(`Ignoring non-ACP stdout: ${line}`);
        return;
    }

    if (message.method) {
        if (debug) console.error("ACP notification", JSON.stringify(message));
        onNotification(message);
        if (message.id !== undefined) {
            writeJson(child, { jsonrpc: "2.0", id: message.id, result: null });
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

const rejectPending = (pending, error) => {
    for (const waiter of pending.values()) {
        clearTimeout(waiter.timeout);
        waiter.reject(error);
    }
    pending.clear();
};

const resolveCommand = (agentServer) => {
    if (process.platform === "win32") {
        return { command: "cmd.exe", args: ["/c", agentServer.command, ...agentServer.args] };
    }
    return { command: agentServer.command, args: agentServer.args };
};

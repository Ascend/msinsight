/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { spawn } from "node:child_process";
import { evaluateBashPolicy, normalizeBashInput } from "../permissions/bashPolicy.mjs";

const MAX_OUTPUT_BYTES = 200 * 1024;

export const createBashTools = ({
    sessions,
    hostClient,
    cwd = process.cwd(),
    env = process.env,
    shell = env.MSINSIGHT_NATIVE_BASH_PATH ?? "bash",
    spawnProcess = spawn,
}) => {
    const active = new Map();

    return [{
        name: "Bash",
        description: "Run one foreground, non-interactive Bash command inside an allowed filesystem root. Commands are subject to product policy and may require user approval.",
        inputSchema: {
            type: "object",
            properties: {
                command: { type: "string", minLength: 1 },
                cwd: { type: "string", description: "Working directory. Relative paths resolve from the Native Agent workspace; omit to use that workspace." },
                timeout: { type: "number", minimum: 1000, maximum: 300000 },
            },
            required: ["command"],
            additionalProperties: false,
        },
        async execute(input, { sessionId, signal } = {}) {
            const session = sessions.get(String(sessionId ?? ""));
            if (!session) throw new Error(`Bash session is unavailable: ${sessionId}`);
            if (active.has(session.sessionId)) throw new Error("Another Bash command is already running in this session");
            const reservation = Symbol(session.sessionId);
            active.set(session.sessionId, reservation);
            try {
                const normalized = await authorizeBash({ input, session, signal, cwd, hostClient });
                if (signal?.aborted) throw signal.reason ?? new Error("Bash command was cancelled");
                return await runBashCommand({ input: normalized, signal, shell, spawnProcess, env: createBashEnvironment(env) });
            } finally {
                if (active.get(session.sessionId) === reservation) active.delete(session.sessionId);
            }
        },
    }];
};

const authorizeBash = async ({ input, session, signal, cwd, hostClient }) => {
    const normalized = await normalizeBashInput(input, session, cwd);
    const policy = evaluateBashPolicy({ command: normalized.command, rules: session.primaryAgentBashRules });
    if (policy.behavior === "deny") throw new Error(policy.message);
    if (policy.behavior === "allow") return normalized;
    const result = await hostClient.request("session/request_permission", {
        sessionId: session.sessionId,
        kind: "bash",
        title: "Run Bash command",
        target: normalized.command,
        rememberKey: `bash:${session.primaryAgentId}:${policy.normalizedRule}`,
        details: {
            cwd: normalized.cwd,
            primaryAgentId: session.primaryAgentId,
            commandRule: policy.normalizedRule,
        },
        options: [
            { optionId: "allow_once", kind: "allow_once", name: "Allow once" },
            { optionId: "allow_always", kind: "allow_always", name: "Allow for this session" },
            { optionId: "deny", kind: "reject_once", name: "Deny" },
        ],
    }, { signal });
    const optionId = result?.outcome?.optionId;
    if (optionId !== "allow_once" && optionId !== "allow_always") throw new Error("Bash command was denied by the user");
    return normalized;
};

const runBashCommand = ({ input, signal, shell, spawnProcess, env }) => new Promise((resolve, reject) => {
    const child = spawnProcess(shell, ["-lc", input.command], {
        cwd: input.cwd,
        env,
        stdio: ["ignore", "pipe", "pipe"],
        windowsHide: true,
        detached: process.platform !== "win32",
    });
    let stdout = "";
    let stderr = "";
    let outputBytes = 0;
    let terminationError;
    let settled = false;

    const cleanup = () => {
        clearTimeout(timeout);
        signal?.removeEventListener("abort", abort);
    };
    const finish = (callback, value) => {
        if (settled) return;
        settled = true;
        cleanup();
        callback(value);
    };
    const terminate = (error) => {
        if (terminationError) return;
        terminationError = error;
        terminateProcessTree(child, spawnProcess);
    };
    const append = (channel, chunk) => {
        const data = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        const remaining = Math.max(0, MAX_OUTPUT_BYTES - outputBytes);
        if (remaining) {
            const text = data.subarray(0, remaining).toString("utf8");
            if (channel === "stdout") stdout += text;
            else stderr += text;
        }
        outputBytes += data.length;
        if (outputBytes > MAX_OUTPUT_BYTES) terminate(new Error(`Bash output exceeded ${MAX_OUTPUT_BYTES} bytes`));
    };
    const abort = () => terminate(signal?.reason instanceof Error ? signal.reason : new Error("Bash command was cancelled"));
    const timeout = setTimeout(() => terminate(new Error(`Bash command timed out after ${input.timeout} milliseconds`)), input.timeout);

    child.stdout?.on("data", (chunk) => append("stdout", chunk));
    child.stderr?.on("data", (chunk) => append("stderr", chunk));
    child.once("error", (error) => finish(reject, new Error(`Failed to start Bash: ${error.message}`)));
    child.once("close", (code, childSignal) => {
        if (terminationError) {
            terminationError.stdout = stdout;
            terminationError.stderr = stderr;
            finish(reject, terminationError);
            return;
        }
        finish(resolve, { exitCode: code, signal: childSignal ?? undefined, stdout, stderr });
    });
    signal?.addEventListener("abort", abort, { once: true });
    if (signal?.aborted) abort();
});

const createBashEnvironment = (env) => Object.fromEntries(Object.entries(env)
    .filter(([key, value]) => value !== undefined && !/(?:API_KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL)/i.test(key))
    .map(([key, value]) => [key, String(value)]));

const terminateProcessTree = (child, spawnProcess) => {
    if (!child.pid) {
        child.kill?.();
        return;
    }
    if (process.platform === "win32") {
        const killer = spawnProcess("taskkill", ["/pid", String(child.pid), "/t", "/f"], {
            stdio: "ignore",
            windowsHide: true,
        });
        killer.once?.("error", () => child.kill?.());
        return;
    }
    try {
        process.kill(-child.pid, "SIGKILL");
    } catch (_error) {
        child.kill?.("SIGKILL");
    }
};

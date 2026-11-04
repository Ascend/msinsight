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
import { resolveShellRuntime } from "./shellRuntime.mjs";
import { randomUUID } from "node:crypto";
import { buildProcessDetails, createToolError, isToolDiagnosticId, normalizeToolError } from "../../utils/toolError.mjs";
import { readDiagnosticProperty as read } from "../../utils/toolErrorBudget.mjs";

const MAX_OUTPUT_BYTES = 200 * 1024;

export const createBashTools = ({
    sessions,
    hostClient,
    env = process.env,
    cwd = process.cwd(),
    shellRuntime = resolveShellRuntime({ env }),
    spawnProcess = spawn,
}) => {
    const active = new Map();

    return [{
        name: "Bash",
        description: shellRuntime.description,
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
        async execute(input, { sessionId, signal, diagnosticId } = {}) {
            const diagnostic = isToolDiagnosticId(diagnosticId) ? diagnosticId : randomUUID();
            const failure = (error, stage, outcome = "not_started") => createToolError(normalizeToolError({
                code: read(error, "code"), message: typeof error === "string" ? error : read(error, "message"), cause: error,
                details: buildProcessDetails(error, { diagnosticId: diagnostic }),
                retryable: read(error, "code") === "BASH_CONCURRENT",
            }, { diagnosticId: diagnostic, toolName: "Bash", source: "bash", stage, outcome,
                retryAdvice: read(error, "code") === "BASH_CONCURRENT" ? "retry_after_ready"
                    : ["BASH_CANCELLED", "BASH_DENIED_BY_USER", "BASH_POLICY_DENIED"].includes(read(error, "code")) ? "do_not_retry"
                        : outcome === "unknown" ? "inspect_before_retry" : "correct_input" }), error);
            const session = sessions.get(String(sessionId ?? ""));
            if (!session) throw failure(bashError("BASH_SESSION_UNAVAILABLE", "Bash session is unavailable."), "validate");
            if (signal?.aborted) throw failure(bashError("BASH_CANCELLED", "Bash command was cancelled."), "dispatch");
            if (active.has(session.sessionId)) throw failure(bashError("BASH_CONCURRENT", "Another Bash command is already running in this session"), "dispatch");
            const reservation = Symbol(session.sessionId);
            active.set(session.sessionId, reservation);
            let started = false;
            try {
                const normalized = await authorizeBash({ input, session, signal, cwd, hostClient, shellRuntime });
                if (signal?.aborted) throw bashError("BASH_CANCELLED", `${shellRuntime.displayName} command was cancelled`);
                started = true;
                return await runShellCommand({
                    input: normalized,
                    signal,
                    shellRuntime,
                    spawnProcess,
                    env: createBashEnvironment(env),
                });
            } catch (error) {
                const code = read(error, "code");
                const cancelled = signal?.aborted && !(typeof code === "string" && code.startsWith("BASH_"));
                const value = cancelled ? bashError("BASH_CANCELLED", "Bash command was cancelled.", error) : error;
                const valueCode = read(value, "code");
                const stage = valueCode === "BASH_INPUT_INVALID" ? "validate" : started ? "execute" : "authorize";
                const outcome = !started || valueCode === "BASH_START_FAILED" ? "not_started"
                    : ["BASH_CANCELLED", "BASH_TIMEOUT"].includes(valueCode) ? "unknown" : "failed";
                throw failure(value, stage, outcome);
            } finally {
                if (active.get(session.sessionId) === reservation) active.delete(session.sessionId);
            }
        },
    }];
};

const authorizeBash = async ({ input, session, signal, cwd, hostClient, shellRuntime }) => {
    const normalized = await normalizeBashInput(input, session, cwd);
    const policy = evaluateBashPolicy({
        command: normalized.command,
        rules: session.primaryAgentBashRules,
        shellKind: shellRuntime.kind,
    });
    if (policy.behavior === "deny") throw bashError("BASH_POLICY_DENIED", policy.message);
    // Full Access skips interactive approval, never explicit product/Agent denies.
    if (policy.behavior === "allow" || (session.permissionModeDuringSave ?? session.permissionMode) === "full-access") return normalized;
    const result = await hostClient.request("session/request_permission", {
        sessionId: session.sessionId,
        kind: "bash",
        _meta: { msinsightNativePermission: true },
        title: `Run ${shellRuntime.displayName} command`,
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
    if (optionId !== "allow_once" && optionId !== "allow_always") throw bashError("BASH_DENIED_BY_USER", "Bash command was denied by the user");
    return normalized;
};

const runShellCommand = ({ input, signal, shellRuntime, spawnProcess, env }) => new Promise((resolve, reject) => {
    let child;
    try { child = spawnProcess(shellRuntime.executable, [
        ...shellRuntime.commandArgs,
        shellRuntime.prepareCommand(input.command),
    ], {
        cwd: input.cwd,
        env,
        stdio: ["ignore", "pipe", "pipe"],
        windowsHide: true,
        detached: process.platform !== "win32",
    }); } catch (error) {
        reject(bashError("BASH_START_FAILED", `Failed to start ${shellRuntime.displayName}.`, error));
        return;
    }
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
        if (outputBytes > MAX_OUTPUT_BYTES) terminate(Object.assign(bashError("BASH_OUTPUT_LIMIT", `${shellRuntime.displayName} output exceeded ${MAX_OUTPUT_BYTES} bytes`), { captureLimited: true, limitBytes: MAX_OUTPUT_BYTES }));
    };
    const abort = () => terminate(bashError("BASH_CANCELLED", `${shellRuntime.displayName} command was cancelled`, signal?.reason));
    const timeout = setTimeout(() => terminate(Object.assign(bashError("BASH_TIMEOUT", `${shellRuntime.displayName} command timed out after ${input.timeout} milliseconds`), { timeoutMs: input.timeout })), input.timeout);

    child.stdout?.on("data", (chunk) => append("stdout", chunk));
    child.stderr?.on("data", (chunk) => append("stderr", chunk));
    child.once("error", (error) => finish(reject, bashError("BASH_START_FAILED", `Failed to start ${shellRuntime.displayName}.`, error)));
    child.once("close", (code, childSignal) => {
        if (terminationError) {
            terminationError.stdout = stdout;
            terminationError.stderr = stderr;
            terminationError.exitCode = code;
            terminationError.signal = childSignal;
            finish(reject, terminationError);
            return;
        }
        finish(resolve, { exitCode: code, signal: childSignal ?? undefined, stdout, stderr });
    });
    signal?.addEventListener("abort", abort, { once: true });
    if (signal?.aborted) abort();
});

const bashError = (code, message, cause) => Object.assign(new Error(message, { cause }), { code });

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

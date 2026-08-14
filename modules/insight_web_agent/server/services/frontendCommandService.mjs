/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";

const DEFAULT_TIMEOUT_MS = 30000;
const MAX_TIMEOUT_MS = 60000;

export const createFrontendCommandService = ({ eventBus, timeoutMs = DEFAULT_TIMEOUT_MS } = {}) => {
    const pending = new Map();
    const toRequestEvent = (item) => ({
        type: "frontend_command_request",
        requestId: item.requestId,
        sessionId: item.sessionId,
        command: item.command,
        args: item.args,
        deadline: item.deadline,
    });
    const stopReplay = eventBus.onConnect?.((send) => {
        for (const item of pending.values()) {
            if (item.state === "pending" && Date.now() < item.deadline) send(toRequestEvent(item));
        }
    });

    const request = ({ requestId = randomUUID(), sessionId, command, args, deadline, signal } = {}) => {
        const normalizedRequestId = String(requestId ?? "").trim();
        const normalizedCommand = String(command ?? "").trim();
        if (!normalizedRequestId) return Promise.reject(commandError("COMMAND_INVALID", "requestId is required.", false));
        if (!normalizedCommand) return Promise.reject(commandError("COMMAND_INVALID", "command is required.", false));
        if (!args || typeof args !== "object" || Array.isArray(args)) return Promise.reject(commandError("COMMAND_INVALID", "args must be an object.", false));
        if (pending.has(normalizedRequestId)) return Promise.reject(commandError("COMMAND_INVALID", `Request '${normalizedRequestId}' is already pending.`, false));
        const finalDeadline = normalizeDeadline(deadline, timeoutMs);
        if (signal?.aborted) return Promise.reject(abortError(signal.reason));
        return new Promise((resolve, reject) => {
            const settle = (handler, value) => {
                const item = pending.get(normalizedRequestId);
                if (!item || item.state === "settled") return;
                item.state = "settled";
                clearTimeout(item.timeout);
                item.signal?.removeEventListener("abort", item.abort);
                pending.delete(normalizedRequestId);
                handler(value);
            };
            const timeout = setTimeout(() => {
                eventBus.broadcast({ type: "frontend_command_cancel", requestId: normalizedRequestId, reason: "deadline_exceeded" });
                settle(reject, commandError("COMMAND_TIMEOUT", "The frontend command exceeded its deadline.", true));
            }, Math.max(0, finalDeadline - Date.now()));
            const abort = () => {
                eventBus.broadcast({ type: "frontend_command_cancel", requestId: normalizedRequestId, reason: "cancelled" });
                settle(reject, abortError(signal?.reason));
            };
            pending.set(normalizedRequestId, {
                requestId: normalizedRequestId,
                sessionId: String(sessionId ?? ""),
                state: "pending",
                resolve: (value) => settle(resolve, value),
                reject: (error) => settle(reject, error),
                timeout,
                deadline: finalDeadline,
                command: normalizedCommand,
                args,
                signal,
                abort,
            });
            signal?.addEventListener("abort", abort, { once: true });
            eventBus.broadcast(toRequestEvent(pending.get(normalizedRequestId)));
        });
    };

    const claim = (requestId) => {
        const item = pending.get(String(requestId ?? ""));
        if (!item || item.state !== "pending" || Date.now() >= item.deadline) return { ok: true, claimed: false };
        item.state = "claimed";
        item.claimToken = randomUUID();
        return { ok: true, claimed: true, claimToken: item.claimToken };
    };

    const respond = ({ requestId, claimToken, status, result, error } = {}) => {
        const item = pending.get(String(requestId ?? ""));
        if (!item) return { ok: true, settled: true };
        if (item.state !== "claimed" || !claimToken || claimToken !== item.claimToken) {
            return { ok: true, settled: false };
        }
        if (status === "completed") item.resolve(result);
        else item.reject(toCommandError(error, status));
        return { ok: true, settled: true };
    };

    const cancel = (requestId, reason = "cancelled") => {
        const item = pending.get(String(requestId ?? ""));
        if (!item) return false;
        eventBus.broadcast({ type: "frontend_command_cancel", requestId: item.requestId, reason });
        item.reject(commandError("COMMAND_CANCELLED", "The frontend command was cancelled.", true));
        return true;
    };

    const cancelSession = (sessionId, reason = "session_cancelled") => {
        for (const item of [...pending.values()]) {
            if (item.sessionId === String(sessionId ?? "")) cancel(item.requestId, reason);
        }
    };

    const cancelAll = (reason = "cancelled") => {
        for (const item of [...pending.values()]) cancel(item.requestId, reason);
    };

    const dispose = () => {
        stopReplay?.();
        cancelAll("service_disposed");
    };

    return { request, claim, respond, cancel, cancelSession, cancelAll, dispose };
};

const normalizeDeadline = (deadline, timeoutMs) => {
    const now = Date.now();
    const requested = Number(deadline);
    const configured = Math.min(Math.max(Number(timeoutMs) || DEFAULT_TIMEOUT_MS, 1000), MAX_TIMEOUT_MS);
    if (!Number.isFinite(requested)) return now + configured;
    return Math.min(requested, now + MAX_TIMEOUT_MS);
};

const toCommandError = (error, status) => {
    if (error && typeof error === "object") return Object.assign(new Error(String(error.message ?? status ?? "Frontend command failed")), error);
    return commandError("COMMAND_EXECUTION_FAILED", String(error ?? status ?? "Frontend command failed"), false);
};

const abortError = (reason) => commandError("COMMAND_CANCELLED", String(reason?.message ?? reason ?? "The frontend command was cancelled."), true);
const commandError = (code, message, retryable) => Object.assign(new Error(message), { code, retryable });

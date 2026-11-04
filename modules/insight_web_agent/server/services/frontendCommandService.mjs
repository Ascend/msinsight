/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";
import { createToolError, isToolDiagnosticId, normalizeToolError } from "../utils/toolError.mjs";
import { readDiagnosticProperty as read } from "../utils/toolErrorBudget.mjs";

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

    const request = ({ requestId = randomUUID(), diagnosticId: suppliedDiagnosticId, sessionId, command, args, deadline, signal } = {}) => {
        const normalizedRequestId = String(requestId ?? "").trim();
        const normalizedCommand = String(command ?? "").trim();
        if (!normalizedRequestId) return Promise.reject(commandError("COMMAND_INVALID", "requestId is required.", false));
        if (!normalizedCommand) return Promise.reject(commandError("COMMAND_INVALID", "command is required.", false));
        if (!args || typeof args !== "object" || Array.isArray(args)) return Promise.reject(commandError("COMMAND_INVALID", "args must be an object.", false));
        if (pending.has(normalizedRequestId)) return Promise.reject(commandError("COMMAND_INVALID", `Request '${normalizedRequestId}' is already pending.`, false));
        const finalDeadline = normalizeDeadline(deadline, timeoutMs);
        const diagnosticId = isToolDiagnosticId(suppliedDiagnosticId) ? suppliedDiagnosticId
            : isToolDiagnosticId(normalizedRequestId) ? normalizedRequestId : randomUUID();
        if (signal?.aborted) return Promise.reject(cancellationError({ diagnosticId, state: "pending" }, cancellationReason(signal.reason)));
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
                settle(reject, cancellationError(pending.get(normalizedRequestId), "deadline_exceeded"));
            }, Math.max(0, finalDeadline - Date.now()));
            const abort = () => {
                eventBus.broadcast({ type: "frontend_command_cancel", requestId: normalizedRequestId, reason: "cancelled" });
                settle(reject, cancellationError(pending.get(normalizedRequestId), cancellationReason(signal?.reason)));
            };
            pending.set(normalizedRequestId, {
                requestId: normalizedRequestId,
                diagnosticId,
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
        else if (status === "cancelled" || read(error, "code") === "COMMAND_CANCELLED") item.reject(cancellationError(item, "cancelled"));
        else {
            item.reject(createToolError(normalizeToolError(error ?? "Frontend command failed", {
                diagnosticId: item.diagnosticId, source: "command",
                ...receiptFailureContext(error),
            }), error));
        }
        return { ok: true, settled: true };
    };

    const cancel = (requestId, reason = "cancelled") => {
        const item = pending.get(String(requestId ?? ""));
        if (!item) return false;
        const safeReason = cancellationReason(reason);
        eventBus.broadcast({ type: "frontend_command_cancel", requestId: item.requestId, reason: safeReason });
        item.reject(cancellationError(item, safeReason));
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

const cancellationReason = (value) => ["cancelled", "user_cancelled", "session_cancelled", "service_disposed", "deadline_exceeded"].includes(value)
    ? value : "cancelled";

const receiptFailureContext = (error) => {
    const code = read(error, "code");
    if (code === "COMMAND_RESULT_INVALID") {
        return { stage: "respond", outcome: "completed", retryAdvice: "inspect_before_retry" };
    }
    if (["COMMAND_TIMEOUT", "COMMAND_CONNECTION_LOST", "TABLE_COMMAND_TIMEOUT", "TABLE_CONNECTION_LOST"].includes(code)) {
        return { stage: "execute", outcome: "unknown", retryAdvice: "inspect_before_retry" };
    }
    if (["COMMAND_BUSY", "TABLE_BUSY"].includes(code)) {
        return { stage: "validate", outcome: "not_started", retryAdvice: "retry_after_ready" };
    }
    const reason = read(read(error, "details"), "reason");
    if (code === "TABLE_STATE_STALE" || (code === "COMMAND_UNAVAILABLE"
        && ["module_not_active", "module_not_registered", "command_not_registered"].includes(reason))) {
        return { stage: "validate", outcome: "not_started", retryAdvice: "refresh_context" };
    }
    if (["COMMAND_PERMISSION_DENIED", "COMMAND_APPROVAL_REQUIRED"].includes(code)) {
        return { stage: "authorize", outcome: "not_started", retryAdvice: "do_not_retry" };
    }
    if (["COMMAND_INVALID", "COMMAND_NOT_FOUND"].includes(code)) {
        return { stage: "validate", outcome: "not_started", retryAdvice: "correct_input" };
    }
    return { stage: "execute", outcome: "failed", retryAdvice: "inspect_before_retry" };
};

const cancellationError = (item, reason) => {
    const claimed = item?.state === "claimed";
    const timeout = reason === "deadline_exceeded";
    return createToolError(normalizeToolError({
        code: timeout ? "COMMAND_TIMEOUT" : "COMMAND_CANCELLED",
        message: timeout ? "The frontend command exceeded its deadline." : "The frontend command was cancelled.",
        retryable: timeout && !claimed, details: { reason },
    }, {
        diagnosticId: item?.diagnosticId ?? randomUUID(), source: "command",
        stage: claimed ? "execute" : "dispatch", outcome: claimed ? "unknown" : "not_started",
        retryAdvice: claimed ? "inspect_before_retry" : timeout ? "retry_after_ready" : "do_not_retry",
    }));
};
const commandError = (code, message, retryable) => Object.assign(new Error(message), { code, retryable });

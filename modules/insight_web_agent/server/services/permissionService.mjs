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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";
import { createDefaultAllowlist, evaluateReadPolicy, parentAllowlistEntry } from "./permissionPolicy.mjs";
import { errorResult } from "./errorResult.mjs";

export class PermissionDeniedError extends Error {
    constructor(message = "permission denied") {
        super(message);
        this.name = "PermissionDeniedError";
        this.code = "permission_denied";
    }
}

const DEFAULT_TIMEOUT_MS = 5 * 60 * 1000;
const DECISIONS = new Set(["allow_once", "allow_always", "deny"]);
const RESOLVED_STATES = new Set(["allowed_once", "allowed_always", "denied", "expired", "invalidated"]);

export const createPermissionService = ({ state, eventBus, config, timeoutMs } = {}) => {
    let requestTimeoutMs = Number.isFinite(Number(timeoutMs)) && Number(timeoutMs) > 0
        ? Number(timeoutMs)
        : Number.isFinite(Number(config?.permissionRequestTimeoutMs)) && Number(config.permissionRequestTimeoutMs) > 0
            ? Number(config.permissionRequestTimeoutMs)
            : DEFAULT_TIMEOUT_MS;
    // 同一会话只激活队首，排队请求在展示后才开始超时；不同会话互不阻塞。
    const requestQueues = new Map();

    const ensureRuntimeAllowlist = (sessionId) => {
        const key = String(sessionId ?? "").trim();
        let entries = state.permissionRuntimeAllowlist.get(key);
        if (!entries) {
            entries = new Set();
            state.permissionRuntimeAllowlist.set(key, entries);
        }
        return entries;
    };

    const defaultAllowlist = async () => createDefaultAllowlist({
        rootDir: config.rootDir,
        cwd: config.cwd,
        activeAgentName: state.activeAgentName ?? config.activeAgentName ?? config.agentServer?.name,
        activeAgentWorkspaceKey: state.activeAgentWorkspaceKey,
        projectRoot: state.activeContext?.projectRoot,
        includeDocsRoot: config.defaultAllowlist?.includeDocsRoot,
        includeAgentWorkspaceRoot: config.defaultAllowlist?.includeAgentWorkspaceRoot,
        includeProjectRoot: config.defaultAllowlist?.includeProjectRoot,
        extraPaths: config.extraAllowlistPaths,
    });

    const evaluate = async ({ sessionId, path, cwd }) => evaluateReadPolicy({
        targetPath: path,
        cwd,
        defaultAllowlist: await defaultAllowlist(),
        runtimeAllowlist: [...ensureRuntimeAllowlist(sessionId)].filter((entry) => !entry.startsWith("bash:")),
    });

    const isRemembered = (sessionId, rememberKey) => {
        const key = normalizeRememberKey("bash", rememberKey);
        return Boolean(key && ensureRuntimeAllowlist(sessionId).has(key));
    };

    const requestApproval = async ({ sessionId, path, normalizedPath, kind = "filesystem", title, target, details, rememberKey, source = "fs/read_text_file", options = defaultOptions() }) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) throw new Error("sessionId is required");
        const requestId = randomUUID();
        const permissionTarget = target ?? normalizedPath ?? path;
        const request = {
            sessionId: targetSessionId,
            requestId,
            kind,
            title,
            target: permissionTarget,
            path: permissionTarget,
            originalPath: path,
            details,
            rememberKey: normalizeRememberKey(kind, rememberKey),
            source,
            options,
            state: "pending",
            createdAt: Date.now(),
        };

        const promise = new Promise((resolve) => {
            request.resolve = resolve;
        });
        state.pendingPermissions.set(permissionKey(targetSessionId, requestId), request);
        const queue = requestQueues.get(targetSessionId) ?? [];
        queue.push(request);
        requestQueues.set(targetSessionId, queue);
        if (queue.length === 1) activateRequest(request);
        return promise;
    };

    const activateRequest = (request) => {
        request.timeout = setTimeout(() => resolveRequest(request, "expired"), requestTimeoutMs);
        eventBus.broadcast({
            type: "permission_request",
            sessionId: request.sessionId,
            requestId: request.requestId,
            kind: request.kind,
            title: request.title,
            target: request.target,
            path: request.path,
            details: request.details,
            actions: permissionActions(request.options),
        });
    };

    const ensureReadAllowed = async ({ sessionId, path, cwd, source } = {}) => {
        const policy = await evaluate({ sessionId, path, cwd });
        if (policy.action === "allow") return policy;
        const result = await requestApproval({
            sessionId,
            path,
            normalizedPath: policy.normalizedPath,
            source: source ?? "fs/read_text_file",
        });
        if (!result.allowed) throw new PermissionDeniedError(result.reason ?? "permission denied");
        return policy;
    };

    const respond = async ({ sessionId, requestId, decision }) => {
        const targetSessionId = String(sessionId ?? "").trim();
        const targetRequestId = String(requestId ?? "").trim();
        const targetDecision = String(decision ?? "").trim();
        if (!targetSessionId || !targetRequestId || !DECISIONS.has(targetDecision)) {
            return errorResult(
                "invalid_permission_response",
                "sessionId, requestId, and a valid decision are required",
                400,
                { validDecisions: [...DECISIONS] },
            );
        }
        const key = permissionKey(targetSessionId, targetRequestId);
        const request = state.pendingPermissions.get(key) ?? state.resolvedPermissions.get(key);
        if (!request) {
            return errorResult(
                "permission_request_not_found",
                "The permission request does not exist or is no longer available",
                404,
            );
        }
        if (request.state !== "pending") {
            return errorResult(
                "permission_request_resolved",
                `The permission request has already been resolved with state '${request.state}'`,
                409,
                { state: request.state },
            );
        }
        if (!permissionActions(request.options).includes(targetDecision)) {
            return errorResult(
                "permission decision is unavailable",
                "permission decision is unavailable",
                400
            );
        }

        const finalState = targetDecision === "allow_once"
            ? "allowed_once"
            : targetDecision === "allow_always"
                ? "allowed_always"
                : "denied";
        resolveRequest(request, finalState);
        return { ok: true, requestId: targetRequestId, state: finalState };
    };

    const rejectSessionRequests = (sessionId, reason = "invalidated", clearRemembered = false) => {
        const targetSessionId = String(sessionId ?? "").trim();
        for (const request of [...state.pendingPermissions.values()]) {
            if (!targetSessionId || request.sessionId === targetSessionId) resolveRequest(request, reason, false);
        }
        if (targetSessionId && clearRemembered) state.permissionRuntimeAllowlist.delete(targetSessionId);
    };

    const resetRuntime = () => {
        rejectSessionRequests(undefined, "invalidated");
        state.permissionRuntimeAllowlist.clear();
    };

    const resolveRequest = (request, finalState, activateNext = true) => {
        if (!request || request.state !== "pending") return;
        const stateName = RESOLVED_STATES.has(finalState) ? finalState : "invalidated";
        clearTimeout(request.timeout);
        request.state = stateName;
        request.resolvedAt = Date.now();
        state.pendingPermissions.delete(permissionKey(request.sessionId, request.requestId));
        state.resolvedPermissions.set(permissionKey(request.sessionId, request.requestId), request);
        const queue = requestQueues.get(request.sessionId) ?? [];
        const wasActive = queue[0] === request;
        const nextQueue = queue.filter((item) => item !== request);
        if (nextQueue.length) requestQueues.set(request.sessionId, nextQueue);
        else requestQueues.delete(request.sessionId);
        if (stateName === "allowed_always") {
            // 通用 Tool 的长期规则由 Agent 自己保存；Host 只持久化自己能安全解释的文件目录和 Bash 规则。
            const entry = request.kind === "bash"
                ? request.rememberKey
                : request.kind === "filesystem"
                    ? parentAllowlistEntry(request.path)
                    : undefined;
            if (entry) ensureRuntimeAllowlist(request.sessionId).add(entry);
        }
        eventBus.broadcast({
            type: "permission_resolved",
            sessionId: request.sessionId,
            requestId: request.requestId,
            state: stateName,
        });
        request.resolve({ allowed: stateName === "allowed_once" || stateName === "allowed_always", state: stateName, reason: stateName });
        if (activateNext && wasActive && nextQueue[0]) activateRequest(nextQueue[0]);
    };

    const updateTimeout = (timeoutMs) => {
        const nextTimeoutMs = Number(timeoutMs);
        if (Number.isFinite(nextTimeoutMs) && nextTimeoutMs > 0) requestTimeoutMs = nextTimeoutMs;
    };

    return {
        evaluate,
        ensureReadAllowed,
        isRemembered,
        requestApproval,
        respond,
        rejectSessionRequests,
        resetRuntime,
        updateTimeout,
    };
};

const permissionKey = (sessionId, requestId) => `${sessionId}:${requestId}`;
const permissionActions = (options = []) => {
    const kinds = new Set(options.map(({ kind }) => kind));
    return [
        kinds.has("allow_once") ? "allow_once" : undefined,
        kinds.has("allow_always") ? "allow_always" : undefined,
        kinds.has("reject_once") || kinds.has("reject_always") ? "deny" : undefined,
    ].filter(Boolean);
};
const normalizeRememberKey = (kind, rememberKey) => {
    const key = String(rememberKey ?? "").trim();
    return kind === "bash" && key.startsWith("bash:") ? key : undefined;
};

const defaultOptions = () => [
    { optionId: "allow_once", kind: "allow_once", name: "Allow once" },
    { optionId: "allow_always", kind: "allow_always", name: "Allow always" },
    { optionId: "deny", kind: "reject_once", name: "Deny" },
];

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
import { randomUUID } from "node:crypto";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { createCliCapabilityDefinition } from "../../capability-center/cliCapability.mjs";
import { MSINSIGHT_CAPABILITY, RAG_RETRIEVE_CAPABILITY } from "../../capability-center/definitions.mjs";
import { PUBLISH_REPORT_CAPABILITY } from "../../capability-center/artifactCapability.mjs";
import { loadCapabilityCenterConfig } from "../../config/capabilityCenterConfig.mjs";
import { createToolError, isToolDiagnosticId, normalizeToolError } from "../../utils/toolError.mjs";

const DEFAULT_BASE_URL = "http://127.0.0.1:9090";
const DEFAULT_CAPABILITY_TIMEOUT_MS = 120000;
const MAX_CAPABILITY_TIMEOUT_MS = 3605000;
const REQUEST_GRACE_MS = 5000;
const MAX_ERROR_RESPONSE_BYTES = 16 * 1024;
const SESSION_APPROVALS = Symbol("nativeCapabilityApprovals");
const SESSION_ACTIVE_CAPABILITIES = Symbol("nativeActiveCapabilities");

export const loadNativeCapabilityDefinitions = ({ resourceDir, env = process.env } = {}) => {
    const definitions = [
        { ...MSINSIGHT_CAPABILITY, requiresApproval: false },
        { ...RAG_RETRIEVE_CAPABILITY, requiresApproval: false },
        { ...PUBLISH_REPORT_CAPABILITY, requiresApproval: false },
    ];
    const configPath = join(resourceDir, "capability-center.json");
    if (!existsSync(configPath)) return definitions;
    const configured = loadCapabilityCenterConfig({ configPath, resourceDir, env });
    return [
        ...definitions,
        ...configured.map(({ name, description, timeoutMs }) => ({
            ...createCliCapabilityDefinition({ name, description }),
            timeoutMs,
            requiresApproval: true,
        })),
    ];
};

// Native Runtime 沿用原 msinsight 工厂注册方式，从同一产品配置生成代理 Tool。
export const createCapabilityTools = ({
    definitions,
    sessions,
    hostClient,
    baseUrl = process.env.INSIGHT_WEB_AGENT_BASE_URL ?? DEFAULT_BASE_URL,
    capabilityToken = process.env.MSINSIGHT_NATIVE_CAPABILITY_TOKEN,
    fetchImpl = fetch,
} = {}) => definitions.map((definition) => ({
    name: definition.name,
    description: definition.description ?? "",
    inputSchema: definition.inputSchema,
    async execute(input, context = {}) {
        const session = sessions?.get(String(context.sessionId ?? ""));
        const invocationId = isToolDiagnosticId(context.invocationId) ? context.invocationId
            : isToolDiagnosticId(context.diagnosticId) ? context.diagnosticId : randomUUID();
        if (!definition.requiresApproval) {
            return requestCapability(baseUrl, capabilityToken, {
                invocationId,
                sessionId: context.sessionId,
                name: definition.name,
                input,
                timeoutMs: definition.timeoutMs,
                signal: context.signal,
            }, fetchImpl);
        }
        const localFailure = (code, message, stage, cause) => createToolError(normalizeToolError({ code, message, retryable: code === "CAPABILITY_CONCURRENT", cause }, {
            diagnosticId: invocationId, toolName: definition.name, source: "capability_http",
            stage, outcome: "not_started", retryAdvice: code === "CAPABILITY_CONCURRENT" ? "retry_after_ready" : "do_not_retry",
        }), cause);
        if (!session) throw localFailure("CAPABILITY_SESSION_UNAVAILABLE", "Capability session is unavailable.", "validate");
        const active = session[SESSION_ACTIVE_CAPABILITIES] ?? new Set();
        if (active.has(definition.name)) throw localFailure("CAPABILITY_CONCURRENT", `Capability '${definition.name}' is already running in this session`, "dispatch");
        session[SESSION_ACTIVE_CAPABILITIES] = active;
        active.add(definition.name);
        try {
            try {
                await authorizeCapability({ definition, input, session, signal: context.signal, hostClient });
            } catch (error) {
                throw localFailure(context.signal?.aborted ? "TOOL_CANCELLED" : error?.code ?? "CAPABILITY_EXECUTION_FAILED",
                    context.signal?.aborted ? "Capability approval was cancelled." : error?.message ?? "Capability approval failed.", "authorize", error);
            }
            return await requestCapability(baseUrl, capabilityToken, {
                invocationId,
                sessionId: context.sessionId,
                name: definition.name,
                input,
                timeoutMs: definition.timeoutMs,
                signal: context.signal,
            }, fetchImpl);
        } finally {
            active.delete(definition.name);
        }
    },
}));

const authorizeCapability = async ({ definition, input, session, signal, hostClient }) => {
    if ((session.permissionModeDuringSave ?? session.permissionMode) === "full-access" || session[SESSION_APPROVALS]?.has(definition.name)) return;
    const result = await hostClient.request("session/request_permission", {
        sessionId: session.sessionId,
        kind: "tool",
        _meta: { msinsightNativePermission: true },
        title: definition.name,
        target: definition.name,
        toolCall: {
            kind: "other",
            title: definition.name,
            rawInput: input,
        },
        options: [
            { optionId: "allow_once", kind: "allow_once", name: "Allow once" },
            { optionId: "allow_always", kind: "allow_always", name: "Allow for this session" },
            { optionId: "deny", kind: "reject_once", name: "Deny" },
        ],
    }, { signal });
    const optionId = result?.outcome?.optionId;
    if (optionId === "allow_always") {
        if (!session[SESSION_APPROVALS]) session[SESSION_APPROVALS] = new Set();
        session[SESSION_APPROVALS].add(definition.name);
        return;
    }
    if (optionId !== "allow_once") throw Object.assign(new Error(`Capability '${definition.name}' was denied by the user`), { code: "CAPABILITY_DENIED_BY_USER" });
};

// 该内部 HTTP 路径避免 Native Runtime 再建立 MCP 连接，同时保留统一校验和取消语义。
const requestCapability = async (baseUrl, capabilityToken, body, fetchImpl) => {
    // Let the backend return a structured timeout error before the native HTTP guard aborts.
    const timeoutMs = capabilityTimeoutMs({ defaultTimeoutMs: body.timeoutMs, requestedTimeoutMs: body.input?.timeoutMs });
    const timeoutSignal = AbortSignal.timeout(timeoutMs);
    const signal = body.signal ? AbortSignal.any([body.signal, timeoutSignal]) : timeoutSignal;
    const context = {
        diagnosticId: body.invocationId, toolName: body.name, source: "capability_http",
        stage: "dispatch", outcome: "unknown", retryAdvice: "inspect_before_retry",
    };
    if (body.signal?.aborted) {
        throw requestFailure("TOOL_CANCELLED", "Capability invocation was cancelled.", {}, {
            ...context, outcome: "not_started", retryAdvice: "do_not_retry",
        }, body.signal.reason);
    }
    const headers = { "content-type": "application/json" };
    if (capabilityToken) headers.authorization = `Bearer ${capabilityToken}`;
    let response;
    let dispatched = false;
    try {
        const url = new URL("/api/capabilities/invoke", baseUrl);
        const payload = JSON.stringify({
            invocationId: body.invocationId,
            sessionId: body.sessionId,
            name: body.name,
            input: body.input,
        });
        dispatched = true;
        response = await fetchImpl(url, { method: "POST", headers, body: payload, signal });
    } catch (error) {
        if (body.signal?.aborted) {
            throw requestFailure("TOOL_CANCELLED", "Capability invocation was cancelled.", {}, {
                ...context, outcome: dispatched ? "unknown" : "not_started", retryAdvice: "do_not_retry",
            }, body.signal.reason);
        }
        throw requestFailure("CAPABILITY_REQUEST_FAILED", signal.aborted ? "Capability request timed out." : "Capability request failed.", {}, {
            ...context, outcome: dispatched ? "unknown" : "not_started",
        }, signal.aborted ? signal.reason : error);
    }
    let result;
    let truncated = false;
    try {
        if (response.ok) result = await response.json();
        else {
            const read = await readFailureBody(response);
            truncated = read.truncated;
            if (truncated) throw new Error("Capability error response exceeded its limit");
            result = JSON.parse(read.text);
        }
        if (!result || typeof result !== "object" || Array.isArray(result)) throw new Error("Invalid capability response envelope");
    } catch (error) {
        if (body.signal?.aborted) {
            throw requestFailure("TOOL_CANCELLED", "Capability invocation was cancelled.", {}, {
                ...context, stage: "decode", retryAdvice: "do_not_retry",
            }, body.signal.reason, response.status);
        }
        if (signal.aborted) {
            throw requestFailure("CAPABILITY_REQUEST_FAILED", "Capability response timed out.", {
                httpStatus: response.status,
            }, { ...context, stage: "decode" }, signal.reason, response.status);
        }
        throw requestFailure("CAPABILITY_RESPONSE_INVALID", `Capability returned an invalid response (HTTP ${response.status}).`, {
            httpStatus: response.status, contentType: response.headers.get("content-type"),
            ...(truncated ? { truncation: { responseBody: true } } : {}),
        }, { ...context, stage: "decode" }, error, response.status);
    }
    if (!response.ok || result.ok === false) {
        // 错误响应同时兼容归一化后的扁平结构（error/code/message）与嵌套的 error 对象。
        const nestedError = result.error && typeof result.error === "object" ? result.error : undefined;
        const unauthorized = response.status === 401 && result.error === "Unauthorized";
        const code = unauthorized ? "CAPABILITY_AUTH_FAILED" : result.code ?? nestedError?.code ?? result.error ?? "CAPABILITY_EXECUTION_FAILED";
        const message = unauthorized ? "Native capability authentication failed."
            : result.message ?? nestedError?.message ?? `Capability request failed with HTTP ${response.status}`;
        const details = result.details ?? nestedError?.details;
        const errorContext = {
            ...context,
            source: details?.source ?? context.source,
            stage: unauthorized ? "authorize" : details?.stage ?? "execute",
            outcome: unauthorized ? "not_started" : details?.outcome ?? "unknown",
            retryAdvice: unauthorized ? "do_not_retry" : details?.retryAdvice ?? context.retryAdvice,
        };
        const shape = normalizeToolError({
            code, message, retryable: result.retryable ?? nestedError?.retryable,
            details, state: result.state ?? nestedError?.state,
        }, errorContext);
        throw Object.assign(createToolError(shape), { status: response.status });
    }
    return result.result;
};

const requestFailure = (code, message, details, context, cause, status) => {
    const shape = normalizeToolError(Object.assign(new Error(message, { cause }), { code, retryable: false, details }), context);
    return Object.assign(createToolError(shape, cause), { status });
};

const readFailureBody = async (response) => {
    const reader = response.body?.getReader();
    if (!reader) return { text: "", truncated: false };
    const chunks = [];
    let bytes = 0;
    let truncated = false;
    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            const remaining = Math.max(0, MAX_ERROR_RESPONSE_BYTES - bytes);
            chunks.push(Buffer.from(value.subarray(0, remaining)));
            bytes += value.byteLength;
            if (bytes > MAX_ERROR_RESPONSE_BYTES) {
                truncated = true;
                await reader.cancel().catch(() => {});
                break;
            }
        }
    } finally {
        reader.releaseLock();
    }
    return { text: new TextDecoder().decode(Buffer.concat(chunks)), truncated };
};

export const capabilityTimeoutMs = ({ defaultTimeoutMs, requestedTimeoutMs, env = process.env } = {}) => {
    const configured = Number(env.MSINSIGHT_FRONTEND_COMMAND_TIMEOUT_MS);
    const fallback = Number.isFinite(configured) && configured > 0 ? configured : DEFAULT_CAPABILITY_TIMEOUT_MS;
    const baseline = defaultTimeoutMs ?? fallback;
    const requested = Number.isFinite(requestedTimeoutMs) && requestedTimeoutMs > 0 ? requestedTimeoutMs : 0;
    return Math.min(Math.max(baseline, requested) + REQUEST_GRACE_MS, MAX_CAPABILITY_TIMEOUT_MS);
};

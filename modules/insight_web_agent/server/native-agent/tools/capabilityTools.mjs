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
import { MSINSIGHT_CAPABILITY } from "../../capability-center/definitions.mjs";
import { loadCapabilityCenterConfig } from "../../config/capabilityCenterConfig.mjs";

const DEFAULT_BASE_URL = "http://127.0.0.1:9090";
const DEFAULT_CAPABILITY_TIMEOUT_MS = 30000;
const MAX_CAPABILITY_TIMEOUT_MS = 60000;
const SESSION_APPROVALS = Symbol("nativeCapabilityApprovals");
const SESSION_ACTIVE_CAPABILITIES = Symbol("nativeActiveCapabilities");

export const loadNativeCapabilityDefinitions = ({ resourceDir, env = process.env } = {}) => {
    const definitions = [{ ...MSINSIGHT_CAPABILITY, requiresApproval: false }];
    const configPath = join(resourceDir, "capability-center.json");
    if (!existsSync(configPath)) return definitions;
    const configured = loadCapabilityCenterConfig({ configPath, resourceDir, env });
    return [
        ...definitions,
        ...configured.map(({ name, description }) => ({
            ...createCliCapabilityDefinition({ name, description }),
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
} = {}) => definitions.map((definition) => ({
    name: definition.name,
    description: definition.description ?? "",
    inputSchema: definition.inputSchema,
    async execute(input, context = {}) {
        const session = sessions?.get(String(context.sessionId ?? ""));
        if (!definition.requiresApproval) {
            return requestCapability(baseUrl, capabilityToken, {
                invocationId: randomUUID(),
                sessionId: context.sessionId,
                name: definition.name,
                input,
                signal: context.signal,
            });
        }
        if (!session) throw new Error(`Capability session is unavailable: ${context.sessionId}`);
        const active = session[SESSION_ACTIVE_CAPABILITIES] ?? new Set();
        if (active.has(definition.name)) throw new Error(`Capability '${definition.name}' is already running in this session`);
        session[SESSION_ACTIVE_CAPABILITIES] = active;
        active.add(definition.name);
        try {
            await authorizeCapability({ definition, input, session, signal: context.signal, hostClient });
            return await requestCapability(baseUrl, capabilityToken, {
                invocationId: randomUUID(),
                sessionId: context.sessionId,
                name: definition.name,
                input,
                signal: context.signal,
            });
        } finally {
            active.delete(definition.name);
        }
    },
}));

const authorizeCapability = async ({ definition, input, session, signal, hostClient }) => {
    if (session[SESSION_APPROVALS]?.has(definition.name)) return;
    const result = await hostClient.request("session/request_permission", {
        sessionId: session.sessionId,
        kind: "tool",
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
    if (optionId !== "allow_once") throw new Error(`Capability '${definition.name}' was denied by the user`);
};

// 该内部 HTTP 路径避免 Native Runtime 再建立 MCP 连接，同时保留统一校验和取消语义。
const requestCapability = async (baseUrl, capabilityToken, body) => {
    const timeoutMs = capabilityTimeoutMs();
    const timeoutSignal = AbortSignal.timeout(timeoutMs);
    const signal = body.signal ? AbortSignal.any([body.signal, timeoutSignal]) : timeoutSignal;
    const url = new URL("/api/capabilities/invoke", baseUrl);
    if (capabilityToken) url.searchParams.set("capabilityToken", capabilityToken);
    const response = await fetch(url, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({
            invocationId: body.invocationId,
            sessionId: body.sessionId,
            name: body.name,
            input: body.input,
        }),
        signal,
    });
    const result = await response.json();
    if (!response.ok) {
        const errorBody = result.error ?? {};
        throw Object.assign(new Error(errorBody.message ?? `HTTP ${response.status}`), errorBody);
    }
    return result.result;
};

const capabilityTimeoutMs = () => {
    const configured = Number(process.env.MSINSIGHT_FRONTEND_COMMAND_TIMEOUT_MS);
    if (!Number.isFinite(configured) || configured <= 0) return DEFAULT_CAPABILITY_TIMEOUT_MS;
    return Math.min(configured, MAX_CAPABILITY_TIMEOUT_MS);
};

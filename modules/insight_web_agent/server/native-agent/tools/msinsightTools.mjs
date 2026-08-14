/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";

const DEFAULT_BASE_URL = "http://127.0.0.1:9090";
const DEFAULT_FRONTEND_COMMAND_TIMEOUT_MS = 30000;
const MAX_FRONTEND_COMMAND_TIMEOUT_MS = 60000;

export const createMsinsightTools = ({
    baseUrl = process.env.INSIGHT_WEB_AGENT_BASE_URL ?? DEFAULT_BASE_URL,
    capabilityToken = process.env.ACP_CAPABILITY_TOKEN,
} = {}) => [{
    name: "msinsight",
    description: "Discover, observe, and operate the current MindStudio Insight page through structured commands. Use command 'help' to discover available commands.",
    inputSchema: {
        type: "object",
        properties: {
            command: { type: "string", minLength: 1 },
            args: { type: "object" },
        },
        required: ["command"],
        additionalProperties: false,
    },
    async execute(input, context) {
        const command = String(input?.command ?? "").trim();
        if (!command) throw new Error("command is required");
        if (input?.args !== undefined && (!input.args || typeof input.args !== "object" || Array.isArray(input.args))) {
            throw new Error("args must be an object");
        }
        return requestFrontendCommand(baseUrl, capabilityToken, {
            requestId: randomUUID(),
            sessionId: context?.sessionId,
            command,
            args: input?.args ?? {},
            signal: context?.signal,
        });
    },
}];

const requestFrontendCommand = async (baseUrl, capabilityToken, { requestId, sessionId, command, args, signal }) => {
    const timeoutMs = frontendCommandTimeoutMs();
    const deadline = Date.now() + timeoutMs;
    const timeoutController = new AbortController();
    const timeout = setTimeout(() => timeoutController.abort(), timeoutMs + 1000);
    const combinedSignal = signal ? AbortSignal.any([signal, timeoutController.signal]) : timeoutController.signal;
    const requestUrl = commandUrl(baseUrl, capabilityToken, "/api/frontend-commands/request");
    const cancel = () => {
        void fetch(commandUrl(baseUrl, capabilityToken, "/api/frontend-commands/cancel"), {
            method: "POST",
            headers: { "content-type": "application/json" },
            body: JSON.stringify({ requestId, reason: "cancelled" }),
        }).catch(() => undefined);
    };
    signal?.addEventListener("abort", cancel, { once: true });
    try {
        const response = await fetch(requestUrl, {
            method: "POST",
            headers: { "content-type": "application/json" },
            body: JSON.stringify({ requestId, sessionId, command, args, deadline }),
            signal: combinedSignal,
        });
        const result = await response.json();
        if (!response.ok) {
            const errorBody = result.error ?? {};
            throw Object.assign(new Error(errorBody.message ?? `HTTP ${response.status}`), errorBody);
        }
        return result.result;
    } finally {
        clearTimeout(timeout);
        signal?.removeEventListener("abort", cancel);
    }
};

const frontendCommandTimeoutMs = () => {
    const configured = Number(process.env.MSINSIGHT_FRONTEND_COMMAND_TIMEOUT_MS);
    if (!Number.isFinite(configured) || configured <= 0) return DEFAULT_FRONTEND_COMMAND_TIMEOUT_MS;
    return Math.min(configured, MAX_FRONTEND_COMMAND_TIMEOUT_MS);
};

const commandUrl = (baseUrl, capabilityToken, pathname) => {
    const url = new URL(pathname, baseUrl);
    if (capabilityToken) url.searchParams.set("capabilityToken", capabilityToken);
    return url;
};

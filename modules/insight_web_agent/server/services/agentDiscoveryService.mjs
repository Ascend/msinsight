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
import { mkdir } from "node:fs/promises";
import { join } from "node:path";
import { createAcpAdapter } from "../infrastructure/acpAdapter.mjs";
import { agentLaunchKey, withAgentIdentity } from "./agentIdentityService.mjs";

export const ACP_AGENT_CATALOG = [
    {
        id: "opencode",
        config: { name: "OpenCode(auto)", command: "opencode", args: ["acp"], env: {} },
    },
    {
        id: "claude-code",
        config: { name: "Claude Code(auto)", command: "claude-agent-acp", args: [], env: {} },
    },
    {
        id: "codex",
        config: { name: "Codex(auto)", command: "codex-acp", args: [], env: {} },
    },
    {
        id: "trae",
        config: { name: "Trae(auto)", command: "traecli", args: ["serve", "acp"], env: {} },
    },
];

const INITIALIZE_PARAMS = {
    protocolVersion: 1,
    clientCapabilities: {
        fs: { readTextFile: false, writeTextFile: false },
        terminal: false,
        auth: { terminal: false },
    },
    clientInfo: { name: "insight-web-agent-discovery", version: "0.1.0" },
};

export const discoverAgents = async ({
    cwd,
    catalog = ACP_AGENT_CATALOG,
    excludedLaunchKeys = new Set(),
    adapterFactory = createAcpAdapter,
    timeoutMs = 3000,
} = {}) => {
    const discoveryRoot = join(cwd, ".discovery");
    await mkdir(discoveryRoot, { recursive: true });

    const results = await Promise.all(catalog.map((candidate) => {
        if (excludedLaunchKeys.has(agentLaunchKey(candidate.config))) {
            return {
                candidate,
                available: false,
                reason: "skipped_configured",
                elapsedMs: 0,
            };
        }
        return probeAgent(candidate, {
            adapterFactory,
            cwd: join(discoveryRoot, candidate.id),
            timeoutMs,
        });
    }));
    return {
        agentServers: results
            .filter((result) => result.available)
            .map(({ candidate }) => withAgentIdentity(candidate.config, "discovered")),
        results,
    };
};

export const mergeAgentServers = (discovered = [], configured = []) => {
    const discoveredKeys = new Set(discovered.map(agentLaunchKey));
    return [...discovered, ...configured.filter((agent) => !discoveredKeys.has(agentLaunchKey(agent)))];
};

export const sameAgentLaunch = (left, right) => left?.command === right?.command
    && JSON.stringify(left?.args ?? []) === JSON.stringify(right?.args ?? [])
    && JSON.stringify(left?.env ?? {}) === JSON.stringify(right?.env ?? {});

export const agentConfigForLog = (agentServer) => ({
    name: agentServer.name,
    command: agentServer.command,
    args: [...(agentServer.args ?? [])],
    env: Object.fromEntries(Object.entries(agentServer.env ?? {}).map(([key, value]) => [
        key,
        /key|token|secret|password|auth|credential/i.test(key) ? "***" : value,
    ])),
});

const probeAgent = async (candidate, { adapterFactory, cwd, timeoutMs }) => {
    const startedAt = Date.now();
    let adapter;

    try {
        await mkdir(cwd, { recursive: true });
        adapter = adapterFactory({
            agentServer: candidate.config,
            cwd,
            requestTimeoutMs: timeoutMs,
            promptRequestTimeoutMs: timeoutMs,
            forwardStderr: false,
        });
        const result = await adapter.request("initialize", INITIALIZE_PARAMS);
        if (!result || typeof result !== "object") {
            return { candidate, available: false, reason: "invalid_response", elapsedMs: Date.now() - startedAt };
        }
        return {
            candidate,
            available: true,
            agentInfo: result.agentInfo ?? result.agent_info,
            elapsedMs: Date.now() - startedAt,
        };
    } catch (error) {
        return {
            candidate,
            available: false,
            reason: classifyProbeError(error),
            message: error.message,
            elapsedMs: Date.now() - startedAt,
        };
    } finally {
        if (adapter) {
            try {
                await adapter.disconnect();
            } catch {
                // Probe cleanup failures do not change the discovery result.
            }
        }
    }
};

const classifyProbeError = (error) => {
    if (error?.code === "ENOENT" || /not recognized|not found/i.test(error?.message ?? "")) return "not_found";
    if (/timed out/i.test(error?.message ?? "")) return "timeout";
    return "initialization_failed";
};

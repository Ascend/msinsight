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
import { readFile, rename, rm, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { agentLaunchKey, BUILTIN_AGENT_NAME } from "./agentIdentityService.mjs";

const AGENT_CONFIG_FILE = "agent-servers.json";
const SESSION_CONFIG_FILE = "acp-session-conf.json";
const NATIVE_CONFIG_FILE = "msinsight-native.json";
const DEFAULT_REQUEST_TIMEOUT_MS = 30000;
const DEFAULT_PROMPT_TIMEOUT_MS = 5 * 60 * 1000;
const DEFAULT_PERMISSION_TIMEOUT_MS = 5 * 60 * 1000;

export const createAgentConfigService = ({ rootDir, state, beforeReload, reloadRuntime, tempId = randomUUID } = {}) => {
    const agentConfigPath = join(rootDir, AGENT_CONFIG_FILE);
    const sessionConfigPath = join(rootDir, SESSION_CONFIG_FILE);
    const nativeConfigPath = join(rootDir, NATIVE_CONFIG_FILE);

    const readSnapshot = async () => {
        const [agentConfig, sessionConfig, builtinAgent] = await Promise.all([
            readJson(agentConfigPath),
            readOptionalJson(sessionConfigPath),
            readJson(nativeConfigPath),
        ]);
        return normalizeSnapshot(agentConfig, sessionConfig, builtinAgent);
    };

    const saveAgentServers = async (input) => {
        const currentSnapshot = await readSnapshot();
        if (isBusy(state)) return structuredError("agent_busy", "Agent is busy", 409);
        const validation = validateAgentConfig(input, currentSnapshot);
        if (validation.error) return validation;
        return saveSection({
            path: agentConfigPath,
            value: validation.config,
            readSnapshot,
            beforeReload,
            reloadRuntime,
            tempId,
            section: "agentServers",
        });
    };

    const saveBuiltinAgent = async (input) => {
        if (isBusy(state)) return structuredError("agent_busy", "Agent is busy", 409);
        const validation = validateBuiltinConfig(input);
        if (validation.error) return validation;
        return saveSection({
            path: nativeConfigPath,
            value: validation.config,
            readSnapshot,
            reloadRuntime,
            tempId,
            section: "builtinAgent",
        });
    };

    const saveSessionConfig = async (input) => {
        if (isBusy(state)) return structuredError("agent_busy", "Agent is busy", 409);
        const validation = validateSessionConfigInput(input);
        if (validation.error) return validation;
        return saveSection({
            path: sessionConfigPath,
            value: validation.config,
            readSnapshot,
            reloadRuntime,
            tempId,
            section: "sessionConfig",
        });
    };

    return { readSnapshot, saveAgentServers, saveBuiltinAgent, saveSessionConfig };
};

const readJson = async (path) => JSON.parse((await readFile(path, "utf8")).replace(/^﻿/, ""));

const readOptionalJson = async (path) => {
    try {
        return await readJson(path);
    } catch (error) {
        if (error.code === "ENOENT") return {};
        throw error;
    }
};

const normalizeSnapshot = (agentConfig = {}, sessionConfig = {}, builtinAgent = {}) => {
    const agentServers = normalizeAgentServers(agentConfig.agentServers);
    const requestedActiveAgentName = String(agentConfig.activeAgent ?? agentServers[0]?.name ?? "").trim();
    const activeAgentName = requestedActiveAgentName === BUILTIN_AGENT_NAME
        || isDiscoveredAgentName(requestedActiveAgentName)
        || agentServers.some((server) => server.name === requestedActiveAgentName)
        ? requestedActiveAgentName
        : agentServers[0]?.name ?? BUILTIN_AGENT_NAME;
    return {
        activeAgentName,
        agentServers,
        builtinAgent: normalizeBuiltinAgent(builtinAgent),
        sessionConfig: normalizeSessionConfig(sessionConfig),
    };
};

const normalizeBuiltinAgent = (config = {}) => ({
    schemaVersion: 1,
    name: BUILTIN_AGENT_NAME,
    provider: String(config.provider ?? "openai"),
    model: String(config.model ?? ""),
    baseUrl: String(config.baseUrl ?? ""),
    apiKey: String(config.apiKey ?? ""),
});

const normalizeAgentServers = (servers) => Array.isArray(servers)
    ? servers.map((server) => ({
        name: String(server?.name ?? "").trim(),
        command: String(server?.command ?? "").trim(),
        args: Array.isArray(server?.args) ? server.args.map(String) : [],
        env: normalizeEnv(server?.env),
    })).filter((server) => server.name && server.command)
    : [];

const normalizeEnv = (env) => {
    if (!env || typeof env !== "object" || Array.isArray(env)) return {};
    return Object.fromEntries(Object.entries(env)
        .filter(([, value]) => value !== undefined && value !== null)
        .map(([key, value]) => [String(key), String(value)]));
};

const normalizeSessionConfig = (config = {}) => ({
    requestTimeoutMs: normalizeTimeout(config.requestTimeoutMs, DEFAULT_REQUEST_TIMEOUT_MS),
    promptRequestTimeoutMs: normalizeTimeout(config.promptRequestTimeoutMs, DEFAULT_PROMPT_TIMEOUT_MS),
    permissionRequestTimeoutMs: normalizeTimeout(config.permissionRequestTimeoutMs, DEFAULT_PERMISSION_TIMEOUT_MS),
    defaultAllowlist: {
        includeDocsRoot: config.defaultAllowlist?.includeDocsRoot !== false,
        includeAgentWorkspaceRoot: config.defaultAllowlist?.includeAgentWorkspaceRoot !== false,
        includeProjectRoot: config.defaultAllowlist?.includeProjectRoot !== false,
        extraPaths: Array.isArray(config.defaultAllowlist?.extraPaths) ? config.defaultAllowlist.extraPaths.map(String).filter((path) => path.trim()) : [],
    },
});

const normalizeTimeout = (value, fallback) => {
    const timeout = Number(value ?? fallback);
    return Number.isFinite(timeout) && timeout > 0 ? timeout : fallback;
};

const validateAgentConfig = (input, currentSnapshot) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) return structuredError("validation_failed", "agent config must be an object", 400);
    const errors = [];
    const agentServers = validateAgentServers(input.agentServers, currentSnapshot, errors);
    const activeAgentName = String(input.activeAgentName ?? "").trim();
    if (!activeAgentName) errors.push({ field: "activeAgentName", message: "active agent is required" });
    if (activeAgentName
        && activeAgentName !== BUILTIN_AGENT_NAME
        && !isDiscoveredAgentName(activeAgentName)
        && !agentServers.some((server) => server.name === activeAgentName)) {
        errors.push({ field: "activeAgentName", message: "active agent must reference a configured agent" });
    }
    const duplicateLaunch = findLaunchDuplicate(agentServers);

    if (errors.length) return structuredError("validation_failed", errors[0].message, 400, { details: errors });
    if (duplicateLaunch) {
        return structuredError(
            "duplicate_agent_launch",
            `ACP launch command duplicates configured agent: ${duplicateLaunch.existing.name}`,
            409,
            {
                conflictingAgent: duplicateLaunch.existing.name,
                duplicateAgent: duplicateLaunch.duplicate.name,
                launchKey: duplicateLaunch.launchKey,
            },
        );
    }
    return { config: { activeAgent: activeAgentName, agentServers } };
};

const validateBuiltinConfig = (input) => {
    if (!input || typeof input !== "object" || Array.isArray(input)) return structuredError("validation_failed", "built-in agent config must be an object", 400);
    const errors = [];
    const config = validateBuiltinAgent(input, errors);
    if (errors.length) return structuredError("validation_failed", errors[0].message, 400, { details: errors });
    return { config: {
        schemaVersion: 1,
        provider: config.provider,
        model: config.model,
        baseUrl: config.baseUrl,
        apiKey: config.apiKey,
    } };
};

const validateSessionConfigInput = (input) => {
    const errors = [];
    const config = validateSessionConfig(input, errors);
    if (errors.length) return structuredError("validation_failed", errors[0].message, 400, { details: errors });
    return { config };
};

const validateAgentServers = (servers, currentSnapshot, errors) => {
    if (!Array.isArray(servers)) {
        errors.push({ field: "agentServers", message: "agent servers must be an array" });
        return [];
    }

    const seen = new Set();
    const normalized = servers.map((server, index) => {
        const name = String(server?.name ?? "").trim();
        const command = String(server?.command ?? "").trim();
        const args = Array.isArray(server?.args) ? server.args.map((arg) => String(arg).trim()) : undefined;
        const env = validateEnv(server?.env, `agentServers.${index}.env`, errors);
        if (!name) errors.push({ field: `agentServers.${index}.name`, message: "agent name is required" });
        if (name === BUILTIN_AGENT_NAME) errors.push({ field: `agentServers.${index}.name`, message: `${BUILTIN_AGENT_NAME} is reserved for the built-in agent` });
        if (/\(auto\)$/i.test(name)) errors.push({ field: `agentServers.${index}.name`, message: "agent names ending in (auto) are reserved for discovered agents" });
        if (seen.has(name)) errors.push({ field: `agentServers.${index}.name`, message: "agent names must be unique" });
        seen.add(name);
        if (!command) errors.push({ field: `agentServers.${index}.command`, message: "agent command is required" });
        if (!Array.isArray(args)) {
            errors.push({ field: `agentServers.${index}.args`, message: "agent args must be an array" });
        } else if (args.some((arg) => arg === "")) {
            errors.push({ field: `agentServers.${index}.args`, message: "agent args cannot be empty" });
        }
        return { name, command, args: args ?? [], env };
    });

    for (const existing of currentSnapshot.agentServers) {
        if (!seen.has(existing.name)) {
            errors.push({ field: "agentServers", message: `existing agent cannot be deleted or renamed: ${existing.name}` });
        }
    }
    return normalized;
};

const validateBuiltinAgent = (input, errors) => {
    const config = normalizeBuiltinAgent(input);
    if (!config.provider.trim()) errors.push({ field: "builtinAgent.provider", message: "provider is required" });
    if (!config.model.trim()) errors.push({ field: "builtinAgent.model", message: "model is required" });
    if (!config.baseUrl.trim()) errors.push({ field: "builtinAgent.baseUrl", message: "base URL is required" });
    return config;
};

const findLaunchDuplicate = (servers) => {
    const seen = new Map();
    for (const server of servers) {
        const key = agentLaunchKey(server);
        const existing = seen.get(key);
        if (existing) return { existing, duplicate: server, launchKey: key };
        seen.set(key, server);
    }
    return undefined;
};

const isDiscoveredAgentName = (name) => /\(auto\)$/i.test(String(name ?? "").trim());

const validateEnv = (env, field, errors) => {
    if (!env || typeof env !== "object" || Array.isArray(env)) return {};
    const entries = Object.entries(env).map(([key, value]) => [String(key).trim(), String(value ?? "")]);
    const seen = new Set();
    for (const [key] of entries) {
        if (!key) errors.push({ field, message: "env keys cannot be empty" });
        if (seen.has(key)) errors.push({ field, message: "env keys must be unique" });
        seen.add(key);
    }
    return Object.fromEntries(entries);
};

const validateSessionConfig = (config, errors) => {
    if (!config || typeof config !== "object" || Array.isArray(config)) {
        errors.push({ field: "sessionConfig", message: "session config is required" });
        return normalizeSessionConfig();
    }
    const requestTimeoutMs = validatePositiveNumber(config.requestTimeoutMs, "sessionConfig.requestTimeoutMs", errors);
    const promptRequestTimeoutMs = validatePositiveNumber(config.promptRequestTimeoutMs, "sessionConfig.promptRequestTimeoutMs", errors);
    const permissionRequestTimeoutMs = validatePositiveNumber(config.permissionRequestTimeoutMs, "sessionConfig.permissionRequestTimeoutMs", errors);
    const defaultAllowlist = validateDefaultAllowlist(config.defaultAllowlist, errors);
    return { requestTimeoutMs, promptRequestTimeoutMs, permissionRequestTimeoutMs, defaultAllowlist };
};

const validatePositiveNumber = (value, field, errors) => {
    const number = Number(value);
    if (!Number.isFinite(number) || number <= 0) {
        errors.push({ field, message: "timeout must be a positive number" });
    }
    return number;
};

const validateDefaultAllowlist = (input = {}, errors) => {
    const config = input && typeof input === "object" && !Array.isArray(input) ? input : {};
    for (const field of ["includeDocsRoot", "includeAgentWorkspaceRoot", "includeProjectRoot"]) {
        if (typeof config[field] !== "boolean") errors.push({ field: `sessionConfig.defaultAllowlist.${field}`, message: "allowlist flags must be booleans" });
    }
    if (!Array.isArray(config.extraPaths)) errors.push({ field: "sessionConfig.defaultAllowlist.extraPaths", message: "extraPaths must be an array" });
    return {
        includeDocsRoot: config.includeDocsRoot,
        includeAgentWorkspaceRoot: config.includeAgentWorkspaceRoot,
        includeProjectRoot: config.includeProjectRoot,
        extraPaths: Array.isArray(config.extraPaths) ? config.extraPaths.map(String).filter((path) => path.trim()) : [],
    };
};

const saveSection = async ({ path, value, readSnapshot, beforeReload, reloadRuntime, tempId, section }) => {
    const tempPath = `${path}.${process.pid}.${tempId()}.tmp`;
    try {
        await writeFile(tempPath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
        await rename(tempPath, path);
    } catch (error) {
        try {
            await rm(tempPath, { force: true });
        } catch {
            // Preserve the original write error when temporary cleanup fails.
        }
        return structuredError("config_write_failed", error.message, 500);
    }

    let rollbackBeforeReload;
    try {
        const snapshot = await readSnapshot();
        rollbackBeforeReload = await beforeReload?.(snapshot);
        await reloadRuntime?.(snapshot, section);
        return { ok: true, snapshot };
    } catch (error) {
        await rollbackBeforeReload?.();
        return structuredError("reload_failed", error.message, 500, { saved: true });
    }
};

export const isBusy = (state) => {
    if (!state) return false;
    if ([...(state.sessionContexts?.values?.() ?? [])].some((context) => context?.pendingPrompt)) return true;
    return [...(state.pendingPermissions?.values?.() ?? [])].some((request) => request?.state === "pending");
};

const structuredError = (error, message, status, extra = {}) => ({ error, message, status, ...extra });

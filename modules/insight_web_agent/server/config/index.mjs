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
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, resolve } from "node:path";
import { initLogger } from "../utils/logger.mjs";

const defaultRootDir = (() => {
    const entryDir = dirname(resolve(process.argv[1] ?? "."));
    return basename(entryDir) === "server" ? dirname(entryDir) : entryDir;
})();

const parseCliOptions = (args) => {
    const options = {};
    for (let index = 0; index < args.length; index += 1) {
        const arg = args[index];
        if (arg === "--path") {
            options.path = args[index + 1];
            index += 1;
            continue;
        }
        if (arg.startsWith("--path=")) {
            options.path = arg.slice("--path=".length);
            continue;
        }
        if (arg === "--resource-path") {
            options.resourcePath = args[index + 1];
            index += 1;
            continue;
        }
        if (arg.startsWith("--resource-path=")) {
            options.resourcePath = arg.slice("--resource-path=".length);
            continue;
        }
        if (arg === "--port") {
            options.port = args[index + 1];
            index += 1;
            continue;
        }
        if (arg.startsWith("--port=")) {
            options.port = arg.slice("--port=".length);
            continue;
        }
        if (arg === "--host") {
            options.host = args[index + 1];
            index += 1;
            continue;
        }
        if (arg.startsWith("--host=")) {
            options.host = arg.slice("--host=".length);
        }
    }
    return options;
};

const normalizeRootDir = (input) => {
    const value = String(input ?? "").trim();
    if (!value) return defaultRootDir;
    return isAbsolute(value) ? value : resolve(process.cwd(), value);
};

const normalizePort = (input, fallback) => {
    const port = Number(input ?? fallback);
    return Number.isInteger(port) && port > 0 && port < 65536 ? port : fallback;
};

const normalizeHost = (input, fallback) => {
    const host = String(input ?? "").trim();
    return host || fallback;
};

const loadJsonConfig = (configPath, label) => {
    try {
        return JSON.parse(readFileSync(configPath, "utf8").replace(/^﻿/, ""));
    } catch (error) {
        throw new Error(`Failed to load ${label} ${configPath}: ${error.message}`);
    }
};

const loadAgentServersConfig = (configPath) => loadJsonConfig(configPath, "ACP agent server config");

const loadSessionConfig = (configPath) => {
    if (!existsSync(configPath)) return {};
    return loadJsonConfig(configPath, "ACP session config");
};

const normalizeTimeout = (value, fallback) => {
    const timeout = Number(value ?? fallback);
    return Number.isFinite(timeout) && timeout > 0 ? timeout : fallback;
};

const normalizeDefaultAllowlistConfig = (config = {}) => ({
    includeDocsRoot: config.includeDocsRoot !== false,
    includeAgentWorkspaceRoot: config.includeAgentWorkspaceRoot !== false,
    includeProjectRoot: config.includeProjectRoot !== false,
    extraPaths: Array.isArray(config.extraPaths) ? config.extraPaths.map(String) : [],
});

const normalizeSessionConfig = (config = {}) => ({
    requestTimeoutMs: normalizeTimeout(config.requestTimeoutMs, 30000),
    promptRequestTimeoutMs: normalizeTimeout(config.promptRequestTimeoutMs, 5 * 60 * 1000),
    permissionRequestTimeoutMs: normalizeTimeout(config.permissionRequestTimeoutMs, 5 * 60 * 1000),
    defaultAllowlist: normalizeDefaultAllowlistConfig(config.defaultAllowlist),
});

const mergeSessionConfig = (sessionConfig, env) => ({
    ...sessionConfig,
    requestTimeoutMs: normalizeTimeout(env.ACP_REQUEST_TIMEOUT_MS, sessionConfig.requestTimeoutMs),
    promptRequestTimeoutMs: normalizeTimeout(env.ACP_PROMPT_REQUEST_TIMEOUT_MS, sessionConfig.promptRequestTimeoutMs),
    permissionRequestTimeoutMs: normalizeTimeout(env.ACP_PERMISSION_REQUEST_TIMEOUT_MS, sessionConfig.permissionRequestTimeoutMs),
});

const sessionConfigPathForRoot = (rootDir) => join(rootDir, "acp-session-conf.json");
const agentServersConfigPathForRoot = (rootDir) => join(rootDir, "agent-servers.json");

const loadResolvedSessionConfig = (rootDir, env) => mergeSessionConfig(
    normalizeSessionConfig(loadSessionConfig(sessionConfigPathForRoot(rootDir))),
    env,
);

const loadResolvedAgentServersConfig = (rootDir) => loadAgentServersConfig(agentServersConfigPathForRoot(rootDir));

const filterAvailableAllowlistPaths = (paths = []) => paths.filter((path) => String(path ?? "").trim());

const resolveExtraAllowlistPaths = (rootDir, paths = []) => filterAvailableAllowlistPaths(paths).map((path) => (
    isAbsolute(path) ? path : resolve(rootDir, path)
));

const createSessionConfigPaths = (rootDir, sessionConfig) => ({
    sessionConfigPath: sessionConfigPathForRoot(rootDir),
    extraAllowlistPaths: resolveExtraAllowlistPaths(rootDir, sessionConfig.defaultAllowlist.extraPaths),
});

const createResolvedSessionConfig = (rootDir, env) => {
    const sessionConfig = loadResolvedSessionConfig(rootDir, env);
    return {
        ...sessionConfig,
        ...createSessionConfigPaths(rootDir, sessionConfig),
    };
};

const createResolvedAgentConfig = (rootDir) => {
    const agentServersConfigPath = agentServersConfigPathForRoot(rootDir);
    return {
        agentServersConfigPath,
        agentServersConfig: loadResolvedAgentServersConfig(rootDir),
    };
};

const loadRuntimeConfigBundle = (rootDir, env) => {
    const resolvedSessionConfig = createResolvedSessionConfig(rootDir, env);
    const resolvedAgentConfig = createResolvedAgentConfig(rootDir);
    return {
        ...resolvedSessionConfig,
        ...resolvedAgentConfig,
    };
};

const readRuntimeConfigBundle = (rootDir, env) => loadRuntimeConfigBundle(rootDir, env);

const loadSystemPrompt = (resourceDir) => {
    const filePath = join(resourceDir, "prompts", "system.md");
    if (!existsSync(filePath)) return "";
    try {
        return readFileSync(filePath, "utf8").replace(/^﻿/, "").trim();
    } catch (error) {
        console.warn(`Failed to load system prompt ${filePath}: ${error.message}`);
        return "";
    }
};

const normalizeAgentServers = (servers) => {
    if (!Array.isArray(servers)) return [];
    return servers.map((server) => ({
        name: String(server?.name ?? "").trim(),
        command: String(server?.command ?? "").trim(),
        args: Array.isArray(server?.args) ? server.args.map(String) : [],
        env: normalizeEnv(server?.env),
    })).filter((server) => server.name && server.command);
};

const normalizeEnv = (env) => {
    if (!env || typeof env !== "object" || Array.isArray(env)) return {};
    return Object.fromEntries(Object.entries(env)
        .filter(([, value]) => value !== undefined && value !== null)
        .map(([key, value]) => [String(key), String(value)]));
};

const cliOptions = parseCliOptions(process.argv.slice(2));
const rootDir = normalizeRootDir(cliOptions.path ?? process.env.ACP_ROOT ?? defaultRootDir);
const resourceDir = normalizeRootDir(cliOptions.resourcePath ?? process.env.ACP_RESOURCE_ROOT ?? defaultRootDir);

const createRuntimeConfig = (rootDir, resourceDir, env) => {
    const {
        agentServersConfigPath,
        agentServersConfig,
        sessionConfigPath,
        requestTimeoutMs,
        promptRequestTimeoutMs,
        permissionRequestTimeoutMs,
        defaultAllowlist,
        extraAllowlistPaths,
    } = readRuntimeConfigBundle(rootDir, env);
    const agentServers = normalizeAgentServers(agentServersConfig.agentServers);
    const requestedActiveAgentName = env.ACP_AGENT ?? agentServersConfig.activeAgent ?? agentServers[0]?.name;
    const agentServer = agentServers.find((server) => server.name === requestedActiveAgentName) ?? agentServers[0];
    const port = normalizePort(cliOptions.port ?? env.PORT, 9090);

    if (!agentServer) {
        throw new Error(`No ACP agent servers configured in ${agentServersConfigPath}`);
    }

    return {
        rootDir,
        resourceDir,
        agentServersConfigPath,
        sessionConfigPath,
        agentServers,
        activeAgentName: agentServer.name,
        agentServer,
        host: normalizeHost(cliOptions.host ?? env.HOST, "127.0.0.1"),
        port,
        cwd: env.ACP_CWD ?? join(rootDir, "agent-workspace"),
        debug: env.ACP_DEBUG === "1",
        defaultModel: env.ACP_MODEL,
        systemPrompt: loadSystemPrompt(resourceDir),
        requestTimeoutMs,
        promptRequestTimeoutMs,
        permissionRequestTimeoutMs,
        defaultAllowlist,
        extraAllowlistPaths,
        requestedActiveAgentName,
    };
};

export const config = createRuntimeConfig(rootDir, resourceDir, process.env);
initLogger({ rootDir: config.rootDir, port: config.port });

export const reloadConfig = (env = process.env) => {
    const nextConfig = createRuntimeConfig(rootDir, resourceDir, env);
    Object.keys(config).forEach((key) => delete config[key]);
    Object.assign(config, nextConfig);
    return config;
};

export const saveActiveAgent = (name) => {
    const currentConfig = loadResolvedAgentServersConfig(config.rootDir);
    const nextConfig = { ...currentConfig, activeAgent: name };
    writeFileSync(config.agentServersConfigPath, `${JSON.stringify(nextConfig, null, 2)}\n`, "utf8");
};

if (!process.env.ACP_AGENT && config.requestedActiveAgentName && config.requestedActiveAgentName !== config.agentServer.name) {
    console.warn(`Configured active agent "${config.requestedActiveAgentName}" is unavailable; using "${config.agentServer.name}".`);
    try {
        saveActiveAgent(config.agentServer.name);
    } catch (error) {
        console.warn(`Failed to save active agent fallback: ${error.message}`);
    }
}

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
        if (arg === "--port") {
            options.port = args[index + 1];
            index += 1;
            continue;
        }
        if (arg.startsWith("--port=")) {
            options.port = arg.slice("--port=".length);
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

const loadAgentServersConfig = (configPath) => {
    try {
        return JSON.parse(readFileSync(configPath, "utf8").replace(/^\uFEFF/, ""));
    } catch (error) {
        throw new Error(`Failed to load ACP agent server config ${configPath}: ${error.message}`);
    }
};

const loadSystemPrompt = (rootDir) => {
    const filePath = join(rootDir, "prompts", "system.md");
    if (!existsSync(filePath)) return "";
    try {
        return readFileSync(filePath, "utf8").replace(/^\uFEFF/, "").trim();
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
const agentServersConfigPath = join(rootDir, "agent-servers.json");
const agentServersConfig = loadAgentServersConfig(agentServersConfigPath);
const agentServers = normalizeAgentServers(agentServersConfig.agentServers);
const requestedActiveAgentName = process.env.ACP_AGENT ?? agentServersConfig.activeAgent ?? agentServers[0]?.name;
const agentServer = agentServers.find((server) => server.name === requestedActiveAgentName) ?? agentServers[0];

if (!agentServer) {
    throw new Error(`No ACP agent servers configured in ${agentServersConfigPath}`);
}

export const config = {
    rootDir,
    agentServersConfigPath,
    agentServers,
    activeAgentName: agentServer.name,
    agentServer,
    port: normalizePort(cliOptions.port ?? process.env.PORT, 9090),
    cwd: process.env.ACP_CWD ?? join(rootDir, "agent-workspace"),
    debug: process.env.ACP_DEBUG === "1",
    defaultModel: process.env.ACP_MODEL,
    systemPrompt: loadSystemPrompt(rootDir),
};

export const saveActiveAgent = (name) => {
    const nextConfig = { ...agentServersConfig, activeAgent: name };
    writeFileSync(agentServersConfigPath, `${JSON.stringify(nextConfig, null, 2)}\n`, "utf8");
};

if (!process.env.ACP_AGENT && requestedActiveAgentName && requestedActiveAgentName !== agentServer.name) {
    console.warn(`Configured active agent "${requestedActiveAgentName}" is unavailable; using "${agentServer.name}".`);
    try {
        saveActiveAgent(agentServer.name);
    } catch (error) {
        console.warn(`Failed to save active agent fallback: ${error.message}`);
    }
}

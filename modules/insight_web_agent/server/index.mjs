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
import { existsSync } from "node:fs";
import { lstat, mkdir, realpath, symlink, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { createApp } from "./app.mjs";
import { config, reloadConfig, saveActiveAgent } from "./config/index.mjs";
import { createAcpAdapter } from "./infrastructure/acpAdapter.mjs";
import { createAuditLogger } from "./observability/auditLogger.mjs";
import { createAgentConfigService } from "./services/agentConfigService.mjs";
import { createChatService } from "./services/chatService.mjs";
import { createContextAssembler } from "./services/contextAssembler.mjs";
import { createFileReadService, createPermissionHostHandler } from "./services/fileReadService.mjs";
import { createPageContextService } from "./services/pageContextService.mjs";
import { createPermissionService } from "./services/permissionService.mjs";
import { createSessionManager } from "./services/sessionManager.mjs";
import { createSessionService } from "./services/sessionService.mjs";
import { createSkillService } from "./services/skillService.mjs";
import { createEventBus } from "./state/eventBus.mjs";
import { createRuntimeState, publicState, resetRuntimeForAgent, restoreRuntimeState, snapshotRuntimeState } from "./state/runtimeState.mjs";

const PROJECT_RULE_FILES = ["AGENTS.md", "CLAUDE.md"];

const syncProjectRules = async (workspacePath, systemPrompt) => {
    const body = String(systemPrompt ?? "").trim();
    if (!body) return;
    const content = `${body}\n`;
    await Promise.all(PROJECT_RULE_FILES.map((name) => writeFile(join(workspacePath, name), content, "utf8")));
};

const insightWebAgentBaseUrl = () => `http://${config.host}:${config.port}`;

const nativeFilesystemPolicy = () => ({
    includeDocsRoot: config.defaultAllowlist?.includeDocsRoot !== false,
    includeAgentWorkspaceRoot: config.defaultAllowlist?.includeAgentWorkspaceRoot !== false,
    includeProjectRoot: config.defaultAllowlist?.includeProjectRoot !== false,
    docsRoot: join(config.resourceDir, "docs"),
    skillsRoot: join(config.resourceDir, "skills"),
    projectRoot: config.rootDir,
    extraPaths: config.extraAllowlistPaths,
});

const withHostEnv = (agentServer) => ({
    ...agentServer,
    args: resolveAgentArgs(agentServer),
    env: {
        ...(agentServer.env ?? {}),
        INSIGHT_WEB_AGENT_BASE_URL: insightWebAgentBaseUrl(),
        INSIGHT_WEB_AGENT_RESOURCE_DIR: config.resourceDir,
        INSIGHT_WEB_AGENT_FILESYSTEM_POLICY: JSON.stringify(nativeFilesystemPolicy()),
        ...(agentServer.name === "msinsight-native"
            ? { MSINSIGHT_NATIVE_STORE_DIR: join(config.rootDir, ".msinsight_native_agent") }
            : {}),
    },
});

const resolveAgentArgs = (agentServer) => {
    if (agentServer.name !== "msinsight-native") return agentServer.args;
    return (agentServer.args ?? []).map((arg) => (
        arg === "server/native-agent/index.mjs" ? resolveNativeAgentEntry() : arg
    ));
};

const resolveNativeAgentEntry = () => {
    const bundledEntry = join(config.resourceDir, "native-agent", "index.mjs");
    if (existsSync(bundledEntry)) return bundledEntry;
    return join(config.resourceDir, "server", "native-agent", "index.mjs");
};

const ensureResourceSymlink = async (rootDir, name) => {
    const linkPath = join(rootDir, name);
    const targetPath = resolve(rootDir, "..", "..", name);
    let entry;
    try {
        entry = await lstat(linkPath);
    } catch (error) {
        if (error.code !== "ENOENT") {
            console.warn(`Failed to check ${name} symlink: ${error.message}`);
            return;
        }
    }
    if (entry) {
        if (!entry.isSymbolicLink()) {
            if (!entry.isDirectory()) console.warn(`Resource path exists but is not a directory: ${linkPath}`);
            return;
        }
        try {
            const actualTarget = await realpath(linkPath);
            const expectedTarget = await realpath(targetPath);
            if (actualTarget !== expectedTarget) {
                console.warn(`Resource symlink points to ${actualTarget}, expected ${expectedTarget}`);
            }
        } catch (error) {
            console.warn(`Failed to resolve ${name} symlink ${linkPath}: ${error.message}`);
        }
        return;
    }
    try {
        await symlink(targetPath, linkPath, "dir");
    } catch (error) {
        const hint = process.platform === "win32"
            ? " Enable Windows Developer Mode or run with permission to create symbolic links."
            : "";
        console.warn(`Failed to create ${name} symlink ${linkPath} -> ${targetPath}: ${error.message}.${hint}`);
    }
};

let activeAcpClient;
let activeAcpMessageBuffer;
const stagedAcpMessageBuffers = new WeakMap();

const installHostHandlers = (adapter, agentServer) => {
    if (!adapter) return;
    const cwd = () => join(config.cwd, agentServer.name);
    const adapterFileReadService = createFileReadService({ permissionService, cwd });
    const adapterPermissionHostHandler = createPermissionHostHandler({ permissionService, cwd });
    adapter.registerHandler("session/request_permission", adapterPermissionHostHandler);
    adapter.registerHandler("fs/read_text_file", (params) => adapterFileReadService.readTextFile(params));
};

const createActiveAcpAdapter = (agentServer, { autoConnect = true } = {}) => {
    const agentWorkspacePath = join(config.cwd, agentServer.name);
    const adapter = createAcpAdapter({
        agentServer: withHostEnv(agentServer),
        cwd: agentWorkspacePath,
        debug: config.debug,
        requestTimeoutMs: config.requestTimeoutMs,
        promptRequestTimeoutMs: config.promptRequestTimeoutMs,
    });
    adapter.onMessage((message) => {
        const stagedMessageBuffer = stagedAcpMessageBuffers.get(adapter);
        if (stagedMessageBuffer) {
            stagedMessageBuffer.push(message);
            return;
        }
        if (adapter !== activeAcpClient) return;
        if (activeAcpMessageBuffer) {
            activeAcpMessageBuffer.push(message);
            return;
        }
        chatService?.handleAcpNotification(message);
    });
    installHostHandlers(adapter, agentServer);
    if (autoConnect) adapter.connect();
    return adapter;
};

await Promise.all([
    ensureResourceSymlink(config.resourceDir, "docs"),
    ensureResourceSymlink(config.resourceDir, "skills"),
]);
await mkdir(config.cwd, { recursive: true });
await mkdir(join(config.cwd, config.agentServer.name), { recursive: true });
await syncProjectRules(join(config.cwd, config.agentServer.name), config.systemPrompt);

const state = createRuntimeState();
const eventBus = createEventBus(state);
const pageContextService = createPageContextService({ eventBus });
const skillService = createSkillService({ rootDir: config.resourceDir });
let chatService;
let activeAgentServer = config.agentServer;
resetRuntimeForAgent(state, { agentServers: config.agentServers, activeAgentName: activeAgentServer.name });
const permissionService = createPermissionService({ state, eventBus, config, timeoutMs: config.permissionRequestTimeoutMs });
activeAcpClient = createActiveAcpAdapter(activeAgentServer);

const acpAdapter = {
    get agentId() {
        return activeAcpClient.agentId;
    },
    get runtime() {
        return activeAcpClient.runtime;
    },
    request(method, params) {
        return activeAcpClient.request(method, params);
    },
};
const auditLogger = createAuditLogger({ cwd: config.cwd, debug: config.debug });
const contextAssembler = createContextAssembler({ state });
const sessionManager = createSessionManager({ adapter: acpAdapter, eventBus, state, config, auditLogger, permissionService });
const sessionService = createSessionService({
    acpClient: acpAdapter,
    config,
    eventBus,
    state,
    sessionManager,
});
chatService = createChatService({
    acpAdapter,
    eventBus,
    sessionService,
    skillService,
    state,
    sessionManager,
    contextAssembler,
    systemPrompt: config.systemPrompt,
});

const cloneRuntimeConfig = () => ({
    ...config,
    agentServers: [...config.agentServers],
    defaultAllowlist: { ...config.defaultAllowlist, extraPaths: [...(config.defaultAllowlist?.extraPaths ?? [])] },
    extraAllowlistPaths: [...(config.extraAllowlistPaths ?? [])],
});

const restoreRuntimeConfig = (previousConfig) => {
    Object.keys(config).forEach((key) => delete config[key]);
    Object.assign(config, previousConfig);
};

const reloadRuntime = async ({ activeAgentName, persistActiveAgent = false, reloadFromDisk = false } = {}) => {
    const previousConfig = cloneRuntimeConfig();
    const previousAgentServer = activeAgentServer;
    const previousClient = activeAcpClient;
    const previousState = snapshotRuntimeState(state);
    const previousClientMessages = [];
    const nextClientMessages = [];
    let nextClient;

    activeAcpMessageBuffer = previousClientMessages;
    try {
        if (reloadFromDisk) reloadConfig();
        const requestedAgentName = String(activeAgentName ?? config.activeAgentName ?? "").trim();
        const nextAgentServer = config.agentServers.find((server) => server.name === requestedAgentName) ?? config.agentServer;
        if (!nextAgentServer) throw new Error("agent is unavailable");
        if (persistActiveAgent) saveActiveAgent(nextAgentServer.name);

        await mkdir(join(config.cwd, nextAgentServer.name), { recursive: true });
        await syncProjectRules(join(config.cwd, nextAgentServer.name), config.systemPrompt);

        nextClient = createActiveAcpAdapter(nextAgentServer, { autoConnect: false });
        stagedAcpMessageBuffers.set(nextClient, nextClientMessages);
        permissionService.updateTimeout(config.permissionRequestTimeoutMs);
        permissionService.resetRuntime();
        resetRuntimeForAgent(state, { agentServers: config.agentServers, activeAgentName: nextAgentServer.name });
        state.activeContext = previousState.activeContext;
        nextClient.connect();
        await chatService.initialize({ targetAdapter: nextClient, broadcast: false, refreshSessions: false });
        if (!state.initialized) throw new Error(state.agentError ?? "ACP agent initialization failed");
        activeAcpMessageBuffer = undefined;
        stagedAcpMessageBuffers.delete(nextClient);
        activeAcpClient = nextClient;
        activeAgentServer = nextAgentServer;
        for (const message of nextClientMessages) chatService?.handleAcpNotification(message);
        await sessionService.refreshSessions();
        await previousClient.disconnect();
        eventBus.broadcast({ type: "state", state: publicState(state) });
        return { ok: true };
    } catch (error) {
        const failedClient = nextClient && nextClient !== previousClient ? nextClient : undefined;
        if (nextClient) stagedAcpMessageBuffers.delete(nextClient);
        activeAcpClient = previousClient;
        activeAgentServer = previousAgentServer;
        restoreRuntimeConfig(previousConfig);
        permissionService.updateTimeout(config.permissionRequestTimeoutMs);
        restoreRuntimeState(state, previousState);
        activeAcpMessageBuffer = undefined;
        for (const message of previousClientMessages) chatService?.handleAcpNotification(message);
        eventBus.broadcast({ type: "state", state: publicState(state) });
        await failedClient?.disconnect?.().catch((disconnectError) => {
            console.warn(`Failed to disconnect failed ACP adapter: ${disconnectError.message}`);
        });
        throw error;
    } finally {
        if (activeAcpMessageBuffer === previousClientMessages) activeAcpMessageBuffer = undefined;
        if (nextClient) stagedAcpMessageBuffers.delete(nextClient);
    }
};

const agentService = {
    list() {
        return {
            activeAgentName: activeAgentServer.name,
            agentServers: config.agentServers.map(({ name }) => ({ name })),
        };
    },

    async switchAgent(name) {
        const nextAgentServer = config.agentServers.find((server) => server.name === String(name ?? "").trim());
        if (!nextAgentServer) return { error: "agent is unavailable", status: 400 };
        if (nextAgentServer.name === activeAgentServer.name) return { ok: true, ...this.list() };

        try {
            await reloadRuntime({ activeAgentName: nextAgentServer.name, persistActiveAgent: true });
            return { ok: true, ...this.list() };
        } catch (error) {
            return { error: error.message, status: 500 };
        }
    },
};

const agentConfigService = createAgentConfigService({
    rootDir: config.rootDir,
    state,
    reloadRuntime: async (snapshot) => reloadRuntime({ activeAgentName: snapshot.activeAgentName, reloadFromDisk: true }),
});

await chatService.initialize();

const server = createApp({
    agentService,
    eventBus,
    chatService,
    sessionService,
    state,
    permissionService,
    agentConfigService,
    pageContextService,
});

server.listen(config.port, config.host, () => {
    console.log(`ACP web extracted API: http://${config.host}:${config.port}/`);
    console.log(`Agent: ${config.activeAgentName} (${config.agentServer.command} ${config.agentServer.args.join(" ")})`);
});

server.on("error", (error) => {
    console.error(`Failed to start HTTP server: ${error.message}`);
    process.exitCode = 1;
});

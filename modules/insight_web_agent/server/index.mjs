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
import { mkdir, writeFile } from "node:fs/promises";
import { basename, join, resolve } from "node:path";
import { createApp } from "./app.mjs";
import { config, reloadConfig, saveActiveAgent } from "./config/index.mjs";
import { createAcpAdapter } from "./infrastructure/acpAdapter.mjs";
import { createAuditLogger } from "./observability/auditLogger.mjs";
import { createAgentConfigService } from "./services/agentConfigService.mjs";
import { agentConfigForLog, discoverAgents, mergeAgentServers, sameAgentLaunch } from "./services/agentDiscoveryService.mjs";
import { agentLaunchKey } from "./services/agentIdentityService.mjs";
import { createChatService } from "./services/chatService.mjs";
import { createContextAssembler } from "./services/contextAssembler.mjs";
import { errorCause, errorResult } from "./services/errorResult.mjs";
import { createFileReadService, createPermissionHostHandler } from "./services/fileReadService.mjs";
import { createFrontendCommandService } from "./services/frontendCommandService.mjs";
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

const sourceResourceRoot = existsSync(join(config.resourceDir, "server", "native-agent", "index.mjs"));
const bundledResourceDirectory = (name) => sourceResourceRoot && ["docs", "skills"].includes(name)
    ? resolve(config.resourceDir, "..", "..", name)
    : join(config.resourceDir, name);

const nativeFilesystemPolicy = () => ({
    includeDocsRoot: config.defaultAllowlist?.includeDocsRoot !== false,
    includeAgentWorkspaceRoot: config.defaultAllowlist?.includeAgentWorkspaceRoot !== false,
    includeProjectRoot: config.defaultAllowlist?.includeProjectRoot !== false,
    docsRoot: bundledResourceDirectory("docs"),
    skillsRoot: bundledResourceDirectory("skills"),
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
        MSINSIGHT_NATIVE_BUNDLED_AGENTS_DIR: bundledResourceDirectory("agents"),
        MSINSIGHT_NATIVE_BUNDLED_SKILLS_DIR: bundledResourceDirectory("skills"),
        INSIGHT_WEB_AGENT_FILESYSTEM_POLICY: JSON.stringify(nativeFilesystemPolicy()),
        ...(agentServer.name === "msinsight-native"
            ? {
                ACP_CAPABILITY_TOKEN: config.capabilityToken,
                MSINSIGHT_NATIVE_STORE_DIR: join(config.rootDir, ".msinsight_native_agent"),
            }
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

let activeAcpClient;
let activeAcpMessageBuffer;
const stagedAcpMessageBuffers = new WeakMap();

const installHostHandlers = (adapter, agentServer) => {
    if (!adapter) return;
    const cwd = () => join(config.cwd, agentServer.workspaceKey);
    const adapterFileReadService = createFileReadService({ permissionService, cwd });
    const adapterPermissionHostHandler = createPermissionHostHandler({ permissionService, cwd });
    adapter.registerHandler("session/request_permission", adapterPermissionHostHandler);
    adapter.registerHandler("fs/read_text_file", (params) => adapterFileReadService.readTextFile(params));
};

const createActiveAcpAdapter = (agentServer, { autoConnect = true } = {}) => {
    const agentWorkspacePath = join(config.cwd, agentServer.workspaceKey);
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

await mkdir(config.cwd, { recursive: true });
await mkdir(join(config.cwd, config.agentServer.workspaceKey), { recursive: true });
await syncProjectRules(join(config.cwd, config.agentServer.workspaceKey), config.systemPrompt);

const state = createRuntimeState();
const autoDiscoveryEnabled = process.env.ACP_AUTO_DISCOVERY !== "0";
state.agentDiscoveryLoading = autoDiscoveryEnabled;
const eventBus = createEventBus(state);
const frontendCommandService = createFrontendCommandService({ eventBus });
const pageContextService = createPageContextService({ eventBus });
const skillService = createSkillService({ rootDir: config.resourceDir, skillsDir: bundledResourceDirectory("skills") });
let chatService;
let activeAgentServer = config.agentServer;
let discoveredAgentServers = [];
const availableAgentServers = () => mergeAgentServers(discoveredAgentServers, config.agentServers);
resetRuntimeForAgent(state, { agentServers: availableAgentServers(), activeAgentName: activeAgentServer.name, activeAgentWorkspaceKey: activeAgentServer.workspaceKey });
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
    notify(method, params) {
        return activeAcpClient.notify(method, params);
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
    frontendCommandService,
    permissionService,
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

const reloadRuntime = async ({ activeAgentName, persistActiveAgent = false, reloadFromDisk = false, broadcast = true } = {}) => {
    frontendCommandService.cancelAll("runtime_reloading");
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
        const nextAgentServer = availableAgentServers().find((server) => server.name === requestedAgentName) ?? config.agentServer;
        if (!nextAgentServer) throw new Error("agent is unavailable");
        await mkdir(join(config.cwd, nextAgentServer.workspaceKey), { recursive: true });
        await syncProjectRules(join(config.cwd, nextAgentServer.workspaceKey), config.systemPrompt);

        nextClient = createActiveAcpAdapter(nextAgentServer, { autoConnect: false });
        stagedAcpMessageBuffers.set(nextClient, nextClientMessages);
        permissionService.updateTimeout(config.permissionRequestTimeoutMs);
        permissionService.resetRuntime();
        resetRuntimeForAgent(state, { agentServers: availableAgentServers(), activeAgentName: nextAgentServer.name, activeAgentWorkspaceKey: nextAgentServer.workspaceKey });
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
        if (persistActiveAgent) saveActiveAgent(nextAgentServer.name);
        try {
            await previousClient.disconnect();
        } catch (disconnectError) {
            console.warn(`Failed to disconnect previous ACP adapter: ${disconnectError.message}`);
        }
        if (broadcast) eventBus.broadcast({ type: "state", state: publicState(state) });
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
        if (broadcast) eventBus.broadcast({ type: "state", state: publicState(state) });
        try {
            await failedClient?.disconnect?.();
        } catch (disconnectError) {
            console.warn(`Failed to disconnect failed ACP adapter: ${disconnectError.message}`);
        }
        throw error;
    } finally {
        if (activeAcpMessageBuffer === previousClientMessages) activeAcpMessageBuffer = undefined;
        if (nextClient) stagedAcpMessageBuffers.delete(nextClient);
    }
};

let agentRefreshPromise;
const logAgentDiscoveryResults = (label, results) => {
    for (const { candidate, available, reason, message, elapsedMs } of results) {
        const status = available ? "available" : reason;
        const details = {
            id: candidate.id,
            name: candidate.config.name,
            command: candidate.config.command,
            args: candidate.config.args ?? [],
            status,
            elapsedMs,
        };
        if (message) details.message = message;
        console.log(`ACP agent ${label} probe: ${JSON.stringify(details)}`);
    }
};

const refreshDiscoveredAgents = async () => {
    if (agentRefreshPromise) return agentRefreshPromise;
    const runRefresh = async () => {
        const previousConfig = cloneRuntimeConfig();
        const previousDiscovered = discoveredAgentServers;
        let runtimeChanged = false;
        state.agentDiscoveryLoading = true;
        eventBus.broadcast({ type: "agent_discovery_started" });
        try {
            reloadConfig();
            permissionService.updateTimeout(config.permissionRequestTimeoutMs);
            const excludedLaunchKeys = new Set(config.configuredAgentServers.map(agentLaunchKey));
            const discovery = await discoverAgents({ cwd: config.cwd, excludedLaunchKeys });
            logAgentDiscoveryResults("refresh", discovery.results);
            discoveredAgentServers = discovery.agentServers;

            const nextServers = availableAgentServers();
            const requestedAgent = nextServers.find(({ name }) => name === config.requestedActiveAgentName);
            const currentAgent = nextServers.find(({ name }) => name === activeAgentServer.name);
            const nextAgent = requestedAgent ?? currentAgent ?? config.agentServer;
            if (nextAgent && (nextAgent.name !== activeAgentServer.name || !sameAgentLaunch(nextAgent, activeAgentServer))) {
                await reloadRuntime({ activeAgentName: nextAgent.name, broadcast: false });
                runtimeChanged = true;
            } else {
                state.agentServers = nextServers;
            }

            console.log(`ACP agent refresh completed: available=${discoveredAgentServers.map(({ name }) => name).join(", ") || "none"}`);
            console.log(`ACP agent refresh available configs: ${JSON.stringify(discoveredAgentServers.map(agentConfigForLog))}`);
            return { ok: true, runtimeChanged };
        } catch (error) {
            discoveredAgentServers = previousDiscovered;
            restoreRuntimeConfig(previousConfig);
            permissionService.updateTimeout(config.permissionRequestTimeoutMs);
            state.agentServers = availableAgentServers();
            throw error;
        } finally {
            state.agentDiscoveryLoading = false;
            state.agentServers = availableAgentServers();
            eventBus.broadcast({ type: "agent_discovery_completed", runtimeChanged });
        }
    };
    agentRefreshPromise = (async () => {
        try {
            return await runRefresh();
        } finally {
            agentRefreshPromise = undefined;
        }
    })();
    return agentRefreshPromise;
};

const isAgentRuntimeBusy = () => [...(state.sessionContexts?.values?.() ?? [])].some((context) => context?.pendingPrompt)
    || [...(state.pendingPermissions?.values?.() ?? [])].some((request) => request?.state === "pending");

const agentService = {
    list() {
        return {
            activeAgentName: activeAgentServer.name,
            agentServers: availableAgentServers().map(({ name }) => ({ name })),
            discoveryLoading: state.agentDiscoveryLoading,
        };
    },

    async switchAgent(name) {
        if (state.agentDiscoveryLoading) {
            return errorResult(
                "agent_discovery_in_progress",
                "Wait for Agent discovery to finish before switching Agents",
                409,
            );
        }
        const nextAgentServer = availableAgentServers().find((server) => server.name === String(name ?? "").trim());
        if (!nextAgentServer) {
            return errorResult(
                "agent_unavailable",
                `Agent '${String(name ?? "").trim()}' is not available in the current configuration or discovery results`,
                400,
            );
        }
        if (nextAgentServer.name === activeAgentServer.name) {
            return { ok: true, ...this.list() };
        }

        try {
            await reloadRuntime({ activeAgentName: nextAgentServer.name, persistActiveAgent: true });
            return { ok: true, ...this.list() };
        } catch (error) {
            return errorResult(
                "agent_switch_failed",
                `Failed to start Agent '${nextAgentServer.name}'`,
                502,
                { cause: errorCause(error) },
            );
        }
    },

    async refreshAgents() {
        if (isAgentRuntimeBusy()) {
            return errorResult(
                "agent_busy",
                "Agent discovery cannot run while a message or permission request is pending",
                409,
            );
        }
        try {
            return await refreshDiscoveredAgents();
        } catch (error) {
            return errorResult(
                "agent_discovery_failed",
                "Automatic Agent discovery did not complete successfully",
                500,
                { cause: errorCause(error) },
            );
        }
    },
};

const agentConfigService = createAgentConfigService({
    rootDir: config.rootDir,
    state,
    beforeReload: async (snapshot) => {
        const previousDiscovered = discoveredAgentServers;
        const configuredKeys = new Set(snapshot.agentServers.map(agentLaunchKey));
        discoveredAgentServers = discoveredAgentServers.filter((agent) => !configuredKeys.has(agentLaunchKey(agent)));
        return () => {
            discoveredAgentServers = previousDiscovered;
        };
    },
    reloadRuntime: async (snapshot) => reloadRuntime({ activeAgentName: snapshot.activeAgentName, reloadFromDisk: true }),
});

const server = createApp({
    agentService,
    eventBus,
    chatService,
    sessionService,
    state,
    permissionService,
    agentConfigService,
    pageContextService,
    frontendCommandService,
    capabilityToken: config.capabilityToken,
    allowedOrigins: config.allowedOrigins,
});

let shutdownPromise;
const shutdown = (exitCode = 0) => {
    if (shutdownPromise) return shutdownPromise;
    shutdownPromise = (async () => {
        frontendCommandService.dispose();
        eventBus.close();
        await new Promise((done) => {
            if (!server.listening) return done();
            server.close(() => done());
        });
        server.closeAllConnections?.();
        await activeAcpClient?.disconnect?.().catch((error) => {
            console.warn(`Failed to disconnect ACP adapter: ${error.message}`);
        });
        process.exitCode = exitCode;
    })();
    return shutdownPromise;
};

server.listen(config.port, config.host, () => {
    console.log(`ACP web extracted API: http://${config.host}:${config.port}/`);
    console.log(`Agent: ${config.activeAgentName} (${config.agentServer.command} ${config.agentServer.args.join(" ")})`);
});

server.on("error", (error) => {
    console.error(`Failed to start HTTP server: ${error.message}`);
    void shutdown(1);
});

if (autoDiscoveryEnabled) {
    try {
        const excludedLaunchKeys = new Set(config.configuredAgentServers.map(agentLaunchKey));
        const discovery = await discoverAgents({ cwd: config.cwd, excludedLaunchKeys });
        logAgentDiscoveryResults("discovery", discovery.results);
        discoveredAgentServers = discovery.agentServers;
        state.agentServers = availableAgentServers();
        const discoveredActive = discoveredAgentServers.find(({ name }) => name === config.requestedActiveAgentName);
        if (discoveredActive && !sameAgentLaunch(discoveredActive, activeAgentServer)) {
            await reloadRuntime({ activeAgentName: discoveredActive.name, broadcast: false });
        } else {
            await chatService.initialize({ broadcast: false });
        }
        console.log(`ACP agent discovery completed: available=${discoveredAgentServers.map(({ name }) => name).join(", ") || "none"}`);
        console.log(`ACP agent discovery available configs: ${JSON.stringify(discoveredAgentServers.map(agentConfigForLog))}`);
    } catch (error) {
        console.warn(`ACP agent discovery failed: ${error.message}`);
        await chatService.initialize({ broadcast: false });
    } finally {
        state.agentDiscoveryLoading = false;
        state.agentServers = availableAgentServers();
        eventBus.broadcast({ type: "agent_discovery_completed", runtimeChanged: true });
    }
} else {
    await chatService.initialize();
}

for (const signal of ["SIGINT", "SIGTERM"]) {
    process.once(signal, () => void shutdown(0));
}

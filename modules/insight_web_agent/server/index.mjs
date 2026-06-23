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
import { createApp } from "./app.mjs";
import { config, saveActiveAgent } from "./config/index.mjs";
import { createAcpClient } from "./infrastructure/acpClient.mjs";
import { createChatService } from "./services/chatService.mjs";
import { createSessionService } from "./services/sessionService.mjs";
import { createSkillService } from "./services/skillService.mjs";
import { createEventBus } from "./state/eventBus.mjs";
import { createRuntimeState, publicState, resetRuntimeForAgent } from "./state/runtimeState.mjs";

const createActiveAcpClient = (agentServer) => {
    const agentWorkspacePath = join(config.cwd, agentServer.name);

    return createAcpClient({
        agentServer,
        cwd: agentWorkspacePath,
        debug: config.debug,
        onNotification: (message) => chatService?.handleAcpNotification(message),
    });
};

await mkdir(config.cwd, { recursive: true });
await mkdir(join(config.cwd, config.agentServer.name), { recursive: true });

const state = createRuntimeState();
const eventBus = createEventBus(state);
const skillService = createSkillService({ rootDir: config.rootDir });
let chatService;
let activeAgentServer = config.agentServer;
let activeAcpClient = createActiveAcpClient(activeAgentServer);
resetRuntimeForAgent(state, { agentServers: config.agentServers, activeAgentName: activeAgentServer.name });

const acpClient = {
    request(method, params) {
        return activeAcpClient.request(method, params);
    },
};
const sessionService = createSessionService({
    acpClient,
    config,
    eventBus,
    state,
});
chatService = createChatService({
    acpClient,
    eventBus,
    sessionService,
    skillService,
    state,
});

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

        activeAcpClient.dispose?.();
        activeAgentServer = nextAgentServer;
        try {
            saveActiveAgent(activeAgentServer.name);
        } catch (error) {
            console.warn(`Failed to save active agent: ${error.message}`);
        }
        await mkdir(join(config.cwd, activeAgentServer.name), { recursive: true });
        activeAcpClient = createActiveAcpClient(activeAgentServer);
        resetRuntimeForAgent(state, { agentServers: config.agentServers, activeAgentName: activeAgentServer.name });
        await chatService.initialize();
        eventBus.broadcast({ type: "state", state: publicState(state) });
        return { ok: true, ...this.list() };
    },
};

await chatService.initialize();

const server = createApp({ agentService, eventBus, chatService, sessionService, state });

server.listen(config.port, "127.0.0.1", () => {
    console.log(`ACP web extracted API: http://127.0.0.1:${config.port}/`);
    console.log(`Agent: ${config.activeAgentName} (${config.agentServer.command} ${config.agentServer.args.join(" ")})`);
});

server.on("error", (error) => {
    console.error(`Failed to start HTTP server: ${error.message}`);
    process.exitCode = 1;
});

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
import { join } from "node:path";
import { createPromptContent } from "./chatService.mjs";
import { getSessionContext } from "../state/runtimeState.mjs";

export const createSessionManager = ({ adapter, eventBus, state, config, contextAssembler, auditLogger, permissionService }) => {
    let sessionService;

    const getAgentCwd = () => join(config.cwd, state.activeAgentName ?? config.agentServer?.name ?? adapter.agentId ?? "");

    const cloneSessionContext = (context) => ({
        ...context,
        grants: new Set(context.grants ?? []),
        messages: [...(context.messages ?? [])],
        configOptions: [...(context.configOptions ?? [])],
    });

    const refreshHiddenContext = async (context) => {
        if (!contextAssembler || !context) return context?.hiddenContext;
        const packet = await contextAssembler.assemble(context);
        context.hiddenContext = { kind: "AgentContextPacket", packet };
        void auditLogger?.contextAssembled?.(context.sessionId, packet);
        return context.hiddenContext;
    };

    const applyActiveContext = async (context, activeContext = state.activeContext) => {
        if (!context) return;
        context.view = activeContext?.activeModule;
        context.profileId = activeContext?.profileId;
        await refreshHiddenContext(context);
    };

    const startSession = async ({ agentId, mode, view, profileId, grants } = {}) => {
        const session = await adapter.request("session/new", {
            cwd: getAgentCwd(),
            additionalDirectories: [],
            mcpServers: [],
        });
        const context = getSessionContext(state, session.sessionId);
        context.agentId = agentId ?? adapter.agentId ?? state.activeAgentName;
        context.runtime = adapter.runtime ?? "stdio";
        context.mode = mode ?? context.mode ?? "free_chat";
        context.view = view ?? state.activeContext?.activeModule;
        context.profileId = profileId ?? state.activeContext?.profileId;
        context.grants = grants instanceof Set ? new Set(grants) : new Set(grants ?? []);
        context.messages = context.messages ?? [];
        context.pendingPrompt = false;
        context.configOptions = session?.configOptions ?? session?.config_options ?? [];
        context.replayingHistory = false;
        context.createdAt = context.createdAt ?? Date.now();
        await refreshHiddenContext(context);
        state.sessionContexts.set(session.sessionId, context);
        void auditLogger?.sessionStart?.(context);
        return cloneSessionContext(context);
    };

    const endSession = async (sessionId) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) return { error: "sessionId is required", status: 400 };
        const context = state.sessionContexts.get(targetSessionId);
        if (context?.pendingPrompt) {
            try {
                await adapter.request("session/cancel", { sessionId: targetSessionId });
            } catch (error) {
                console.warn(`[ACP] session/cancel failed for ${targetSessionId}: ${error.message}`);
            }
        }
        try {
            await adapter.request("session/delete", { sessionId: targetSessionId });
        } catch (error) {
            console.warn(`[ACP] session/delete failed for ${targetSessionId}: ${error.message}`);
            if (!error.message.includes("not found")) {
                void auditLogger?.error?.(targetSessionId, error);
                return { error: error.message, status: 500 };
            }
        }
        permissionService?.rejectSessionRequests?.(targetSessionId, "invalidated");
        state.localTitles.delete(targetSessionId);
        state.sessionContexts.delete(targetSessionId);
        void auditLogger?.sessionEnd?.(targetSessionId);
        return { ok: true };
    };

    const pushUserMessage = async (sessionId, { text, images = [], hiddenContext, mode } = {}) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) throw new Error("sessionId is required");
        const context = getSessionContext(state, targetSessionId);
        if (context.pendingPrompt) throw new Error("another prompt is running");
        if (mode) context.mode = mode;
        const contextForPrompt = hiddenContext ?? await refreshHiddenContext(context);
        const prompt = createPromptContent(String(text ?? "").trim(), images, [], contextForPrompt);
        context.pendingPrompt = true;
        try {
            await adapter.request("session/prompt", { sessionId: targetSessionId, prompt });
        } catch (error) {
            void auditLogger?.error?.(targetSessionId, error);
            throw error;
        } finally {
            context.pendingPrompt = false;
        }
        return { ok: true };
    };

    const handleAgentEvent = async (sessionId, event) => {
        if (eventBus?.broadcast && event?.audit) eventBus.broadcast({ type: "session_event", sessionId, event });
    };

    const updateContext = async (activeContext) => {
        state.activeContext = activeContext;
        await Promise.all([...state.sessionContexts.values()].map((context) => applyActiveContext(context, activeContext)));
        return state.activeContext;
    };

    const getPromptContext = async (sessionId) => {
        return refreshHiddenContext(getSessionContext(state, sessionId));
    };

    return {
        startSession,
        endSession,
        pushUserMessage,
        handleAgentEvent,
        updateContext,
        getPromptContext,
        bindSessionService(service) {
            sessionService = service;
        },
        listSessions: (...args) => sessionService.listSessions(...args),
        loadSessionById: (...args) => sessionService.loadSessionById(...args),
        deleteSessionById: (...args) => sessionService.deleteSessionById(...args),
        setModel: (...args) => sessionService.setModel(...args),
        setMode: (...args) => sessionService.setMode(...args),
    };
};

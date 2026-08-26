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
import { errorCause, errorResult } from "./errorResult.mjs";

export const createSessionManager = ({ adapter, eventBus, state, config, auditLogger, permissionService, capabilitySessionIntegration }) => {
    let sessionService;

    const getAgentCwd = () => join(config.cwd, state.activeAgentWorkspaceKey ?? state.activeAgentName ?? config.agentServer?.name ?? adapter.agentId ?? "");

    const cloneSessionContext = (context) => ({
        ...context,
        grants: new Set(context.grants ?? []),
        messages: [...(context.messages ?? [])],
        configOptions: [...(context.configOptions ?? [])],
    });

    const startSession = async ({ agentId, mode, view, profileId, grants } = {}) => {
        // Session Integration 只包装 ACP 参数，真正的 Session 创建仍由原 Agent 完成。
        const create = (mcpServers) => adapter.request("session/new", {
            cwd: getAgentCwd(),
            additionalDirectories: [],
            mcpServers,
        });
        const session = capabilitySessionIntegration?.withMcpServers
            ? await capabilitySessionIntegration.withMcpServers(create)
            : await create([]);
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
        state.sessionContexts.set(session.sessionId, context);
        auditLogger?.sessionStart?.(context);
        return cloneSessionContext(context);
    };

    const endSession = async (sessionId) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) {
            return errorResult("session_id_required", "sessionId is required to end a session", 400);
        }
        const context = state.sessionContexts.get(targetSessionId);
        permissionService?.rejectSessionRequests?.(targetSessionId, "invalidated", true);
        if (context?.pendingPrompt) {
            try {
                if (adapter.notify) adapter.notify("session/cancel", { sessionId: targetSessionId });
                else await adapter.request("session/cancel", { sessionId: targetSessionId });
            } catch (error) {
                console.warn(`[ACP] session/cancel failed for ${targetSessionId}: ${error.message}`);
            }
        }
        try {
            await adapter.request("session/delete", { sessionId: targetSessionId });
        } catch (error) {
            const cause = errorCause(error);
            console.warn(`[ACP] session/delete failed for ${targetSessionId}: ${cause}`);
            if (!cause.includes("not found")) {
                auditLogger?.error?.(targetSessionId, error);
                return errorResult(
                    "session_delete_failed",
                    `Session '${targetSessionId}' could not be deleted from the selected Agent`,
                    502,
                    { cause },
                );
            }
        }
        state.localTitles.delete(targetSessionId);
        state.sessionContexts.delete(targetSessionId);
        auditLogger?.sessionEnd?.(targetSessionId);
        return { ok: true };
    };

    const pushUserMessage = async (sessionId, { text, images = [], mode } = {}) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) throw new Error("sessionId is required");
        const context = getSessionContext(state, targetSessionId);
        if (context.pendingPrompt) throw new Error("another prompt is running");
        if (mode) context.mode = mode;
        const prompt = createPromptContent(String(text ?? "").trim(), images);
        context.pendingPrompt = true;
        try {
            await adapter.request("session/prompt", { sessionId: targetSessionId, prompt });
        } catch (error) {
            auditLogger?.error?.(targetSessionId, error);
            throw error;
        } finally {
            context.pendingPrompt = false;
        }
        return { ok: true };
    };

    const handleAgentEvent = async (sessionId, event) => {
        if (eventBus?.broadcast && event?.audit) eventBus.broadcast({ type: "session_event", sessionId, event });
    };

    return {
        startSession,
        endSession,
        pushUserMessage,
        handleAgentEvent,
        bindSessionService(service) {
            sessionService = service;
        },
        listSessions: (...args) => sessionService.listSessions(...args),
        loadSessionById: (...args) => sessionService.loadSessionById(...args),
        deleteSessionById: (...args) => sessionService.deleteSessionById(...args),
        setConfigOption: (...args) => sessionService.setConfigOption(...args),
        setModel: (...args) => sessionService.setModel(...args),
        setMode: (...args) => sessionService.setMode(...args),
    };
};

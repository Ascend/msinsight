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
import { publicState } from "../state/runtimeState.mjs";
import { supportsSessionDelete, supportsSessionList, supportsSessionLoad, supportsSessionResume, supportsSetConfigOption } from "./capabilityService.mjs";
import { getModelConfig, normalizeModelValue, setConfigOptionCurrentValue, setConfigOptions } from "./configOptionService.mjs";

export const createSessionService = ({ acpClient, config, eventBus, state, sessionManager }) => {
    let mutationQueue = Promise.resolve();

    const getAgentCwd = () => join(config.cwd, state.activeAgentName);
    const manager = sessionManager ?? {
        async startSession() {
            const session = await acpClient.request("session/new", {
                cwd: getAgentCwd(),
                additionalDirectories: [],
                mcpServers: [],
            });
            const context = getOrCreateSessionContext(session.sessionId);
            context.agentId = acpClient.agentId ?? state.activeAgentName;
            context.runtime = acpClient.runtime ?? context.runtime;
            context.configOptions = session?.configOptions ?? [];
            return context;
        },
        async endSession(sessionId) {
            await acpClient.request("session/delete", { sessionId });
            state.localTitles.delete(sessionId);
            state.sessionContexts.delete(sessionId);
            return { ok: true };
        },
    };

    const broadcastState = () => {
        eventBus.broadcast({ type: "state", state: publicState(state) });
    };

    const enqueueMutation = async (operation) => {
        const next = mutationQueue.then(operation);
        mutationQueue = next.catch(() => {});
        return next;
    };

    const listSessions = async () => {
        if (!supportsSessionList(state)) {
            // If the agent doesn't support session list (like dcode),
            // just return the locally generated sessions
            state.sessions = localSessions();
            console.log(`Session list uses local sessions: count=${state.sessions.length}`);
            return state.sessions;
        }

        console.log(`Listing remote sessions: cwd=${getAgentCwd()}`);
        const response = await acpClient.request("session/list", { cwd: getAgentCwd() });
        const remoteSessions = response.sessions ?? [];
        const remoteIds = new Set(remoteSessions.map((session) => session.sessionId));
        state.sessions = [
            ...remoteSessions.map((session) => ({
                ...session,
                title: state.localTitles.get(session.sessionId) ?? session.title,
                pendingPrompt: state.sessionContexts.get(session.sessionId)?.pendingPrompt ?? false,
            })),
            ...localSessions().filter((session) => !remoteIds.has(session.sessionId)),
        ];
        console.log(`Session list completed: remote=${remoteSessions.length}, total=${state.sessions.length}`);
        return state.sessions;
    };

    const refreshSessions = async () => {
        try {
            await listSessions();
        } catch (error) {
            console.error(`Failed to list sessions: ${error.message}`);
        }
    };

    const loadConfigOptions = async () => {
        if (state.configOptions.length) return state.configOptions;

        try {
            console.log("Loading config options with temporary session");
            const session = await acpClient.request("session/new", {
                cwd: getAgentCwd(),
                additionalDirectories: [],
                mcpServers: [],
            });
            setConfigOptions({ eventBus, state }, session?.configOptions ?? [], null);

            if (session?.sessionId) {
                console.log(`Cleaning up temporary config session: sessionId=${session.sessionId}`);
                await cleanupSession(session.sessionId);
                await refreshSessions();
            }
        } catch (error) {
            console.warn(`Failed to load config options: ${error.message}`);
        }

        return state.configOptions;
    };

    const createSessionContext = async ({ messages, agentId, mode, view, profileId, grants }) => {
        console.log(`Creating ACP session: cwd=${getAgentCwd()}, initialMessages=${messages.length}`);
        const context = await manager.startSession({ agentId, mode, view, profileId, grants });
        const sessionContext = getOrCreateSessionContext(context.sessionId);
        sessionContext.messages = messages;
        setConfigOptions({ eventBus, state }, sessionContext.configOptions, context.sessionId);
        await applyPreferredModel(context.sessionId);
        console.log(`ACP session created: sessionId=${context.sessionId}, configOptions=${sessionContext.configOptions.length}`);
        return context.sessionId;
    };

    const createEmptySession = async () => {
        try {
            const sessionId = await createSessionContext({ messages: [] });
            await refreshSessions();
            broadcastState();
            return {
                ok: true,
                sessionId,
                messages: [],
                pendingPrompt: false,
                configOptions: state.sessionContexts.get(sessionId)?.configOptions ?? [],
            };
        } catch (error) {
            console.error(`Create empty session failed: ${error.message}`);
            return { error: error.message, status: 500 };
        }
    };

    const loadSessionById = async (sessionId) => {
        const targetSessionId = String(sessionId ?? "").trim();
        if (!targetSessionId) {
            console.warn("Load session rejected: sessionId is required");
            return { error: "sessionId is required", status: 400 };
        }

        console.log(`Loading session context: sessionId=${targetSessionId}`);
        const context = getOrCreateSessionContext(targetSessionId);
        context.messages = [];
        context.replayingHistory = true;
        state.sessionContexts.set(targetSessionId, context);

        const response = await openExistingSession(targetSessionId)
            .finally(() => {
                context.replayingHistory = false;
            });
        const configOptions = response?.configOptions ?? [];
        setConfigOptions({ eventBus, state }, configOptions, targetSessionId);
        console.log(`Session loaded: sessionId=${targetSessionId}, messages=${context.messages.length}, configOptions=${configOptions.length}`);

        return {
            ok: true,
            sessionId: targetSessionId,
            messages: context.messages,
            pendingPrompt: context.pendingPrompt,
            configOptions,
        };
    };

    const getOrCreateSessionContext = (sessionId) => {
        const context = state.sessionContexts.get(sessionId) ?? {
            sessionId,
            agentId: undefined,
            runtime: "stdio",
            mode: "free_chat",
            view: undefined,
            profileId: undefined,
            grants: new Set(),
            messages: [],
            pendingPrompt: false,
            configOptions: [],
            replayingHistory: false,
            createdAt: Date.now(),
        };
        state.sessionContexts.set(sessionId, context);
        return context;
    };

    const deleteSessionById = async (sessionId) => {
        return enqueueMutation(async () => {
            const targetSessionId = String(sessionId ?? "").trim();
            if (!targetSessionId) {
                console.warn("Delete session rejected: sessionId is required");
                return { error: "sessionId is required", status: 400 };
            }
            const targetContext = state.sessionContexts.get(targetSessionId);
            if (targetContext?.pendingPrompt) {
                console.warn(`Delete session rejected: pending prompt, sessionId=${targetSessionId}`);
                return { error: "cannot delete a session while prompting", status: 409 };
            }
            if (!supportsSessionDelete(state)) {
                console.warn("Delete session rejected: unsupported by agent");
                return { error: "delete session is not supported by this agent", status: 409 };
            }

            console.log(`Deleting ACP session: sessionId=${targetSessionId}`);
            const result = await manager.endSession(targetSessionId);
            if (result.error) return result;
            await refreshSessions();
            console.log(`Session deleted locally: sessionId=${targetSessionId}`);
            return result;
        });
    };

    const openExistingSession = (sessionId) => {
        if (supportsSessionLoad(state)) {
            return acpClient.request("session/load", {
                sessionId,
                cwd: getAgentCwd(),
                additionalDirectories: [],
                mcpServers: [],
            });
        }
        if (supportsSessionResume(state)) return resumeSession(sessionId);
        throw new Error("load session is not supported by this agent");
    };

    const resumeSession = (sessionId) => acpClient.request("session/resume", {
        sessionId,
        cwd: getAgentCwd(),
        additionalDirectories: [],
        mcpServers: [],
    });

    const setSessionConfigOption = async (configId, value, targetSessionId) => {
        return enqueueMutation(async () => {
            const sessionId = String(targetSessionId ?? "").trim();
            if (!sessionId) {
                console.warn(`Set config option rejected: configId=${configId}, sessionId is required`);
                return { error: "sessionId is required", status: 400 };
            }

            try {
                console.log(`Setting session config option: sessionId=${sessionId}, configId=${configId}`);
                const result = await acpClient.request("session/set_config_option", {
                    sessionId,
                    configId,
                    value,
                });
                setConfigOptions({ eventBus, state }, result?.configOptions ?? result?.config_options ?? [], sessionId);
                broadcastState();
                console.log(`Session config option set: sessionId=${sessionId}, configId=${configId}`);
                return { ok: true, configOptions: state.sessionContexts.get(sessionId)?.configOptions ?? state.configOptions };
            } catch (error) {
                if (error.message.includes("Method not found")) {
                    console.warn(`Set config option unsupported: sessionId=${sessionId}, configId=${configId}`);
                    return { error: "config options are not supported by this agent", status: 409 };
                }
                console.error(`Set config option failed: sessionId=${sessionId}, configId=${configId}, error=${error.message}`);
                return { error: error.message, status: 500 };
            }
        });
    };

    const setModel = async (model, sessionId) => {
        const targetSessionId = String(sessionId ?? "").trim() || undefined;
        const modelConfig = getModelConfig(state, targetSessionId);
        if (!modelConfig) {
            console.warn(`Set model rejected: model config unavailable, sessionId=${targetSessionId ?? ""}`);
            return { error: "model config option is unavailable", status: 409 };
        }
        const value = normalizeModelValue(modelConfig, model);
        if (!value) {
            console.warn(`Set model rejected: unavailable model=${String(model ?? "")}`);
            return { error: `model is unavailable: ${model}`, status: 400 };
        }
        state.preferredModel = value;
        if (!targetSessionId) {
            setConfigOptionCurrentValue({ eventBus, state }, modelConfig.id, value, null);
            broadcastState();
            console.log(`Preferred model set globally: model=${value}`);
            return { ok: true, configOptions: state.configOptions };
        }
        return setSessionConfigOption(modelConfig.id, value, targetSessionId);
    };

    const setMode = async (mode, sessionId) => {
        const targetSessionId = String(sessionId ?? "").trim() || undefined;
        const targetMode = String(mode ?? "").trim();
        if (!targetMode) {
            console.warn("Set mode rejected: mode is required");
            return { error: "mode is required", status: 400 };
        }
        if (!targetSessionId) {
            console.warn("Set mode rejected: sessionId is required");
            return { error: "sessionId is required", status: 400 };
        }
        const configOptions = state.sessionContexts.get(targetSessionId)?.configOptions ?? state.configOptions;
        const modeConfig = getModeConfigFromOptions(configOptions);
        if (!modeConfig) {
            console.warn(`Set mode rejected: mode config unavailable, sessionId=${targetSessionId}`);
            return { error: "mode config option is unavailable", status: 409 };
        }

        return setSessionConfigOption(modeConfig.id, targetMode, targetSessionId);
    };

    const applyPreferredModel = async (sessionId) => {
        const modelConfig = getModelConfig(state, sessionId);
        const preferredModel = state.preferredModel ?? config.defaultModel;
        if (!modelConfig || !preferredModel) return;
        const value = normalizeModelValue(modelConfig, preferredModel);
        if (!value || value === modelConfig.currentValue) return;

        try {
            await setSessionConfigOption(modelConfig.id, value, sessionId);
        } catch (_err) {
            // Ignore failure silently for implicit application
        }
    };

    const getModeConfigFromOptions = (configOptions = []) => {
        return configOptions.find((option) => option?.category === "mode")
            ?? configOptions.find((option) => String(option?.id ?? "").toLowerCase().includes("mode"));
    };

    const localSessions = () => {
        return [...state.sessionContexts.values()]
            .filter((context) => context.agentId === state.activeAgentName)
            .map((context) => ({
                sessionId: context.sessionId,
                title: state.localTitles.get(context.sessionId) ?? "New session",
                pendingPrompt: context.pendingPrompt,
            }));
    };

    const cleanupSession = async (sessionId) => {
        if (!supportsSessionDelete(state)) return;
        await acpClient.request("session/delete", { sessionId });
    };

    sessionManager?.bindSessionService?.({
        deleteSessionById,
        listSessions,
        loadSessionById,
        setMode,
        setModel,
    });

    return {
        applyPreferredModel,
        broadcastState,
        createEmptySession,
        createSessionContext,
        deleteSessionById,
        listSessions,
        loadSessionById,
        loadConfigOptions,
        refreshSessions,
        resumeSession,
        setConfigOptions: (configOptions, sessionId) => setConfigOptions({ eventBus, state }, configOptions, sessionId),
        setMode,
        setModel,
    };
};

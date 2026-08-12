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
import { publicCapabilities } from "../services/capabilityService.mjs";

export const createRuntimeState = () => ({
    initialized: false,
    agentDiscoveryLoading: false,
    sessions: [],
    sessionContexts: new Map(),
    localTitles: new Map(),
    configOptions: [],
    preferredModel: undefined,
    clients: new Set(),
    agentServers: [],
    activeAgentName: undefined,
    activeAgentWorkspaceKey: undefined,
    agentInfo: undefined,
    agentError: undefined,
    agentCapabilities: undefined,
    availableCommands: [],
    availableSkills: [],
    activeContext: undefined,
    permissionRuntimeAllowlist: new Map(),
    pendingPermissions: new Map(),
    resolvedPermissions: new Map(),
});

export const getSessionContext = (state, sessionId) => {
    if (!sessionId) return undefined;
    let context = state.sessionContexts.get(sessionId);
    if (!context) {
        context = {
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
    }
    return context;
};

export const publicState = (state) => ({
    initialized: state.initialized,
    configOptions: state.configOptions,
    activeAgentName: state.activeAgentName,
    agentInfo: state.agentInfo,
    agentError: state.agentError,
    agentCapabilities: publicCapabilities(state),
    availableCommands: state.availableCommands,
    availableSkills: state.availableSkills.map(({ name, description }) => ({ name, description })),
    activeContext: state.activeContext,
});

export const snapshotRuntimeState = (state) => ({
    initialized: state.initialized,
    agentDiscoveryLoading: state.agentDiscoveryLoading,
    sessions: [...state.sessions],
    sessionContexts: cloneSessionContexts(state.sessionContexts),
    localTitles: new Map(state.localTitles),
    configOptions: [...state.configOptions],
    preferredModel: state.preferredModel,
    agentServers: [...state.agentServers],
    activeAgentName: state.activeAgentName,
    activeAgentWorkspaceKey: state.activeAgentWorkspaceKey,
    agentInfo: state.agentInfo,
    agentError: state.agentError,
    agentCapabilities: state.agentCapabilities,
    availableCommands: [...state.availableCommands],
    availableSkills: [...state.availableSkills],
    activeContext: state.activeContext,
    permissionRuntimeAllowlist: cloneMapOfSets(state.permissionRuntimeAllowlist),
    pendingPermissions: new Map(state.pendingPermissions),
    resolvedPermissions: new Map(state.resolvedPermissions),
});

export const restoreRuntimeState = (state, snapshot) => {
    cancelOverwrittenPendingPermissions(state.pendingPermissions, snapshot.pendingPermissions);
    state.initialized = snapshot.initialized;
    state.agentDiscoveryLoading = snapshot.agentDiscoveryLoading;
    state.sessions = snapshot.sessions;
    state.sessionContexts = snapshot.sessionContexts;
    state.localTitles = snapshot.localTitles;
    state.configOptions = snapshot.configOptions;
    state.preferredModel = snapshot.preferredModel;
    state.agentServers = snapshot.agentServers;
    state.activeAgentName = snapshot.activeAgentName;
    state.activeAgentWorkspaceKey = snapshot.activeAgentWorkspaceKey;
    state.agentInfo = snapshot.agentInfo;
    state.agentError = snapshot.agentError;
    state.agentCapabilities = snapshot.agentCapabilities;
    state.availableCommands = snapshot.availableCommands;
    state.availableSkills = snapshot.availableSkills;
    state.activeContext = snapshot.activeContext;
    state.permissionRuntimeAllowlist = snapshot.permissionRuntimeAllowlist;
    state.pendingPermissions = snapshot.pendingPermissions;
    state.resolvedPermissions = snapshot.resolvedPermissions;
};

export const resetRuntimeForAgent = (state, { agentServers, activeAgentName, activeAgentWorkspaceKey }) => {
    state.initialized = false;
    state.sessions = [];
    state.sessionContexts = new Map();
    state.localTitles = new Map();
    state.configOptions = [];
    state.preferredModel = undefined;
    state.agentServers = agentServers;
    state.activeAgentName = activeAgentName;
    state.activeAgentWorkspaceKey = activeAgentWorkspaceKey;
    state.agentInfo = undefined;
    state.agentError = undefined;
    state.agentCapabilities = undefined;
    state.availableCommands = [];
    state.availableSkills = [];
    state.activeContext = undefined;
    state.permissionRuntimeAllowlist = new Map();
    state.pendingPermissions = new Map();
    state.resolvedPermissions = new Map();
};

const cloneSessionContexts = (contexts) => new Map([...contexts].map(([sessionId, context]) => [sessionId, {
    ...context,
    grants: new Set(context.grants ?? []),
    messages: [...(context.messages ?? [])],
    configOptions: [...(context.configOptions ?? [])],
}]));

const cloneMapOfSets = (map) => new Map([...map].map(([key, value]) => [key, new Set(value)]));

const cancelOverwrittenPendingPermissions = (currentPending = new Map(), restoredPending = new Map()) => {
    for (const [key, request] of currentPending) {
        if (restoredPending.has(key)) continue;
        if (request?.state !== "pending") continue;
        clearTimeout(request.timeout);
        request.state = "invalidated";
        request.resolvedAt = Date.now();
        request.resolve?.({ allowed: false, state: "invalidated", reason: "invalidated" });
    }
};

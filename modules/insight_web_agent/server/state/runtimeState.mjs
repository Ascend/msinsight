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
    sessions: [],
    sessionContexts: new Map(),
    localTitles: new Map(),
    configOptions: [],
    preferredModel: undefined,
    clients: new Set(),
    agentServers: [],
    activeAgentName: undefined,
    agentInfo: undefined,
    agentError: undefined,
    agentCapabilities: undefined,
    availableCommands: [],
    availableSkills: [],
});

export const getSessionContext = (state, sessionId) => {
    if (!sessionId) return undefined;
    let context = state.sessionContexts.get(sessionId);
    if (!context) {
        context = { sessionId, messages: [], pendingPrompt: false, configOptions: [], hiddenContext: undefined };
        state.sessionContexts.set(sessionId, context);
    }
    return context;
};

export const publicState = (state) => ({
    initialized: state.initialized,
    configOptions: state.configOptions,
    agentServers: state.agentServers.map(({ name }) => ({ name })),
    activeAgentName: state.activeAgentName,
    agentInfo: state.agentInfo,
    agentError: state.agentError,
    agentCapabilities: publicCapabilities(state),
    availableCommands: state.availableCommands,
    availableSkills: state.availableSkills.map(({ name, description }) => ({ name, description })),
});

export const resetRuntimeForAgent = (state, { agentServers, activeAgentName }) => {
    state.initialized = false;
    state.sessions = [];
    state.sessionContexts = new Map();
    state.localTitles = new Map();
    state.configOptions = [];
    state.preferredModel = undefined;
    state.agentServers = agentServers;
    state.activeAgentName = activeAgentName;
    state.agentInfo = undefined;
    state.agentError = undefined;
    state.agentCapabilities = undefined;
    state.availableCommands = [];
    state.availableSkills = [];
};

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
const pick = (value, ...keys) => {
    for (const key of keys) {
        if (value?.[key] !== undefined) return value[key];
    }
    return undefined;
};

const hasCapability = (value) => value !== undefined && value !== null && value !== false;

export const normalizeAgentCapabilities = (capabilities = {}) => ({
    loadSession: Boolean(pick(capabilities, "loadSession", "load_session")),
    prompt: pick(capabilities, "promptCapabilities", "prompt_capabilities") ?? {},
    session: pick(capabilities, "sessionCapabilities", "session_capabilities") ?? {},
    auth: pick(capabilities, "auth") ?? {},
});

export const setAgentCapabilities = (state, capabilities) => {
    state.agentCapabilities = normalizeAgentCapabilities(capabilities);
};

export const supportsSessionList = (state) => hasCapability(state.agentCapabilities?.session?.list);

export const supportsSessionDelete = (state) => hasCapability(state.agentCapabilities?.session?.delete);

export const supportsSessionLoad = (state) => Boolean(state.agentCapabilities?.loadSession);

export const supportsSessionResume = (state) => hasCapability(state.agentCapabilities?.session?.resume);

export const supportsSessionClose = (state) => hasCapability(state.agentCapabilities?.session?.close);

export const supportsSetConfigOption = (state, sessionId) => {
    if (hasCapability(state.agentCapabilities?.session?.setConfigOption)
        || hasCapability(state.agentCapabilities?.session?.set_config_option)) {
        return true;
    }
    return false;
};

export const publicCapabilities = (state) => ({
    loadSession: supportsSessionLoad(state),
    session: {
        list: supportsSessionList(state),
        delete: supportsSessionDelete(state),
        resume: supportsSessionResume(state),
        close: supportsSessionClose(state),
        setConfigOption: supportsSetConfigOption(state),
    },
});

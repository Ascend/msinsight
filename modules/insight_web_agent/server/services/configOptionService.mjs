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
export const setConfigOptions = ({ eventBus, state }, configOptions = [], sessionId) => {
    if (sessionId) {
        const context = state.sessionContexts.get(sessionId) ?? { sessionId, messages: [], pendingPrompt: false, configOptions: [], hiddenContext: undefined };
        context.configOptions = configOptions;
        state.sessionContexts.set(sessionId, context);
    } else {
        state.configOptions = configOptions;
    }
    eventBus.broadcast({ type: "config_options", sessionId, configOptions });
};

export const setConfigOptionCurrentValue = ({ eventBus, state }, configId, value, sessionId) => {
    const context = sessionId ? state.sessionContexts.get(sessionId) : undefined;
    const configOptions = context?.configOptions?.length ? context.configOptions : state.configOptions;
    const nextConfigOptions = configOptions.map((option) => {
        if (option?.id !== configId) return option;
        return { ...option, currentValue: value };
    });
    if (context) {
        context.configOptions = nextConfigOptions;
    } else {
        state.configOptions = nextConfigOptions;
    }
    eventBus.broadcast({ type: "config_options", sessionId, configOptions: nextConfigOptions });
};

export const getModelConfig = (state, sessionId) => {
    const context = sessionId ? state.sessionContexts.get(sessionId) : undefined;
    const configOptions = context?.configOptions?.length ? context.configOptions : state.configOptions;
    return configOptions.find((option) => option?.category === "model")
        ?? configOptions.find((option) => String(option?.id ?? "").toLowerCase().includes("model"));
};

export const flattenConfigValues = (options = []) => {
    return options.flatMap((option) => {
        if (Array.isArray(option?.options)) return flattenConfigValues(option.options);
        return option?.value ? [option] : [];
    });
};

export const normalizeModelValue = (modelConfig, value) => {
    const target = String(value ?? "").trim();
    if (!target) return undefined;

    const options = flattenConfigValues(modelConfig?.options ?? []);
    return options.find((option) => option.value === target)?.value
        ?? options.find((option) => option.name === target)?.value
        ?? options.find((option) => option.value?.endsWith(`/${target}`))?.value
        ?? options.find((option) => option.name?.endsWith(`/${target}`))?.value;
};

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
export const getNativeConfigurationError = (env) => {
    const provider = String(env.MSINSIGHT_NATIVE_PROVIDER ?? "").trim();
    const missing = (field, label) => ({
        field,
        message: `Native Agent ${label} is not configured. Open Agent Settings and configure Native Agent before sending a message.`,
    });
    if (!provider) return missing("provider", "provider");
    if (!["openai", "anthropic", "openai-compatible"].includes(provider)) {
        return { field: "provider", message: "Native Agent provider is unsupported. Select OpenAI, Anthropic, or OpenAI-compatible in Agent Settings." };
    }
    if (!String(env.MSINSIGHT_NATIVE_API_KEY ?? "").trim()) return missing("apiKey", "API key");
    if (!String(env.MSINSIGHT_NATIVE_MODEL ?? "gpt-4o-mini").trim()) return missing("model", "model");
    if (provider === "openai-compatible" && !String(env.MSINSIGHT_NATIVE_BASE_URL ?? "").trim()) {
        return missing("baseUrl", "base URL");
    }
    return undefined;
};

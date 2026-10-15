/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2
 */

/** One endpoint shares credentials; IDs are the exact provider model names. */
export const normalizeModelCatalog = (config = {}) => {
    const defaultModelId = String(config.model ?? "").trim();
    const input = config.models === undefined
        ? (defaultModelId ? [{ id: defaultModelId, name: defaultModelId }] : [])
        : config.models;
    if (!Array.isArray(input) || (config.models !== undefined && !input.length)) {
        throw new Error("models must be a non-empty array");
    }
    const models = new Map();
    for (const item of input) {
        if (!item || typeof item !== "object" || typeof item.id !== "string" || !item.id.trim()
            || (item.name !== undefined && (typeof item.name !== "string" || !item.name.trim()))) {
            throw new Error("models entries require a non-empty id and optional non-empty name");
        }
        const id = item.id.trim();
        if (!models.has(id)) models.set(id, { id, name: item.name?.trim() || id });
    }
    if (models.size && !models.has(defaultModelId)) throw new Error("default model must belong to models");
    return { defaultModelId, models: [...models.values()] };
};

/** Only missing legacy IDs migrate. Removed/invalid explicit IDs never fall back. */
export const resolveSessionModel = (catalog, session) => {
    const id = session.modelId === undefined ? catalog.defaultModelId : session.modelId;
    if (!catalog.models.some((model) => model.id === id)) throw new Error(`Model is unavailable: ${id}`);
    session.modelId = id;
    return id;
};

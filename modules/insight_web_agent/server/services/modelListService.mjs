/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2
 */
const DEFAULT_BASES = {
    openai: "https://api.openai.com/v1",
    anthropic: "https://api.anthropic.com/v1",
};
const MAX_BYTES = 1024 * 1024;
const MAX_MODELS = 500;
const MAX_PAGES = 5;

export const modelListEndpoint = (provider, baseUrl = "") => {
    if (!["openai", "openai-compatible", "anthropic"].includes(provider)) throw new Error("unsupported");
    const url = new URL(baseUrl.trim() || DEFAULT_BASES[provider]);
    if (!["http:", "https:"].includes(url.protocol) || url.username || url.password || url.search || url.hash) throw new Error("invalid endpoint");
    let path = url.pathname.replace(/\/+$/, "");
    // Anthropic SDK baseURL includes /v1; also accept the official origin without duplicating it.
    if (provider === "anthropic" && !path.endsWith("/v1")) path += "/v1";
    url.pathname = `${path}/models`;
    return url;
};

const readBoundedJson = async (response, budget) => {
    if (!response.body || Number(response.headers.get("content-length")) > budget.remaining) throw new Error("invalid response");
    const reader = response.body.getReader();
    const chunks = [];
    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            budget.remaining -= value.byteLength;
            if (budget.remaining < 0) throw new Error("response too large");
            chunks.push(Buffer.from(value));
        }
        return JSON.parse(Buffer.concat(chunks).toString("utf8"));
    } finally {
        await reader.cancel().catch(() => {});
        reader.releaseLock();
    }
};

const safeText = (value, apiKey) => typeof value === "string" && value.trim() && value.length <= 256
    && !/[\u0000-\u001f\u007f]/.test(value) && !(apiKey && value.includes(apiKey));

/** No retries, redirects, inference probes, or provider-owned next-page URLs. */
export const fetchProviderModels = async ({ provider, baseUrl, apiKey, fetchImpl = globalThis.fetch, timeoutMs = 10000 }) => {
    const endpoint = modelListEndpoint(provider, baseUrl);
    const headers = { accept: "application/json" };
    if (provider === "anthropic") {
        headers["x-api-key"] = apiKey;
        headers["anthropic-version"] = "2023-06-01";
    } else if (apiKey) headers.authorization = `Bearer ${apiKey}`;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), Math.min(10000, Math.max(1, timeoutMs)));
    const budget = { remaining: MAX_BYTES };
    const models = new Map();
    const cursors = new Set();
    try {
        let cursor;
        let truncated = false;
        for (let page = 0; page < MAX_PAGES; page++) {
            const url = new URL(endpoint);
            if (provider === "anthropic") {
                url.searchParams.set("limit", "100");
                if (cursor) url.searchParams.set("after_id", cursor);
            }
            const response = await fetchImpl(url.toString(), { method: "GET", headers, signal: controller.signal, redirect: "error" });
            if (!response.ok) {
                await response.body?.cancel().catch(() => {});
                throw new Error("provider request failed");
            }
            const body = await readBoundedJson(response, budget);
            if (!Array.isArray(body?.data) || body.data.length > MAX_MODELS
                || (body.has_more !== undefined && typeof body.has_more !== "boolean")) throw new Error("invalid response");
            for (const item of body.data) {
                if (!safeText(item?.id, apiKey)) throw new Error("invalid model");
                const id = item.id.trim();
                const label = item.display_name ?? item.name ?? id;
                if (!safeText(label, apiKey)) throw new Error("invalid model name");
                if (!models.has(id) && models.size < MAX_MODELS) models.set(id, { id, name: label.trim() });
                else if (!models.has(id)) truncated = true;
            }
            if (!body.has_more) break;
            if (provider !== "anthropic" || page === MAX_PAGES - 1 || models.size >= MAX_MODELS) {
                truncated = true;
                break;
            }
            if (!safeText(body.last_id, apiKey) || cursors.has(body.last_id)) throw new Error("invalid pagination");
            cursor = body.last_id;
            cursors.add(cursor);
        }
        if (!models.size) throw new Error("empty model list");
        return { models: [...models.values()], truncated };
    } finally {
        clearTimeout(timer);
        controller.abort();
    }
};

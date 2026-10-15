/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2
 */
import { strict as assert } from "node:assert";
import test from "node:test";
import { fetchProviderModels, modelListEndpoint } from "../../services/modelListService.mjs";

const response = (body) => new Response(JSON.stringify(body));
const models = [{ id: "model-a", name: "Model A", extra: "not returned" }];

for (const [provider, baseUrl, expected] of [
    ["openai", "", "https://api.openai.com/v1/models"],
    ["openai", "https://example.test/v1/", "https://example.test/v1/models"],
    ["openai-compatible", "http://localhost:8080/custom/v1/", "http://localhost:8080/custom/v1/models"],
    ["anthropic", "", "https://api.anthropic.com/v1/models"],
    ["anthropic", "https://example.test/v1/", "https://example.test/v1/models"],
    ["anthropic", "https://example.test", "https://example.test/v1/models"],
    ["openai-compatible", "https://api.deepseek.com/v1", "https://api.deepseek.com/v1/models"],
]) {
    test(`${provider} model listing URL and headers: ${baseUrl || 'default'}`, async () => {
        const result = await fetchProviderModels({ provider, baseUrl, apiKey: "mock-key", fetchImpl: async (url, options) => {
            const parsed = new URL(url);
            assert.equal(`${parsed.origin}${parsed.pathname}`, expected);
            assert.equal(options.method, "GET");
            assert.equal(options.redirect, "error");
            if (provider === "anthropic") {
                assert.equal(options.headers["x-api-key"], "mock-key");
                assert.equal(options.headers["anthropic-version"], "2023-06-01");
                assert.equal(options.headers.authorization, undefined);
            } else assert.equal(options.headers.authorization, "Bearer mock-key");
            return response({ data: models });
        } });
        assert.deepEqual(result, { models: [{ id: "model-a", name: "Model A" }], truncated: false });
    });
}

test("rejects invalid provider/URL and URL-embedded credentials", () => {
    for (const [provider, url] of [["other", "https://example.test"], ["openai-compatible", ""],
        ["openai", "file:///x"], ["openai", "https://user:pass@example.test"],
        ["openai", "https://example.test?api_key=secret"], ["openai", "https://example.test/#fragment"]]) {
        assert.throws(() => modelListEndpoint(provider, url));
    }
});

test("Anthropic pagination uses only validated cursors and is bounded to five pages", async () => {
    let calls = 0;
    const result = await fetchProviderModels({ provider: "anthropic", apiKey: "key", fetchImpl: async (url) => {
        const parsed = new URL(url);
        assert.equal(parsed.searchParams.get("limit"), "100");
        assert.equal(parsed.searchParams.get("after_id"), calls ? `m${calls}` : null);
        calls++;
        return response({ data: [{ id: `m${calls}`, display_name: `Model ${calls}` }], has_more: true, last_id: `m${calls}`, next: "https://untrusted.test" });
    } });
    assert.equal(calls, 5);
    assert.equal(result.models.length, 5);
    assert.equal(result.truncated, true);
});

test("compatible pagination is truncated without following an untrusted next URL", async () => {
    let calls = 0;
    const result = await fetchProviderModels({ provider: "openai-compatible", baseUrl: "https://example.test/v1", apiKey: "key", fetchImpl: async () => {
        calls++;
        return response({ data: models, has_more: true, next: "https://untrusted.test" });
    } });
    assert.equal(calls, 1);
    assert.equal(result.truncated, true);
});

test("candidate count, cumulative response bytes and invalid JSON are bounded", async () => {
    for (const body of [{ data: [] }, { data: [{ id: "" }] }, { data: [{ id: "key" }] },
        { data: [{ id: "a", name: "\nunsafe" }] }, { data: {} }, { data: models, has_more: "true" },
        { data: Array.from({ length: 501 }, (_, i) => ({ id: `m${i}` })) }]) {
        await assert.rejects(fetchProviderModels({ provider: "openai", apiKey: "key", fetchImpl: async () => response(body) }));
    }
    for (const body of ["not json", "x".repeat(1024 * 1024 + 1)]) {
        await assert.rejects(fetchProviderModels({ provider: "openai", apiKey: "key", fetchImpl: async () => new Response(body) }));
    }
});

test("pagination rejects repeated cursors", async () => {
    let calls = 0;
    await assert.rejects(fetchProviderModels({ provider: "anthropic", apiKey: "key", fetchImpl: async () => {
        calls++;
        return response({ data: models, has_more: true, last_id: "same" });
    } }));
    assert.equal(calls, 2);
});

test("timeout aborts a pending fetch without retry", async () => {
    let calls = 0;
    await assert.rejects(fetchProviderModels({ provider: "openai", apiKey: "key", timeoutMs: 1, fetchImpl: async (_url, { signal }) => {
        calls++;
        return new Promise((_resolve, reject) => signal.addEventListener("abort", () => reject(new Error("aborted")), { once: true }));
    } }));
    assert.equal(calls, 1);
});

test("timeout covers response body consumption", async () => {
    await assert.rejects(fetchProviderModels({ provider: "openai", apiKey: "key", timeoutMs: 1, fetchImpl: async (_url, { signal }) => new Response(new ReadableStream({
        start(controller) { signal.addEventListener("abort", () => controller.error(new Error("aborted")), { once: true }); },
    })) }));
});

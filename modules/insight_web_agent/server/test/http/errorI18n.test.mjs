/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import messages from "../../../src/i18n/apiErrors.json" with { type: "json" };
import { localizeError, resolveErrorLanguage } from "../../http/errorI18n.mjs";
import { createApp } from "../../app.mjs";
import { createRuntimeState } from "../../state/runtimeState.mjs";
import { json } from "../../http/response.mjs";

const renderResponse = (body, status, language) => {
    let result;
    json({
        req: { headers: { "accept-language": language } },
        writeHead() {},
        end(value) { result = JSON.parse(value); },
    }, body, status);
    return result;
};

test("configuration diagnostics survive HTTP localization without copying arbitrary error text", () => {
    for (const code of ["config_write_failed", "reload_failed"]) {
        const body = { error: code, message: "EACCES: example configuration file",
            details: { cause: "EACCES: example configuration file" }, saved: code === "reload_failed" };
        for (const language of ["en", "zh"]) {
            const result = renderResponse(body, 500, language);
            assert.equal(result.message, messages[code][language]);
            assert.deepEqual(result.details, body.details);
            assert.equal(result.saved, body.saved);
        }
    }
    const unknown = renderResponse({ code: "future_error", message: "sensitive provider diagnostic" }, 500, "zh");
    assert.equal(unknown.message, messages.request_failed.zh);
    assert.equal(unknown.details, undefined);
});

test("legacy HTTP error codes have meaningful localized status messages", () => {
    for (const language of ["en", "zh"]) {
        for (const [status, error] of [[401, "Unauthorized"], [403, "Forbidden"], [500, "Internal server error"]]) {
            const result = renderResponse({ error }, status, language);
            assert.equal(result.code, `http_${status}`);
            assert.equal(result.message, messages[`http_${status}`][language]);
            assert.equal(result.error, error);
        }
    }
});

test("successful responses do not read or negotiate the request language", () => {
    const body = { ok: true };
    const req = { get headers() { throw new Error("Success responses must not negotiate language"); } };
    json({
        req,
        writeHead(status, headers) {
            assert.equal(status, 200);
            assert.deepEqual(headers, { "content-type": "application/json; charset=utf-8" });
        },
        end(value) { assert.deepEqual(JSON.parse(value), body); },
    }, body);
});

test("language negotiation supports application locales, quality values and a stable fallback", () => {
    for (const [header, expected] of [
        [undefined, "en"], ["zhCN", "zh"], ["enUS", "en"], ["ZH-TW", "zh"],
        ["fr,zh-CN;q=0.9,en-US;q=0.5", "zh"], ["zh;q=0,en;q=0.8", "en"],
        ["zh;q=invalid,en", "en"], ["de-DE", "en"], ["en;q=0.2,zh;q=0.8", "zh"],
    ]) {
        assert.equal(resolveErrorLanguage(header), expected, String(header));
    }
});

test("every error has both translations with matching interpolation parameters", () => {
    for (const [code, entry] of Object.entries(messages)) {
        assert.ok(entry.en.trim(), code);
        assert.match(entry.zh, /[\u4e00-\u9fff]/, code);
        assert.deepEqual(entry.en.match(/\{\w+\}/g), entry.zh.match(/\{\w+\}/g), code);
        assert.equal(localizeError({ code: code.toUpperCase() }, "zh").message,
            entry.zh.replace(/\{\w+\}/g, ""), code);
    }
});

test("localization retains validation fields, diagnostic details, codes and save state", () => {
    const body = {
        error: "validation_failed", code: "validation_failed", saved: true,
        details: [{ field: "agentServers.0.name", code: "existing_agent_immutable",
            message: "Original English diagnostic", messageParams: { agentName: "custom" } }],
    };
    const result = localizeError(body, "zh");
    assert.equal(result.message, "不能删除已有 Agent 或修改其名称：custom");
    assert.equal(result.details[0].message, result.message);
    assert.equal(result.details[0].field, body.details[0].field);
    assert.equal(result.code, body.code);
    assert.equal(result.saved, true);
    assert.equal(body.details[0].message, "Original English diagnostic");
    assert.equal(localizeError({ code: "duplicate_agent_launch", conflictingAgent: "OpenCode" }, "zh").message,
        "ACP 启动命令与已配置的 Agent 重复：OpenCode");
    assert.equal(localizeError({ code: "future_code", message: "Raw provider error", details: { cause: "debug" } }, "zh").message,
        messages.request_failed.zh);
});

test("HTTP errors are localized per request without changing success payloads or diagnostics", async (t) => {
    const server = createApp({
        capabilityToken: "test-token", allowedOrigins: [], state: createRuntimeState(),
        agentConfigService: {
            readSnapshot: async () => ({ activeAgentName: "OpenCode" }),
            saveAgentServers: async () => ({ error: "duplicate_agent_launch", status: 409,
                conflictingAgent: "OpenCode", message: "Original diagnostic", saved: true,
                details: { cause: "technical details" } }),
        },
    });
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    t.after(() => server.close());
    const url = `http://127.0.0.1:${server.address().port}`;
    const responses = await Promise.all(["zh-CN", "en-US"].map(async (language) => {
        const response = await fetch(`${url}/api/agent-config/servers`, {
            method: "PUT", headers: { authorization: "Bearer test-token", "accept-language": language }, body: "{}",
        });
        assert.equal(response.status, 409);
        assert.equal(response.headers.get("content-language"), language.startsWith("zh") ? "zh" : "en");
        assert.equal(response.headers.get("cache-control"), "no-store");
        return await response.json();
    }));
    assert.equal(responses[0].message, "ACP 启动命令与已配置的 Agent 重复：OpenCode");
    assert.equal(responses[1].message, "ACP launch command duplicates configured agent: OpenCode");
    assert.equal(responses[0].code, "duplicate_agent_launch");
    assert.equal(responses[0].saved, true);
    assert.deepEqual(responses[0].details, { cause: "technical details" });

    const unauthorized = await fetch(`${url}/api/state`, { headers: { "accept-language": "zh-CN" } });
    assert.equal((await unauthorized.json()).message, messages.unauthorized.zh);
    const invalidJson = await fetch(`${url}/api/agent-config/servers`, { method: "PUT", body: "{",
        headers: { authorization: "Bearer test-token", "accept-language": "zh-CN" } });
    assert.equal(invalidJson.status, 400);
    assert.equal((await invalidJson.json()).message, messages.invalid_json.zh);
    const success = await fetch(`${url}/api/agent-config`, {
        headers: { authorization: "Bearer test-token", "accept-language": "zh-CN" },
    });
    assert.deepEqual(await success.json(), { snapshot: { activeAgentName: "OpenCode" } });
});

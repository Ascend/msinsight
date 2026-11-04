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
import assert from "node:assert/strict";
import test from "node:test";
import { createCapabilityTools } from "../../native-agent/tools/capabilityTools.mjs";
import { createInterruptedHistory } from "../../native-agent/runtime/interruptedHistory.mjs";

const definition = { name: "msinsight", inputSchema: { type: "object" }, requiresApproval: false };
const toolWith = (fetchImpl) => createCapabilityTools({ definitions: [definition], baseUrl: "http://127.0.0.1:1", fetchImpl })[0];

for (const status of [200, 502]) test(`non-JSON HTTP ${status} becomes an actionable, safe decode error`, async () => {
    const tool = toolWith(async () => new Response("<html>fixture_hidden_html</html>", { status, headers: { "content-type": "text/html" } }));
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_RESPONSE_INVALID");
        assert.equal(error.status, status);
        assert.equal(error.details.stage, "decode");
        assert.equal(error.details.outcome, "unknown");
        assert.equal(error.details.httpStatus, status);
        assert.ok(!JSON.stringify(error).includes("fixture_hidden_html"));
        return true;
    });
});

test("401 keeps authentication cause without exposing credentials", async () => {
    const tool = toolWith(async () => new Response('{"error":"Unauthorized"}', { status: 401, headers: { "content-type": "application/json" } }));
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_AUTH_FAILED");
        assert.match(error.message, /authentication/i);
        assert.equal(error.retryable, false);
        return true;
    });
});

test("network cause is preserved while post-dispatch outcome stays unknown", async () => {
    const tool = toolWith(async () => {
        throw new TypeError("fetch failed", { cause: Object.assign(new Error("fixture_private_dependency"), { code: "ECONNRESET" }) });
    });
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_REQUEST_FAILED");
        assert.equal(error.details.causeCode, "ECONNRESET");
        assert.equal(error.details.outcome, "unknown");
        assert.equal(error.retryable, false);
        assert.ok(!JSON.stringify(error).includes("fixture_private_dependency"));
        return true;
    });
});

test("caller cancellation never invokes fetch or becomes a retryable network error", async () => {
    let calls = 0;
    const tool = toolWith(async () => { calls++; return Response.json({ result: true }); });
    const controller = new AbortController();
    controller.abort({ code: "USER_CANCEL", private: "fixture-private" });
    await assert.rejects(tool.execute({ command: "help" }, { signal: controller.signal }), (error) => {
        assert.equal(error.code, "TOOL_CANCELLED");
        assert.equal(error.retryable, false);
        assert.equal(error.details.outcome, "not_started");
        assert.ok(!JSON.stringify(error).includes("fixture-private"));
        return true;
    });
    assert.equal(calls, 0);
});

for (const stage of ["dispatch", "decode"]) test(`HTTP guard timeout is not misreported as invalid JSON: ${stage}`, async (t) => {
    const timeout = AbortSignal.timeout.bind(AbortSignal);
    t.mock.method(AbortSignal, "timeout", () => timeout(5));
    const keepAlive = setTimeout(() => {}, 1000);
    t.after(() => clearTimeout(keepAlive));
    const tool = toolWith(async (_url, { signal }) => {
        if (stage === "dispatch") return new Promise((_resolve, reject) => {
            signal.addEventListener("abort", () => reject(signal.reason), { once: true });
        });
        return new Response(new ReadableStream({
            start(controller) {
                signal.addEventListener("abort", () => controller.error(signal.reason), { once: true });
            },
        }), { status: 200 });
    });
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_REQUEST_FAILED");
        assert.equal(error.name, "TimeoutError");
        assert.equal(error.details.stage, stage);
        assert.equal(error.details.outcome, "unknown");
        assert.match(error.message, /timed out/);
        return true;
    });
});

test("failure body reading is bounded and cancels an oversized response", async () => {
    let cancelled = false;
    const tool = toolWith(async () => new Response(new ReadableStream({
        start(controller) { controller.enqueue(new TextEncoder().encode("x".repeat(20000))); },
        cancel() { cancelled = true; },
    }), { status: 500, headers: { "content-type": "text/plain" } }));
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_RESPONSE_INVALID");
        assert.equal(error.status, 500);
        assert.equal(error.details.truncation.responseBody, true);
        return true;
    });
    assert.equal(cancelled, true);
});

test("a truncated body is not accepted just because its prefix happens to be valid JSON", async () => {
    const tool = toolWith(async () => new Response('{"error":"CLI_EXIT_NONZERO"}' + " ".repeat(20000), {
        status: 500, headers: { "content-type": "application/json" },
    }));
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_RESPONSE_INVALID");
        assert.equal(error.details.truncation.responseBody, true);
        return true;
    });
});

test("successful legacy responses are not truncated by failure budgets", async () => {
    const output = "x".repeat(50000);
    const tool = toolWith(async () => Response.json({ result: { output } }));
    assert.deepEqual(await tool.execute({ command: "help" }), { output });
});

test("an explicit failed HTTP envelope cannot masquerade as a successful tool", async () => {
    const tool = toolWith(async () => Response.json({
        ok: false, error: { code: "CAPABILITY_INVALID_ARGUMENT", message: "args must be an array", retryable: false },
    }));
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_INVALID_ARGUMENT");
        assert.equal(error.message, "args must be an array");
        return true;
    });
    const business = { ok: false, error: "business data, not a protocol failure" };
    assert.deepEqual(await toolWith(async () => Response.json({ ok: true, result: business })).execute({}), business);
});

test("invalid endpoint preparation preserves its cause code without leaking the endpoint", async () => {
    let calls = 0;
    const [tool] = createCapabilityTools({
        definitions: [definition], baseUrl: "fixture-private-invalid-url",
        fetchImpl: async () => { calls++; return Response.json({ result: true }); },
    });
    await assert.rejects(tool.execute({ command: "help" }), (error) => {
        assert.equal(error.code, "CAPABILITY_REQUEST_FAILED");
        assert.equal(error.details.causeCode, "ERR_INVALID_URL");
        assert.equal(error.details.outcome, "not_started");
        assert.ok(!JSON.stringify(error).includes("fixture-private-invalid-url"));
        return true;
    });
    assert.equal(calls, 0);
});

test("interrupted history handles structured non-Error rejection without object stringification", () => {
    const history = createInterruptedHistory();
    history.event({ type: "tool-call", toolCallId: "call", toolName: "msinsight", input: {} });
    history.event({
        type: "tool-error", toolCallId: "call", toolName: "msinsight",
        error: { code: "COMMAND_UNAVAILABLE", message: "Module unavailable.", details: { reason: "module_not_active" } },
    });
    const output = history.finish()[1].content[0].output.value;
    assert.match(output, /COMMAND_UNAVAILABLE/);
    assert.match(output, /module_not_active/);
    assert.notEqual(output, "[object Object]");
});

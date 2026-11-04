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
import { EventEmitter, once } from "node:events";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { PassThrough } from "node:stream";
import test from "node:test";
import { MockLanguageModelV3 } from "ai/test";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { createApp } from "../../app.mjs";
import { createCapabilityCenter } from "../../capability-center/service.mjs";
import { createCliCapability } from "../../capability-center/cliCapability.mjs";
import { capabilityError } from "../../capability-center/registry.mjs";
import { createHttpMcpAdapter } from "../../capability-adapters/http-mcp/adapter.mjs";
import { createCapabilityTools } from "../../native-agent/tools/capabilityTools.mjs";
import { createToolRegistry } from "../../native-agent/tools/ToolRegistry.mjs";
import { createRuntime } from "../../native-agent/runtime/aiSdkRuntime.mjs";
import { createRuntimeState, getSessionContext } from "../../state/runtimeState.mjs";
import { buildSessionExport } from "../../services/sessionExport.mjs";
import { createChatService } from "../../services/chatService.mjs";
import { createAcpSessionNotifier } from "../../native-agent/acp/sessionUpdates.mjs";
import { createFrontendCommandService } from "../../services/frontendCommandService.mjs";

const TOKEN = "fixture-native-only";
const STDERR = "fixture: database is missing required table TASK";
const usage = {
    inputTokens: { total: 10, noCache: 10, cacheRead: 0, cacheWrite: 0 },
    outputTokens: { total: 2, text: 2, reasoning: 0 },
};
const stream = (parts) => new ReadableStream({
    start(controller) {
        for (const part of parts) controller.enqueue(part);
        controller.close();
    },
});
const summary = [
    { type: "text-start", id: "summary" },
    { type: "text-delta", id: "summary", delta: "Fixture failure reported." },
    { type: "text-end", id: "summary" },
    { type: "finish", finishReason: { unified: "stop", raw: "stop" }, usage },
];

const fixture = async (t, { spawnError, maxOutputBytes, frontendError, frontendCommandService, loggerFailure = false } = {}) => {
    let executions = 0;
    const logs = [];
    const center = createCapabilityCenter({
        frontendCommandService: frontendCommandService ?? { request: async () => {
            if (frontendError) throw frontendError;
            return { activeModule: "Timeline" };
        } },
        logger: { warn: (value) => {
            logs.push(value);
            if (loggerFailure) throw new Error("fixture log unavailable");
        } },
    });
    center.register(createCliCapability({
        name: "pt_snap", executable: resolve("fixture-not-a-real-program.exe"), maxOutputBytes,
        spawnProcess: () => {
            executions++;
            if (spawnError) throw spawnError;
            const child = Object.assign(new EventEmitter(), {
                stdout: new PassThrough(), stderr: new PassThrough(),
                kill() { queueMicrotask(() => child.emit("close", null, "SIGTERM")); },
            });
            queueMicrotask(() => {
                child.stderr.end(STDERR);
                child.emit("close", 2, null);
            });
            return child;
        },
    }));
    const adapter = createHttpMcpAdapter({ capabilityCenter: center, accessToken: TOKEN });
    const server = createApp({
        capabilityCenter: center, httpMcpAdapter: adapter, nativeCapabilityToken: TOKEN,
        frontendCommandService,
        state: createRuntimeState(), allowedOrigins: [],
    });
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(async () => {
        await adapter.close();
        await new Promise((done) => server.close(done));
    });
    return {
        center, logs, executions: () => executions,
        baseUrl: `http://127.0.0.1:${server.address().port}`,
    };
};

test("CLI stderr crosses Native HTTP and MCP as the same safe five-field contract", async (t) => {
    const f = await fixture(t);
    const tools = createCapabilityTools({
        definitions: [{ ...f.center.list().find((tool) => tool.name === "pt_snap"), requiresApproval: false }],
        baseUrl: f.baseUrl, capabilityToken: TOKEN,
    });
    const id = "00000000-0000-4000-8000-000000000001";
    let nativeError;
    try { await tools[0].execute({ args: ["query"] }, { invocationId: id }); } catch (error) { nativeError = error; }
    assert.equal(nativeError.code, "CLI_EXIT_NONZERO");
    assert.equal(nativeError.status, 422);
    assert.equal(nativeError.details.stderr, STDERR);
    assert.equal(nativeError.details.exitCode, 2);
    assert.equal(nativeError.details.diagnosticId, id);
    assert.equal(nativeError.details.outcome, "failed");
    const client = new Client({ name: "fixture", version: "1.0.0" });
    t.after(() => client.close());
    await client.connect(new StreamableHTTPClientTransport(new URL("/mcp/capabilities", f.baseUrl), {
        requestInit: { headers: { Authorization: `Bearer ${TOKEN}` } },
    }));
    const result = await client.callTool({ name: "pt_snap", arguments: { args: ["query"] } });
    assert.equal(result.isError, true);
    assert.equal(result.structuredContent.error.details.stderr, STDERR);
    assert.deepEqual(JSON.parse(result.content[0].text), result.structuredContent);
    assert.equal(f.executions(), 2);
    assert.ok(f.logs.some((line) => JSON.stringify(line).includes(id)));
});

test("both adapters preserve permitted command state but never credentials", async (t) => {
    const error = Object.assign(capabilityError("COMMAND_UNAVAILABLE", "Module unavailable.", false, {
        reason: "module_not_active", targetModule: "Timeline", activeModule: "Compute", claimToken: "fixture-hidden",
    }), { state: { activeModule: "Compute", ready: false, claimToken: "fixture-hidden" } });
    const f = await fixture(t, { frontendError: error });
    const response = await fetch(`${f.baseUrl}/api/capabilities/invoke`, {
        method: "POST", headers: { authorization: `Bearer ${TOKEN}`, "content-type": "application/json" },
        body: JSON.stringify({ name: "msinsight", input: { command: "observe" } }),
    });
    const native = await response.json();
    assert.equal(native.state.activeModule, "Compute");
    assert.ok(!JSON.stringify(native).includes("fixture-hidden"));
    const client = new Client({ name: "fixture", version: "1.0.0" });
    t.after(() => client.close());
    await client.connect(new StreamableHTTPClientTransport(new URL("/mcp/capabilities", f.baseUrl), {
        requestInit: { headers: { Authorization: `Bearer ${TOKEN}` } },
    }));
    const result = await client.callTool({ name: "msinsight", arguments: { command: "observe" } });
    assert.equal(result.structuredContent.error.state.activeModule, "Compute");
    assert.ok(!JSON.stringify(result).includes("fixture-hidden"));
});

test("unterminated quoted credentials cannot cross HTTP, MCP or the Host log boundary", async (t) => {
    const secret = "fixturePartialCredential";
    const f = await fixture(t, { frontendError: capabilityError("COMMAND_EXECUTION_FAILED", `token="${secret}`) });
    const [native] = createCapabilityTools({
        definitions: [{ ...f.center.list().find((tool) => tool.name === "msinsight"), requiresApproval: false }],
        baseUrl: f.baseUrl, capabilityToken: TOKEN,
    });
    await assert.rejects(native.execute({ command: "observe" }), (error) => {
        assert.equal(error.code, "COMMAND_EXECUTION_FAILED");
        assert.ok(!JSON.stringify(error).includes(secret));
        assert.ok(!error.message.includes(secret));
        return true;
    });
    const client = new Client({ name: "fixture", version: "1.0.0" });
    t.after(() => client.close());
    await client.connect(new StreamableHTTPClientTransport(new URL("/mcp/capabilities", f.baseUrl), {
        requestInit: { headers: { Authorization: `Bearer ${TOKEN}` } },
    }));
    const result = await client.callTool({ name: "msinsight", arguments: { command: "observe" } });
    assert.equal(result.isError, true);
    assert.ok(!JSON.stringify(result).includes(secret));
    assert.ok(!f.logs.join("\n").includes(secret));
});

test("Basic and proxy auth values cannot cross HTTP, MCP or the Host log boundary", async (t) => {
    const secrets = ["ZmFrZTpwYXNz", "ZmFrZURpZ2VzdA=="];
    const f = await fixture(t, {
        frontendError: capabilityError("COMMAND_EXECUTION_FAILED",
            "Authorization: Basic ZmFrZTpwYXNz\nProxy-Authorization: Digest ZmFrZURpZ2VzdA=="),
    });
    const [native] = createCapabilityTools({
        definitions: [{ ...f.center.list().find((tool) => tool.name === "msinsight"), requiresApproval: false }],
        baseUrl: f.baseUrl, capabilityToken: TOKEN,
    });
    await assert.rejects(native.execute({ command: "observe" }), (error) => {
        assert.equal(error.code, "COMMAND_EXECUTION_FAILED");
        for (const secret of secrets) {
            assert.ok(!JSON.stringify(error).includes(secret), secret);
            assert.ok(!error.message.includes(secret), secret);
        }
        return true;
    });
    const client = new Client({ name: "fixture", version: "1.0.0" });
    t.after(() => client.close());
    await client.connect(new StreamableHTTPClientTransport(new URL("/mcp/capabilities", f.baseUrl), {
        requestInit: { headers: { Authorization: `Bearer ${TOKEN}` } },
    }));
    const result = await client.callTool({ name: "msinsight", arguments: { command: "observe" } });
    assert.equal(result.isError, true);
    for (const secret of secrets) {
        assert.ok(!JSON.stringify(result).includes(secret), secret);
        assert.ok(!f.logs.join("\n").includes(secret), secret);
    }
});
for (const [code, reason, stage, outcome, advice, retryable] of [
    ["COMMAND_BUSY", undefined, "validate", "not_started", "retry_after_ready", true],
    ["COMMAND_UNAVAILABLE", "module_not_active", "validate", "not_started", "refresh_context", true],
    ["COMMAND_CONNECTION_LOST", undefined, "execute", "unknown", "inspect_before_retry", false],
    ["COMMAND_TIMEOUT", undefined, "execute", "unknown", "inspect_before_retry", false],
    ["COMMAND_RESULT_INVALID", "result_not_serializable", "respond", "completed", "inspect_before_retry", false],
]) test(`real Broker classification survives Native HTTP and MCP: ${code}`, async (t) => {
    let receipts = 0;
    const broker = createFrontendCommandService({ eventBus: {
        broadcast(event) {
            if (event.type !== "frontend_command_request") return;
            queueMicrotask(() => {
                const claim = broker.claim(event.requestId);
                receipts++;
                broker.respond({
                    requestId: event.requestId, claimToken: claim.claimToken, status: "failed",
                    error: { code, message: "Fixture command failure.", retryable: true, details: { reason } },
                });
            });
        },
    } });
    t.after(() => broker.dispose());
    const f = await fixture(t, { frontendCommandService: broker });
    const check = (error) => {
        assert.equal(error.code, code);
        assert.equal(error.details.stage, stage);
        assert.equal(error.details.outcome, outcome);
        assert.equal(error.details.retryAdvice, advice);
        assert.equal(error.retryable, retryable);
        assert.ok(f.logs.some((line) => line.includes(error.details.diagnosticId)));
        return true;
    };
    const [native] = createCapabilityTools({
        definitions: [{ ...f.center.list().find((tool) => tool.name === "msinsight"), requiresApproval: false }],
        baseUrl: f.baseUrl, capabilityToken: TOKEN,
    });
    await assert.rejects(native.execute({ command: "observe" }), check);
    const client = new Client({ name: "fixture", version: "1.0.0" });
    t.after(() => client.close());
    await client.connect(new StreamableHTTPClientTransport(new URL("/mcp/capabilities", f.baseUrl), {
        requestInit: { headers: { Authorization: `Bearer ${TOKEN}` } },
    }));
    const result = await client.callTool({ name: "msinsight", arguments: { command: "observe" } });
    assert.equal(result.isError, true);
    check(result.structuredContent.error);
    assert.equal(receipts, 2);
});

test("validation, process-start errors and output limits keep their real phase", async (t) => {
    const f = await fixture(t, { spawnError: Object.assign(new Error("fixture missing executable"), { code: "ENOENT" }), loggerFailure: true });
    await assert.rejects(f.center.invoke({ name: "pt_snap", input: { args: "bad" } }), (error) => {
        assert.equal(error.code, "CAPABILITY_INVALID_ARGUMENT");
        assert.equal(error.details.stage, "validate");
        assert.equal(error.details.outcome, "not_started");
        return true;
    });
    await assert.rejects(f.center.invoke({ name: "pt_snap", input: { args: [] } }), (error) => {
        assert.equal(error.code, "CLI_START_FAILED");
        assert.equal(error.details.causeCode, "ENOENT");
        assert.equal(error.details.outcome, "not_started");
        return true;
    });
    const limited = await fixture(t, { maxOutputBytes: 12 });
    await assert.rejects(limited.center.invoke({ name: "pt_snap", input: { args: [] } }), (error) => {
        assert.equal(error.code, "CLI_OUTPUT_LIMIT");
        assert.equal(error.details.truncation.captureLimited, true);
        assert.ok(error.details.stderr);
        return true;
    });
});

test("pre-cancelled CLI invocation does not spawn a process", async (t) => {
    const f = await fixture(t);
    const controller = new AbortController();
    controller.abort();
    await assert.rejects(f.center.invoke({ name: "pt_snap", input: { args: [] }, signal: controller.signal }), (error) => {
        assert.equal(error.code, "CLI_CANCELLED");
        assert.equal(error.retryable, false);
        assert.equal(error.details.outcome, "not_started");
        return true;
    });
    assert.equal(f.executions(), 0);
});

test("known pre-execution command failures keep retry advice, while timeout is conservative", async () => {
    for (const code of ["COMMAND_BUSY", "COMMAND_TIMEOUT"]) {
        const center = createCapabilityCenter({
            frontendCommandService: { request: async () => { throw capabilityError(code, "Fixture command failure.", true); } },
            logger: { warn() {} },
        });
        await assert.rejects(center.invoke({ name: "msinsight", input: { command: "observe" } }), (error) => {
            assert.equal(error.retryable, code === "COMMAND_BUSY");
            assert.equal(error.details.outcome, code === "COMMAND_BUSY" ? "not_started" : "unknown");
            return true;
        });
    }
    const stale = createCapabilityCenter({
        frontendCommandService: { request: async () => { throw Object.assign(
            capabilityError("TABLE_STATE_STALE", "Table changed.", true, { expectedRevision: 1, currentRevision: 2 }),
            { state: { query: { page: 2, pageSize: 10 }, total: 30, rowCount: 10 } },
        ); } },
        logger: { warn() {} },
    });
    await assert.rejects(stale.invoke({ name: "msinsight", input: { command: "table.setPage" } }), (error) => {
        assert.equal(error.code, "TABLE_STATE_STALE");
        assert.equal(error.details.retryAdvice, "refresh_context");
        assert.equal(error.details.outcome, "not_started");
        assert.equal(error.state.query.page, 2);
        return true;
    });
});

for (const mode of ["normal", "recovery", "user_cancel"]) test(`actual SDK CLI failure retains diagnostics in model, UI and history: ${mode}`, async (t) => {
    const f = await fixture(t);
    const root = await mkdtemp(join(tmpdir(), "tool-error-sdk-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = { sessionId: "fixture-session", permissionMode: "full-access", messages: [], hostSystemPrompt: "", primaryAgentBody: "" };
    const [native] = createCapabilityTools({
        definitions: [{ ...f.center.list().find((tool) => tool.name === "pt_snap"), requiresApproval: true }],
        sessions: new Map([[session.sessionId, session]]),
        hostClient: { request() { throw new Error("Unexpected fixture approval"); } },
        baseUrl: f.baseUrl, capabilityToken: TOKEN,
    });
    const controller = new AbortController();
    let calls = 0;
    const model = new MockLanguageModelV3({
        doStream: async () => {
            calls++;
            if (calls === 2 && mode === "recovery") throw Object.assign(new Error("fixture transport offline"), { statusCode: 503 });
            if (calls === 2 && mode === "user_cancel") controller.abort(Object.assign(new Error("fixture stop"), { code: "USER_CANCEL" }));
            return { stream: stream([
                { type: "stream-start", warnings: [] },
                ...(calls === 1 ? [
                    { type: "tool-call", toolCallId: "call-1", toolName: "pt_snap", input: '{"args":["query"]}' },
                    { type: "finish", finishReason: { unified: "tool-calls", raw: "tool_calls" }, usage },
                ] : summary),
            ]) };
        },
    });
    const updates = [];
    const hostState = createRuntimeState();
    getSessionContext(hostState, session.sessionId).pendingPrompt = true;
    const chat = createChatService({ state: hostState, eventBus: { broadcast() {} } });
    const notifier = createAcpSessionNotifier({ write: (line) => chat.handleAcpNotification(JSON.parse(line)) });
    const runtime = createRuntime({
        aiSdkStoragePath: root, createModel: () => model, retrySleep: async () => {},
        modelCatalog: { defaultModelId: "fixture", models: [{ id: "fixture" }] },
        toolRegistry: createToolRegistry({ tools: [native] }),
        notifier: {
            ...notifier,
            sendToolCallUpdate: (sessionId, kind, toolCall) => {
                updates.push({ kind, ...structuredClone(toolCall) });
                notifier.sendToolCallUpdate(sessionId, kind, toolCall);
            },
        },
    });
    const result = await runtime.runPrompt({ session, sessionId: session.sessionId, userText: "fixture", controller });
    assert.equal(result.ok, mode !== "user_cancel", result.reason);
    assert.equal(f.executions(), 1, "A reporting/recovery failure must not repeat the CLI");
    const modelResult = model.doStreamCalls[1].prompt.find((message) => message.role === "tool").content[0].output;
    assert.match(modelResult.value, /CLI_EXIT_NONZERO/);
    assert.match(modelResult.value, /required table TASK/);
    assert.ok(modelResult.value.length <= 3600);
    const terminal = updates.find((update) => update.kind === "tool_call_update");
    assert.equal(terminal.output, modelResult.value);
    const hostTool = hostState.sessionContexts.get(session.sessionId).messages.flatMap((message) => message.content)
        .find((block) => block.type === "tool");
    assert.equal(hostTool.toolCall.output, modelResult.value);
    const restored = { sessionId: session.sessionId };
    await runtime.restoreSession(restored);
    const tool = restored.messages.flatMap((message) => message.content).find((block) => block.type === "tool");
    assert.equal(tool.toolCall.output, terminal.output);
    const exported = buildSessionExport({ sessionId: session.sessionId, title: "Fixture", messages: restored.messages });
    assert.match(exported.markdown, /required table TASK/);
    assert.match(JSON.stringify(exported.messages), /CLI_EXIT_NONZERO/);
    if (mode !== "normal") {
        assert.match(JSON.stringify(session.runtimeSession.modelMessages), /required table TASK/);
        assert.equal(session.runtimeSession.modelMessages.flatMap((message) => Array.isArray(message.content) ? message.content : [])
            .filter((part) => part.type === "tool-result" && part.toolCallId === "call-1").length, 1);
    }
});

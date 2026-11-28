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
import { createSessionExportSource } from "../../services/sessionExportSource.mjs";
import { createSessionBatchExportService } from "../../services/sessionBatchExport.mjs";
import { setImmediate as yieldTurn } from "node:timers/promises";

const fixture = (options = {}) => {
    const calls = [];
    let receive;
    let disconnects = 0;
    let unsubscribes = 0;
    let adapterOptions;
    const emit = (sessionId, update) => receive?.({ method: "session/update", params: { sessionId, update } });
    const source = () => createSessionExportSource({
        agentServer: { name: options.native ? "msinsight-native" : "owner", command: "unused", args: [], env: { SECRET: "private" } },
        cwd: "/export-cwd", requestTimeoutMs: 1234, maxHistoryBytes: options.maxHistoryBytes,
        hiddenPromptTexts: options.hiddenPromptTexts,
        createAdapter: (config) => {
            adapterOptions = config;
            return {
                onMessage: (handler) => { receive = handler; return () => { receive = undefined; unsubscribes++; }; },
                disconnect: async () => { disconnects++; },
                request: async (method, params) => {
                    calls.push([method, params]);
                    if (method === "initialize") {
                        if (options.initError) throw options.initError;
                        return options.initialized ?? { agentInfo: { name: options.native ? "msinsight-native" : "test-agent", version: "1", env: "not returned" }, agentCapabilities: { loadSession: true, sessionCapabilities: { list: {} } }, _meta: options.native ? { "msinsight.dev/sessionExport": true } : {} };
                    }
                    if (options.request) return options.request(method, params, emit);
                    if (method === "session/list") return { sessions: [{ sessionId: "a" }], nextCursor: "next-page" };
                    if (method === "session/load") { emit(params.sessionId, { sessionUpdate: "user_message_chunk", content: { type: "text", text: "hello" } }); return {}; }
                    throw new Error(`Unexpected RPC ${method}`);
                },
            };
        },
    });
    return { source, calls, emit, get disconnects() { return disconnects; }, get unsubscribes() { return unsubscribes; }, get adapterOptions() { return adapterOptions; } };
};

test("detached export routes delayed tool updates to the original assistant message", async () => {
    const f = fixture({ request: async (method, params, emit) => {
        assert.equal(method, "session/load");
        for (const update of [
            { sessionUpdate: "agent_message_chunk", messageId: "first", content: { type: "text", text: "First" } },
            { sessionUpdate: "tool_call", toolCallId: "call", title: "Original tool", status: "in_progress" },
            { sessionUpdate: "agent_message_chunk", messageId: "second", content: { type: "text", text: "Second" } },
            { sessionUpdate: "tool_call_update", toolCallId: "call", status: "completed", rawOutput: "Done" },
        ]) {
            emit(params.sessionId, update);
        }
        return {};
    } });
    const source = await f.source();
    try {
        const messages = await source.load("s");
        const tools = messages.flatMap((message) => message.content.filter((block) => block.type === "tool"));
        assert.equal(tools.length, 1);
        assert.equal(tools[0].toolCall.status, "completed");
        assert.equal(tools[0].toolCall.name, "Original tool");
        assert.equal(tools[0].toolCall.output, "Done");
        assert.ok(messages[0].content.includes(tools[0]));
        assert.equal(messages[1].id, "second");
    } finally {
        await source.close();
    }
});

test("initializes once without host capabilities, forwards every list cursor, never probes or prompts", async () => {
    const f = fixture();
    const source = await f.source();
    await source.list({});
    await source.list({ cursor: "next-page" });
    await source.load("a");
    await source.load("b");
    await source.close();
    await source.close();
    assert.deepEqual(f.calls.map(([method]) => method), ["initialize", "session/list", "session/list", "session/load", "session/load"]);
    assert.deepEqual(f.calls[0][1].clientCapabilities, { fs: { readTextFile: false, writeTextFile: false }, terminal: false, auth: { terminal: false } });
    assert.deepEqual(f.calls[2][1], { cwd: "/export-cwd", cursor: "next-page" });
    assert.deepEqual(f.calls[3][1], { sessionId: "a", cwd: "/export-cwd", additionalDirectories: [], mcpServers: [] });
    assert.deepEqual(source.agentInfo, { name: "test-agent", version: "1" });
    assert.equal(f.adapterOptions.requestTimeoutMs, 1234);
    assert.equal(f.adapterOptions.forwardStderr, true);
    assert.equal(f.disconnects, 1);
    assert.equal(f.unsubscribes, 1);
    await assert.rejects(source.load("a"), { code: "session_export_source_closed" });
    await assert.rejects(source.list({}), { code: "session_export_source_closed" });
    assert.equal(f.calls.length, 5);
});

test("drains a noisy detached agent's stderr without logging its contents", { timeout: 5000 }, async (t) => {
    const logs = [];
    const log = t.mock.method(console, "error", (message) => logs.push(message));
    const program = `
        const readline = require("node:readline");
        const input = readline.createInterface({ input: process.stdin });
        (async () => {
            for await (const line of input) {
                const request = JSON.parse(line);
                if (request.method === "initialize") {
                    // Exceed the OS pipe buffer; the response requires a drained stderr.
                    await new Promise((resolve) => process.stderr.write("stderr-secret-must-not-leak".repeat(100000), resolve));
                    process.stdout.write(JSON.stringify({ jsonrpc: "2.0", id: request.id, result: {
                        agentInfo: { name: "noisy-agent", version: "1" },
                        agentCapabilities: { loadSession: true, sessionCapabilities: { list: {} } }
                    } }) + "\\n");
                } else if (request.method === "session/list") {
                    process.stdout.write(JSON.stringify({ jsonrpc: "2.0", id: request.id, result: { sessions: [] } }) + "\\n");
                }
            }
        })();
    `;
    let source;
    try {
        source = await createSessionExportSource({
            agentServer: { name: "noisy-owner", command: process.execPath, args: ["-e", program] },
            cwd: process.cwd(), requestTimeoutMs: 2000,
        });
        assert.deepEqual(await source.list({}), { sessions: [] });
        assert.ok(logs.length > 0);
        assert.ok(logs.every((message) => /^ACP stderr: bytes=\d+$/.test(message)));
        assert.equal(JSON.stringify(logs).includes("stderr-secret"), false);
        assert.equal(source.agentInfo.name, "noisy-agent");
    } finally {
        await source?.close();
        log.mock.restore();
    }
});

test("isolated collector retains normalized block fields and complete tool output, ignores other sessions/config", async () => {
    const longOutput = "x".repeat(5000);
    const f = fixture({ request: async (method, { sessionId }, emit) => {
        assert.equal(method, "session/load");
        emit("foreign", { sessionUpdate: "user_message_chunk", content: { type: "text", text: "do not collect" } });
        emit(sessionId, { sessionUpdate: "config_option_update", configOptions: [{ apiKey: "secret" }] });
        emit(sessionId, { sessionUpdate: "userMessageChunk", messageId: "u1", content: [
            { id: "text", type: "text", text: "hello ", annotation: { audience: ["user"] } },
            { id: "text", type: "text", text: "world" },
            { id: "image", type: "image", data: "abc", mimeType: "image/png", name: "plot", annotations: { priority: 1 } },
            { id: "resource", type: "resource", resource: { uri: "file:///example", mimeType: "text/plain", text: "resource body" } },
            { type: "resource", resource: { uri: "insight-system-prompt://project", text: "hidden" } },
        ] });
        emit(sessionId, { sessionUpdate: "agent_thought_chunk", content: { id: "thought", type: "text", text: "reason" } });
        emit(sessionId, { sessionUpdate: "tool_call", toolCallId: "tool-1", title: "Read", status: "pending", rawInput: { path: "/x" }, locations: [{ path: "/x", line: 2 }] });
        emit(sessionId, { sessionUpdate: "tool_call_update", toolCallId: "tool-1", status: "completed", rawOutput: longOutput, durationMs: 45 });
        emit(sessionId, { sessionUpdate: "agent_message_chunk", messageId: "answer", content: { type: "text", text: "answer" } });
        return {};
    } });
    const source = await f.source();
    const history = await source.load("bound");
    await source.close();
    assert.equal(history.length, 2);
    assert.equal(history[0].id, "u1");
    assert.equal(history[0].content[0].text, "hello world");
    assert.deepEqual(history[0].content[0].annotation, { audience: ["user"] });
    assert.equal(history[0].content[1].mimeType, "image/png");
    assert.deepEqual(history[0].content[1].annotations, { priority: 1 });
    assert.equal(history[0].content[2].resource.text, "resource body");
    assert.equal(history[1].content[0].type, "thinking");
    assert.equal(history[1].id, "answer");
    assert.equal(history[1].content[1].toolCall.output, longOutput);
    assert.equal(history[1].content[1].toolCall.status, "completed");
    assert.equal(history[1].content[1].toolCall.durationMs, 45);
    assert.deepEqual(history[1].content[1].toolCall.locations, [{ path: "/x", line: 2 }]);
    assert.equal(history[1].content[2].text, "answer");
    assert.equal(/do not collect|hidden|apiKey/.test(JSON.stringify(history)), false);
});

for (const native of [false, true]) {
    test(`export filters fragmented preset/JSON and preserves visible input: native=${native}`, async () => {
        const preset = "Private instructions";
        const sourceText = `<context ref="insight-assistant-preset://project"/>\n${preset}\n`
            + '<context ref="insight-hidden-context://project"/>\n{"data":{"secret":"hidden"}}\nVisible question';
        const blocks = [];
        for (let index = 0; index < sourceText.length; index += 7) {
            blocks.push({ id: `block-${index}`, type: "text", text: sourceText.slice(index, index + 7) });
        }
        const original = [{ role: "user", content: blocks }];
        const before = structuredClone(original);
        const f = fixture({ native, hiddenPromptTexts: [preset], request: async (method, params, emit) => {
            if (native) {
                assert.equal(method, "_msinsight/session/export");
                return { messages: original };
            }
            assert.equal(method, "session/load");
            for (const block of blocks) {
                emit(params.sessionId, { sessionUpdate: "user_message_chunk", content: block });
            }
            return {};
        } });
        const source = await f.source();
        try {
            const messages = await source.load("s");
            assert.equal(messages.length, 1);
            assert.equal(messages[0].content.map((block) => block.text).join(""), "Visible question");
            assert.deepEqual(original, before);
        } finally {
            await source.close();
        }
    });
}

test("Native uses only advertised read-only RPC and returns exact UI messages without replay", async () => {
    const messages = [{ id: "assistant", role: "assistant", completionStatus: "cancelled", completedAt: 123, content: [{ id: "tool", type: "tool", toolCall: { toolCallId: "tc", output: "complete", custom: true } }, { id: "img", type: "image", mimeType: "image/webp", data: "abc" }] }];
    const f = fixture({ native: true, request: async (method, params) => {
        assert.equal(method, "_msinsight/session/export");
        assert.deepEqual(params, { sessionId: "native-1" });
        return { messages };
    } });
    const source = await f.source();
    const loaded = await source.load("native-1");
    await source.close();
    assert.deepEqual(loaded, messages);
    loaded[0].content[0].toolCall.output = "mutated";
    assert.equal(messages[0].content[0].toolCall.output, "complete");
    assert.deepEqual(f.calls.map(([method]) => method), ["initialize", "_msinsight/session/export"]);
});

for (const [label, initialized, code] of [
    ["missing list", { agentCapabilities: { loadSession: true } }, "session_export_list_unsupported"],
    ["resume only", { agentCapabilities: { sessionCapabilities: { list: {}, resume: {} } } }, "session_export_load_unsupported"],
    ["native missing read-only", { agentInfo: { name: "msinsight-native" }, agentCapabilities: { loadSession: true, sessionCapabilities: { list: {} } } }, "session_export_readonly_unsupported"],
]) {
    test(`unsupported ${label} is explicit and disconnects initialization`, async () => {
        const f = fixture({ initialized });
        await assert.rejects(f.source(), { code });
        assert.equal(f.disconnects, 1);
        assert.equal(f.unsubscribes, 1);
        assert.deepEqual(f.calls.map(([method]) => method), ["initialize"]);
    });
}

test("initialization error closes the subprocess and does not probe", async () => {
    const f = fixture({ initError: new Error("init failure") });
    await assert.rejects(f.source(), /init failure/);
    assert.equal(f.disconnects, 1);
    assert.equal(f.calls.length, 1);
});

test("history limits are explicit for generic replay and Native direct history", async () => {
    for (const native of [false, true]) {
        const f = fixture({ native, maxHistoryBytes: 100, request: async (_method, { sessionId }, emit) => {
            if (native) return { messages: [{ role: "user", content: [{ type: "text", text: "x".repeat(1000) }] }] };
            emit(sessionId, { sessionUpdate: "user_message_chunk", content: { type: "text", text: "x".repeat(1000) } });
            return {};
        } });
        const source = await f.source();
        await assert.rejects(source.load("large"), { code: "session_export_history_limit" });
        await source.close();
        assert.equal(f.disconnects, 1);
    }
});

test("dispose disconnects a source while initialize is pending, and awaits its cleanup", async () => {
    let rejectInitialize;
    let releaseDisconnect;
    let requests = 0;
    let disconnects = 0;
    const service = createSessionBatchExportService({
        now: () => "2026-09-15T00:00:00Z",
        listAgents: () => [{ name: "a" }],
        openSource: (agentServer, { signal }) => createSessionExportSource({
            agentServer, cwd: "/export", signal,
            createAdapter: () => ({
                onMessage: () => () => {},
                request: async () => { requests++; return new Promise((_resolve, reject) => { rejectInitialize = reject; }); },
                disconnect: async () => {
                    disconnects++;
                    await new Promise((resolve) => { releaseDisconnect = resolve; });
                    rejectInitialize(new Error("ACP adapter disconnected"));
                },
            }),
        }),
    });
    await service.startQuery({ days: 1 });
    await yieldTurn();
    assert.equal(requests, 1);
    let disposed = false;
    const disposal = service.dispose().then(() => { disposed = true; });
    await yieldTurn();
    assert.equal(disconnects, 1);
    assert.equal(disposed, false);
    releaseDisconnect();
    await disposal;
    assert.equal(disposed, true);
    assert.equal(requests, 1);
});

test("pre-aborted source never initializes or reconnects", async () => {
    const controller = new AbortController();
    controller.abort();
    let disconnects = 0;
    await assert.rejects(createSessionExportSource({ agentServer: { name: "a" }, signal: controller.signal, createAdapter: () => ({
        onMessage: () => () => {},
        request: () => assert.fail("must not initialize"),
        disconnect: async () => { disconnects++; },
    }) }), { code: "session_export_source_closed" });
    assert.equal(disconnects, 1);
});

test("load failures clear the collector; busy Native and malformed direct history are explicit", async () => {
    let attempts = 0;
    const f = fixture({ request: async (_method, { sessionId }, emit) => {
        if (++attempts === 1) throw new Error("load failed");
        emit(sessionId, { sessionUpdate: "user_message_chunk", content: { type: "text", text: "retry" } });
        return {};
    } });
    const source = await f.source();
    await assert.rejects(source.load("bad"), /load failed/);
    assert.equal((await source.load("good"))[0].content[0].text, "retry");
    await source.close();
    for (const [result, code] of [[new Error("Session has a running prompt and cannot be exported"), "session_export_busy"], [{ messages: null }, "session_export_history_invalid"]]) {
        const nativeFixture = fixture({ native: true, request: async () => { if (result instanceof Error) throw result; return result; } });
        const nativeSource = await nativeFixture.source();
        await assert.rejects(nativeSource.load("native"), { code });
        await nativeSource.close();
    }
});

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
import { EventEmitter } from "node:events";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { MockLanguageModelV3 } from "ai/test";
import { createRuntime } from "../../native-agent/runtime/aiSdkRuntime.mjs";
import { createBashTools } from "../../native-agent/tools/bashTools.mjs";
import { createToolRegistry } from "../../native-agent/tools/ToolRegistry.mjs";
import { createAcpSessionNotifier } from "../../native-agent/acp/sessionUpdates.mjs";
import { createRuntimeState, getSessionContext } from "../../state/runtimeState.mjs";
import { createChatService } from "../../services/chatService.mjs";
import { buildSessionExport } from "../../services/sessionExport.mjs";

test("Bash nonzero survives SDK, ACP, state-v1 restart and JSON/Markdown export without changing model results", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "insight-process-history-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const session = { sessionId: "process-fixture", permissionMode: "full-access", messages: [], hostSystemPrompt: "", primaryAgentBody: "" };
    let executions = 0;
    const tools = createBashTools({ sessions: new Map([[session.sessionId, session]]), cwd: root, spawnProcess: () => {
        executions++;
        const child = new EventEmitter();
        child.stdout = new EventEmitter();
        child.stderr = new EventEmitter();
        queueMicrotask(() => {
            child.stdout.emit("data", "x".repeat(10000));
            child.stderr.emit("data", "missing TASK; token=private-secret");
            child.emit("close", 2, null);
        });
        return child;
    } });
    let requests = 0;
    const usage = { inputTokens: { total: 1, noCache: 1, cacheRead: 0, cacheWrite: 0 }, outputTokens: { total: 1, text: 1, reasoning: 0 } };
    const model = new MockLanguageModelV3({ doStream: async () => ({
        stream: new ReadableStream({ start(controller) {
            controller.enqueue({ type: "stream-start", warnings: [] });
            if (++requests === 1) controller.enqueue({ type: "tool-call", toolCallId: "bash", toolName: "Bash", input: '{"command":"echo fixture"}' });
            controller.enqueue({ type: "finish", finishReason: { unified: requests === 1 ? "tool-calls" : "stop", raw: "fixture" }, usage });
            controller.close();
        } }),
    }) });
    const state = createRuntimeState();
    getSessionContext(state, session.sessionId).pendingPrompt = true;
    const chat = createChatService({ state, eventBus: { broadcast() {} } });
    const runtime = createRuntime({
        aiSdkStoragePath: root, createModel: () => model,
        modelCatalog: { defaultModelId: "fixture", models: [{ id: "fixture" }] },
        toolRegistry: createToolRegistry({ tools }),
        notifier: createAcpSessionNotifier({ write: (line) => chat.handleAcpNotification(JSON.parse(line)) }),
    });
    assert.equal((await runtime.runPrompt({ session, sessionId: session.sessionId, userText: "fixture", controller: new AbortController() })).ok, true);
    assert.equal(executions, 1);
    const modelResult = model.doStreamCalls[1].prompt.find((message) => message.role === "tool").content[0].output.value;
    assert.equal(modelResult.exitCode, 2);
    assert.equal(modelResult.stdout.length, 10000);
    const tool = state.sessionContexts.get(session.sessionId).messages[0].content.find((block) => block.type === "tool").toolCall;
    assert.equal(tool.status, "completed");
    assert.deepEqual(tool.process, { exitCode: 2 });
    assert.match(tool.output.slice(0, 100), /missing TASK/);
    assert.doesNotMatch(tool.output, /private-secret/);
    const restored = { sessionId: session.sessionId };
    await runtime.restoreSession(restored);
    assert.deepEqual(restored.messages.flatMap((message) => message.content).find((block) => block.type === "tool").toolCall.process, { exitCode: 2 });
    const exported = buildSessionExport({ sessionId: session.sessionId, messages: restored.messages });
    assert.match(exported.markdown, /Exit code: 2/);
    assert.deepEqual(JSON.parse(JSON.stringify(exported)).messages.flatMap((message) => message.content)
        .find((block) => block.type === "tool").toolCall.process, { exitCode: 2 });
    assert.equal(JSON.parse(await readFile(join(root, "sessions", `${session.sessionId}.json`), "utf8")).version, 1);
});

/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import { randomUUID } from "node:crypto";
import test from "node:test";
import { normalizeToolCall } from "../../services/chatService.mjs";
import { createWorkspaceTurnCorrelation, workspaceReceiptBlock } from "../../services/workspaceTurnCorrelation.mjs";

const setup = () => {
    const contexts = new Map(["a", "b"].map(id => [id, { pendingPrompt: true, turn: randomUUID() }]));
    const events = [];
    const correlation = createWorkspaceTurnCorrelation({
        state: { sessionContexts: contexts },
        captureWorkspaceTurn: id => contexts.get(id)?.turn,
        recordWorkspaceOutcome: (...args) => events.push(args),
    });
    return { contexts, correlation, events };
};

// Codex ACP encodes the MCP server and tool in rawInput and wraps the MCP
// response inside { result, error } in its terminal tool update.
const rawUpdate = (id) => ({
    rawInput: { server: "msinsight-capabilities", tool: "msinsight", arguments: { command: "Timeline.zoom" } },
    rawOutput: { result: { content: [{ type: "text", text: "zoomed" }, workspaceReceiptBlock(id)] }, error: null },
});
const terminal = { status: "completed", name: "mcp.msinsight-capabilities.msinsight" };

test("assigns each shared MCP receipt to its reporting turn once, including successful actions", () => {
    const { contexts, correlation, events } = setup();
    const pending = correlation.capture();
    for (const outcome of ["interrupted", "completed"]) {
        const id = randomUUID();
        correlation.remember(id, pending, outcome);
        const update = rawUpdate(id);
        correlation.consume("b", update, { ...terminal, status: "in_progress" });
        correlation.consume("b", update, terminal);
        correlation.consume("a", update, terminal);
        correlation.consume("b", update, terminal);
    }
    assert.deepEqual(events, ["interrupted", "completed"].map(outcome => ["b", contexts.get("b").turn, outcome]));
});

test("ignores history, unrelated tools, invented receipts, later turns and expired receipts", t => {
    let now = Date.now();
    t.mock.method(Date, "now", () => now);
    const { contexts, correlation, events } = setup();
    const id = randomUUID();
    correlation.remember(id, correlation.capture(), "interrupted");
    contexts.get("a").turn = randomUUID();
    correlation.consume("a", rawUpdate(id), terminal);
    contexts.get("b").replayingHistory = true;
    correlation.consume("b", rawUpdate(id), terminal);
    contexts.get("b").replayingHistory = false;
    correlation.consume("b", { ...rawUpdate(id), rawInput: { tool: "shell" } }, terminal);
    correlation.consume("b", rawUpdate(randomUUID()), terminal);
    now += 60_001;
    correlation.consume("b", rawUpdate(id), terminal);
    assert.deepEqual(events, []);
});

test("a client dropping successful response metadata can clear only a matching workspace action", () => {
    const { contexts, correlation, events } = setup();
    const jointId = randomUUID();
    correlation.remember(jointId, correlation.capture(), "completed", "Timeline.zoom");
    const update = { rawInput: { tool: "msinsight", arguments: { command: "Timeline.zoom" } } };
    correlation.consume("b", update, terminal, "in_progress");
    assert.deepEqual(events, []); // Other active session may own the same command.
    correlation.consume("a", rawUpdate(jointId), terminal);
    contexts.delete("a");
    correlation.remember(randomUUID(), correlation.capture(), "completed", "Timeline.zoom");
    update.rawInput.arguments.command = "observe";
    correlation.consume("b", update, terminal, "in_progress");
    update.rawInput.arguments.command = "Timeline.zoom";
    correlation.consume("b", update, terminal, "completed"); // Duplicate terminal from an old call.
    correlation.consume("b", update, terminal); // No call started in the current turn.
    assert.equal(events.length, 1);
    correlation.consume("b", update, terminal, "in_progress");
    assert.equal(events.length, 2);
    assert.deepEqual(events[1], ["b", contexts.get("b").turn, "completed"]);
});

test("preserves business results and hides receipt blocks from live and replayed tool history", () => {
    const id = randomUUID();
    const source = rawUpdate(id);
    source.rawOutput.result.structuredContent = { rank: 0 };
    const tool = normalizeToolCall({ ...source, toolCallId: "call", status: "completed" });
    assert.ok(tool.output.includes("zoomed"));
    assert.ok(tool.output.includes("rank"));
    assert.ok(!tool.output.includes(id));
    const serialized = normalizeToolCall({ ...source, rawOutput: JSON.stringify(source.rawOutput), toolCallId: "call", status: "completed" });
    assert.ok(!serialized.output.includes(id));
    assert.ok(serialized.output.includes("zoomed"));
    const blocks = source.rawOutput.result.content.map(content => ({ type: "content", content }));
    const rendered = normalizeToolCall({ toolCallId: "call", status: "completed", content: blocks });
    assert.equal(rendered.progress, "zoomed");
    source.rawOutput.result.content[0].text = "The selected rank changed.";
    const failed = normalizeToolCall({ ...source, toolCallId: "call", status: "failed" });
    assert.equal(failed.output, "The selected rank changed.");
});

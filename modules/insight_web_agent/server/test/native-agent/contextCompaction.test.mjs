/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import { acceptSummary, planCompaction, validateCheckpoint } from "../../native-agent/runtime/contextCompaction.mjs";

const history = (size = 1000) => Array.from({ length: 5 }, (_, index) => [
    { role: "user", content: `question ${index}` },
    { role: "assistant", content: "x".repeat(size) },
]).flat();
const checkpoint = (count = 4) => ({ version: 1, summary: "Previous work", coveredMessageCount: count, createdAt: new Date().toISOString() });

test("checkpoints require a safe boundary and reject unknown tool outcomes", () => {
    const messages = history();
    for (const invalid of [null, {}, checkpoint(99), checkpoint(3), { ...checkpoint(), summary: " " }]) {
        assert.equal(validateCheckpoint(invalid, messages), undefined);
    }
    const calls = [
        { role: "user", content: "tool" },
        { role: "assistant", content: [{ type: "tool-call", toolCallId: "a", toolName: "observe", input: {} }] },
        // Split the call from its result: neither prefix may be covered.
        { role: "user", content: "next" },
        ...history(),
    ];
    assert.equal(validateCheckpoint(checkpoint(2), calls), undefined);
    calls.splice(2, 0, { role: "tool", content: [{ type: "tool-result", toolCallId: "a", toolName: "observe", output: { type: "error-text", value: "Side effects may have occurred" } }] });
    // Protocol pairing alone is not a safe boundary when the real outcome is unknown.
    assert.equal(validateCheckpoint(checkpoint(3), calls), undefined);
    calls[2].content[0].output.value = "Request explicitly rejected";
    assert.ok(validateCheckpoint(checkpoint(3), calls));
});

test("the whole pre-turn history is covered and oversized inputs are refused", () => {
    const messages = [...history(), { role: "user", content: "current turn" }];
    const plan = planCompaction({ messages, turnStart: 10, budget: 1800 });
    assert.equal(plan.needed, true);
    assert.equal(plan.coveredMessageCount, 10);
    assert.equal(plan.canSummarize, true);
    // A turn or summary request that cannot fit the budget must not be attempted.
    assert.equal(planCompaction({ messages: [...history(10000)], turnStart: 10, budget: 9000 }).canSummarize, false);
});

test("only complete, nonempty and reducing summaries are accepted", () => {
    const messages = history();
    const plan = planCompaction({ messages, turnStart: 10, budget: 1800 });
    assert.equal(acceptSummary({ text: "", finishReason: "stop", plan, messages, budget: 1800 }), undefined);
    assert.equal(acceptSummary({ text: "short", finishReason: "length", plan, messages, budget: 1800 }), undefined);
    assert.equal(acceptSummary({ text: "x".repeat(10000), finishReason: "stop", plan, messages, budget: 1800 }), undefined);
    assert.equal(acceptSummary({ text: "short", finishReason: "stop", plan, messages, budget: 1800 }).coveredMessageCount, 10);
});

/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import test from "node:test";
import { ensureHistoryMessage } from "../../services/historyMessageService.mjs";

test("only unbound replay messages may claim an agent message ID", () => {
    const messages = [];
    const toolMessage = ensureHistoryMessage(messages, "assistant");
    assert.equal(ensureHistoryMessage(messages, "assistant", "answer"), toolMessage);
    assert.equal(toolMessage.id, "answer");
    const other = ensureHistoryMessage(messages, "assistant", "other");
    assert.notEqual(other, toolMessage);
    assert.equal(messages.length, 2);
    assert.equal(ensureHistoryMessage(messages, "assistant", "answer"), toolMessage);
    assert.equal(messages.length, 2);
});

test("message IDs do not cross roles, sessions or preexisting identities", () => {
    const messages = [{ id: "local", role: "assistant", content: [] }];
    const identified = ensureHistoryMessage(messages, "assistant", "remote");
    assert.notEqual(identified, messages[0]);
    assert.notEqual(ensureHistoryMessage(messages, "user", "remote"), identified);
    assert.notEqual(ensureHistoryMessage([], "assistant", "remote"), identified);
});

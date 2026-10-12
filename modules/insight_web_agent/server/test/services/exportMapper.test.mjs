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
import { test } from "node:test";
import { messagesFromExport } from "../../services/exportMapper.mjs";

test("messagesFromExport preserves ordered content and ignores file parts", () => {
    const messages = messagesFromExport({
        messages: [{
            info: { id: "msg-1", role: "assistant" },
            parts: [
                { id: "text-1", type: "text", text: "before" },
                { id: "file-1", type: "file", mime: "image/png", url: "data:image/png;base64,iVBORw0KGgo=" },
                { id: "call-1", type: "tool", callID: "call-1", tool: "Read", state: { status: "completed", output: "done" } },
                { id: "reason-1", type: "reasoning", text: "thinking" },
                { id: "text-2", type: "text", text: "after" },
            ],
        }],
    });

    assert.deepEqual(messages[0].content.map((block) => block.type), ["text", "tool", "thinking", "text"]);
    assert.equal(messages[0].content.some((block) => block.type === "image"), false);
});

test("messagesFromExport drops messages containing only file parts", () => {
    assert.deepEqual(messagesFromExport({
        messages: [{
            info: { id: "msg-1", role: "user" },
            parts: [{ id: "file-1", type: "file", mime: "image/jpeg", url: "data:image/jpeg;base64,/9j/4AAQSkZJRg==" }],
        }],
    }), []);
});

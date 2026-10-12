/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { createToolRegistry } from "../../native-agent/tools/ToolRegistry.mjs";

const tool = (name) => ({
    name,
    inputSchema: { type: "object", properties: {} },
    execute: async () => ({}),
});

test("Native Tool Registry rejects duplicate names", () => {
    assert.throws(
        () => createToolRegistry({ tools: [tool("duplicate"), tool("duplicate")] }),
        /already registered/,
    );
});

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
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { strict as assert } from "node:assert";
import test from "node:test";
import { createAuditLogger } from "../../observability/auditLogger.mjs";

test("audit logger writes JSONL events", async () => {
    const cwd = await mkdtemp(join(tmpdir(), "insight-audit-"));
    try {
        const logger = createAuditLogger({ cwd, debug: false });
        await logger.sessionStart({ sessionId: "s1", agentId: "a1", mode: "free_chat" });
        await logger.contextAssembled("s1", { contextProviders: [{ name: "structured" }] });
        await logger.sessionEnd("s1");

        const lines = (await readFile(join(cwd, "audit.log"), "utf8")).trim().split("\n");
        assert.equal(lines.length, 3);
        for (const line of lines) assert.doesNotThrow(() => JSON.parse(line));
    } finally {
        await rm(cwd, { recursive: true, force: true });
    }
});

test("audit logger writes debug output", async () => {
    const cwd = await mkdtemp(join(tmpdir(), "insight-audit-debug-"));
    const originalError = console.error;
    let called = false;
    console.error = () => { called = true; };
    try {
        const logger = createAuditLogger({ cwd, debug: true });
        await logger.toolCall("s1", "capability", 2);
        assert.equal(called, true);
    } finally {
        console.error = originalError;
        await rm(cwd, { recursive: true, force: true });
    }
});

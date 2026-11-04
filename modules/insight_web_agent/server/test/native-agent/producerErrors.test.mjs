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
import { test } from "node:test";
import { createBashTools } from "../../native-agent/tools/bashTools.mjs";
import { createSkillTools } from "../../native-agent/tools/skillTools.mjs";
import { createToolRegistry } from "../../native-agent/tools/ToolRegistry.mjs";
import { createCapabilityTools } from "../../native-agent/tools/capabilityTools.mjs";

const id = "00000000-0000-4000-8000-000000000001";
const context = { sessionId: "s", diagnosticId: id };
const session = { sessionId: "s", permissionMode: "full-access", primaryAgentBashRules: [] };
const check = (code, stage, outcome) => (error) => {
    assert.equal(error.code, code);
    assert.equal(error.details.diagnosticId, id);
    assert.equal(error.details.stage, stage);
    assert.equal(error.details.outcome, outcome);
    assert.doesNotMatch(JSON.stringify(error), /private-secret|\[object Object\]/);
    return true;
};
const bashTool = (extra = {}) => createBashTools({
    sessions: new Map([["s", session]]), hostClient: {}, cwd: process.cwd(), ...extra,
})[0];

test("Bash validation, session, policy and startup errors are typed at their real stage", async () => {
    await assert.rejects(bashTool({ sessions: new Map() }).execute({}, context), check("BASH_SESSION_UNAVAILABLE", "validate", "not_started"));
    await assert.rejects(bashTool().execute({}, context), check("BASH_INPUT_INVALID", "validate", "not_started"));
    await assert.rejects(bashTool().execute({ command: "git reset --hard" }, context), check("BASH_POLICY_DENIED", "authorize", "not_started"));
    const tool = bashTool({ spawnProcess: () => { throw Object.assign(new Error("token=private-secret"), { code: "ENOENT" }); } });
    await assert.rejects(tool.execute({ command: "echo test" }, context), (error) => {
        check("BASH_START_FAILED", "execute", "not_started")(error);
        assert.equal(error.details.causeCode, "ENOENT");
        return true;
    });
});

test("Bash ordinary nonzero remains completed data; capture-limit errors retain stderr", async () => {
    for (const limited of [false, true]) {
        const child = new EventEmitter();
        child.stdout = new EventEmitter();
        child.stderr = new EventEmitter();
        child.kill = () => {};
        const tool = bashTool({ spawnProcess: () => {
            queueMicrotask(() => {
                child.stderr.emit("data", Buffer.from("missing table"));
                if (limited) child.stdout.emit("data", Buffer.alloc(205000, 120));
                child.emit("close", 2, null);
            });
            return child;
        } });
        if (!limited) assert.equal((await tool.execute({ command: "echo test" }, context)).exitCode, 2);
        else await assert.rejects(tool.execute({ command: "echo test" }, context), (error) => {
            check("BASH_OUTPUT_LIMIT", "execute", "failed")(error);
            assert.match(error.details.stderr, /missing table/);
            assert.equal(error.details.truncation.captureLimited, true);
            return true;
        });
    }
});

test("missing skill has bounded candidates, load errors keep known causes and unknown tools are typed", async () => {
    const [skill] = createSkillTools({ skillRegistry: {
        list: () => Array.from({ length: 30 }, (_, i) => ({ name: `skill-${i}`, description: "" })),
        load: async () => undefined,
    } });
    await assert.rejects(skill.execute({ name: "missing" }, context), (error) => {
        check("SKILL_UNAVAILABLE", "validate", "not_started")(error);
        assert.ok(error.details.availableSkills.length <= 20);
        assert.equal(error.details.total, 30);
        return true;
    });
    const [broken] = createSkillTools({ skillRegistry: { list: () => [], load: async () => { throw Object.assign(new Error("token=private-secret"), { code: "EACCES" }); } } });
    await assert.rejects(broken.execute({ name: "broken" }, context), (error) => error.code === "SKILL_LOAD_FAILED" && error.details.causeCode === "EACCES");
    await assert.rejects(createToolRegistry().execute("missing", {}, context), check("TOOL_NOT_FOUND", "validate", "not_started"));
});

test("local capability session/approval failures never invoke transport", async () => {
    const definitions = [{ name: "pt_snap", requiresApproval: true }];
    let requests = 0;
    const [tool] = createCapabilityTools({ definitions, sessions: new Map(), fetchImpl: () => { requests++; } });
    await assert.rejects(tool.execute({}, context), check("CAPABILITY_SESSION_UNAVAILABLE", "validate", "not_started"));
    const [denied] = createCapabilityTools({ definitions, sessions: new Map([["s", { sessionId: "s" }]]),
        hostClient: { request: async () => ({ outcome: { optionId: "deny" } }) }, fetchImpl: () => { requests++; },
    });
    await assert.rejects(denied.execute({}, context), check("CAPABILITY_DENIED_BY_USER", "authorize", "not_started"));
    assert.equal(requests, 0);
});

test("Bash timeout and cancellation keep captured diagnostics without replaying", async () => {
    for (const timeout of [false, true]) {
        const controller = new AbortController();
        let executions = 0;
        const tool = bashTool({ spawnProcess: () => {
            executions++;
            const child = new EventEmitter();
            child.stdout = new EventEmitter();
            child.stderr = new EventEmitter();
            child.kill = () => queueMicrotask(() => child.emit("close", null, "SIGTERM"));
            setImmediate(() => {
                child.stderr.emit("data", "partial stderr; token=private-secret");
                if (!timeout) controller.abort(new Error("private-secret"));
            });
            return child;
        } });
        await assert.rejects(tool.execute({ command: "echo fixture", timeout: 1000 }, { ...context, signal: controller.signal }), (error) => {
            check(timeout ? "BASH_TIMEOUT" : "BASH_CANCELLED", "execute", "unknown")(error);
            assert.match(error.details.stderr, /partial stderr/);
            assert.equal(error.details.exitCode, null);
            assert.equal(error.details.signal, "SIGTERM");
            assert.equal(error.retryable, false);
            return true;
        });
        assert.equal(executions, 1);
    }
});

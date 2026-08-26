/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { mkdtemp, mkdir, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { evaluateBashPolicy, normalizeBashInput } from "../../native-agent/permissions/bashPolicy.mjs";

test("product deny overrides Agent allow inside compound commands", () => {
    for (const command of [
        "pt-snap query --list; sudo reboot",
        "pt-snap query --list && rm -rf /tmp/data",
        "pt-snap query --list $(shutdown now)",
        "pt-snap query --list; /bin/rm -rf /tmp/data",
        "pt-snap query --list; r\\m -rf /tmp/data",
        "pt-snap query --list; 'sudo' reboot",
        "pt-snap query --list; rm -r -f /tmp/data",
        "pt-snap query --list; git push -f",
        "pt-snap query --list; git -C /tmp/repo reset --hard",
        "pt-snap query --list; git -C /tmp/repo clean -fd",
        "pt-snap query --list &",
        "nohup pt-snap query --list",
    ]) {
        assert.equal(evaluateBashPolicy({
            command,
            rules: [{ pattern: "pt-snap query *", behavior: "allow" }],
        }).behavior, "deny");
    }
});

test("quoted ampersands do not count as background execution", () => {
    assert.equal(evaluateBashPolicy({
        command: "printf 'a & b'",
        rules: [{ pattern: "printf *", behavior: "allow" }],
    }).behavior, "ask");
});

test("redirection ampersands require approval instead of product denial", () => {
    assert.equal(evaluateBashPolicy({
        command: "python -V 2>&1",
        rules: [{ pattern: "python *", behavior: "allow" }],
    }).behavior, "ask");
});

test("Agent allow applies to simple commands but compound shell syntax asks", () => {
    const rules = [
        { pattern: "pt-snap query *", behavior: "allow" },
        { pattern: "*", behavior: "ask" },
    ];

    assert.deepEqual(evaluateBashPolicy({
        command: "pt-snap query --list",
        rules,
    }), {
        behavior: "allow",
        normalizedRule: "pt-snap query *",
        message: undefined,
    });

    for (const command of [
        "pt-snap query --list; python -V",
        "pt-snap query --list && python -V",
        "pt-snap query --list | tee output.txt",
        "pt-snap query --list > output.txt",
        "pt-snap query --list\npython -V",
        "pt-snap query $(python -V)",
        "pt-snap query `python -V`",
    ]) {
        const result = evaluateBashPolicy({ command, rules });
        assert.equal(result.behavior, "ask");
        assert.equal(result.normalizedRule, command.trim());
    }
});

test("catch-all ask never creates a catch-all approval key", () => {
    const command = "python -c \"print('hello')\"";
    assert.deepEqual(evaluateBashPolicy({
        command,
        rules: [{ pattern: "*", behavior: "ask" }],
    }), {
        behavior: "ask",
        normalizedRule: command,
        message: "Compound shell commands require user approval",
    });
});

test("specific ask rules retain their declared approval scope", () => {
    assert.deepEqual(evaluateBashPolicy({
        command: "python -m pip install pt-snap-cli",
        rules: [
            { pattern: "python -m pip install *", behavior: "ask" },
            { pattern: "*", behavior: "ask" },
        ],
    }), {
        behavior: "ask",
        normalizedRule: "python -m pip install *",
        message: undefined,
    });
});

test("wildcards can match inside command patterns", () => {
    assert.equal(evaluateBashPolicy({
        command: "python -m pip show pt-snap-cli",
        rules: [
            { pattern: "python * pip show *", behavior: "allow" },
            { pattern: "*", behavior: "ask" },
        ],
    }).behavior, "allow");
});

test("Bash input resolves relative cwd and requires a bounded timeout", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "msinsight-bash-policy-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const workspace = join(root, "workspace");
    const outside = join(root, "outside");
    await Promise.all([mkdir(workspace), mkdir(outside)]);
    const session = { canonicalFilesystemRoots: [workspace] };

    assert.deepEqual(await normalizeBashInput({ command: "python -V" }, session, workspace), {
        command: "python -V",
        timeout: 30000,
        cwd: workspace,
        run_in_background: false,
    });
    assert.equal((await normalizeBashInput({ command: "python -V", cwd: "." }, session, workspace)).cwd, workspace);
    assert.equal((await normalizeBashInput({ command: "python -V", cwd: "../outside" }, session, workspace)).cwd, outside);
    assert.equal((await normalizeBashInput({ command: "python -V", cwd: outside }, session, workspace)).cwd, outside);
    await assert.rejects(normalizeBashInput({ command: "python -V", timeout: 999 }, session, workspace), /between 1000 and 300000/);
    await assert.rejects(normalizeBashInput({ command: "python -V", timeout: 300001 }, session, workspace), /between 1000 and 300000/);
});

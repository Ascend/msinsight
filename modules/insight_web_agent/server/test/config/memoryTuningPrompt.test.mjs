/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { strict as assert } from "node:assert";
import test from "node:test";
import { formatShellCommand, PT_SNAP_COMMAND_PLACEHOLDER, renderMemoryTuningPrompt } from "../../config/memoryTuningPrompt.mjs";

const prompt = `Intro\n\n${PT_SNAP_COMMAND_PLACEHOLDER}\n\nBody`;

test("renders the resolved builtin pt-snap command into the memory prompt", () => {
    const rendered = renderMemoryTuningPrompt(prompt, {
        platform: "darwin",
        capabilities: [{ name: "pt_snap", executable: "/Applications/MindStudio Insight.app/Contents/Resources/python/bin/python3", argsPrefix: ["-m", "pt_snap_cli.cli"] }],
    });
    assert.equal(rendered.includes(PT_SNAP_COMMAND_PLACEHOLDER), false);
    assert.ok(rendered.includes("'/Applications/MindStudio Insight.app/Contents/Resources/python/bin/python3' -m pt_snap_cli.cli"));
    assert.ok(rendered.startsWith("Intro\n\n"));
    assert.ok(rendered.endsWith("\n\nBody"));
});

test("quotes Windows builtin interpreter paths containing spaces", () => {
    const rendered = renderMemoryTuningPrompt(prompt, {
        platform: "win32",
        capabilities: [{ name: "pt_snap", executable: "C:\\Program Files\\MindStudio Insight\\resources\\profiler\\server\\python\\python.exe", argsPrefix: ["-m", "pt_snap_cli.cli"] }],
    });
    assert.ok(rendered.includes("\"C:\\Program Files\\MindStudio Insight\\resources\\profiler\\server\\python\\python.exe\" -m pt_snap_cli.cli"));
    assert.ok(rendered.includes("PowerShell"));
    assert.equal(formatShellCommand(["C:\\python\\python.exe", "-m", "pt_snap_cli.cli"], "win32"), "C:\\python\\python.exe -m pt_snap_cli.cli");
});

test("renders a PATH-resolved pt-snap without an argument prefix", () => {
    const rendered = renderMemoryTuningPrompt(prompt, {
        platform: "linux",
        capabilities: [{ name: "pt_snap", executable: "/usr/local/bin/pt-snap", argsPrefix: [] }],
    });
    assert.ok(rendered.includes("```text\n/usr/local/bin/pt-snap\n```"));
});

test("reports pt-snap as unavailable when the capability was not resolved", () => {
    for (const capabilities of [[], [{ name: "pt_snap", executable: undefined, argsPrefix: [] }], [{ name: "other", executable: "/bin/other" }]]) {
        const rendered = renderMemoryTuningPrompt(prompt, { platform: "linux", capabilities });
        assert.equal(rendered.includes(PT_SNAP_COMMAND_PLACEHOLDER), false);
        assert.ok(rendered.includes("当前构建未找到内置 pt-snap"));
    }
});

test("leaves prompts without the placeholder or empty prompts unchanged", () => {
    assert.equal(renderMemoryTuningPrompt("", { capabilities: [] }), "");
    assert.equal(renderMemoryTuningPrompt("Plain", { capabilities: [] }), "Plain");
    assert.equal(formatShellCommand(["/opt/a'b/python3"], "linux"), "'/opt/a'\\''b/python3'");
});

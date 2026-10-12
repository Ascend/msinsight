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
import { resolvePowerShellExecutable, resolveShellRuntime } from "../../native-agent/tools/shellRuntime.mjs";

test("PowerShell resolution honors an explicit executable override", () => {
    assert.equal(resolvePowerShellExecutable({
        configured: "D:\\Tools\\pwsh.exe",
        platform: "win32",
        env: {},
        candidateExists: () => false,
    }), "D:\\Tools\\pwsh.exe");
});

test("PowerShell resolution prefers the standard PowerShell 7 installation", () => {
    const expected = "C:\\Program Files\\PowerShell\\7\\pwsh.exe";
    assert.equal(resolvePowerShellExecutable({
        platform: "win32",
        env: {
            ProgramFiles: "C:\\Program Files",
            SystemRoot: "C:\\Windows",
            PATH: "D:\\StoreAlias;D:\\Other",
        },
        candidateExists: path => path === expected,
    }), expected);
});

test("PowerShell resolution accepts a quoted PATH pwsh alias", () => {
    const expected = "D:\\Store Alias\\pwsh.exe";
    assert.equal(resolvePowerShellExecutable({
        platform: "win32",
        env: {
            ProgramFiles: "C:\\Program Files",
            SystemRoot: "C:\\Windows",
            PATH: "\"D:\\Store Alias\";D:\\Other",
        },
        candidateExists: path => path === expected,
    }), expected);
});

test("PowerShell resolution falls back to Windows PowerShell 5.1", () => {
    const expected = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    assert.equal(resolvePowerShellExecutable({
        platform: "win32",
        env: { ProgramFiles: "C:\\Program Files", SystemRoot: "C:\\Windows", PATH: "" },
        candidateExists: path => path === expected,
    }), expected);
});

test("PowerShell resolution retains a clear spawn fallback when none is discovered", () => {
    assert.equal(resolvePowerShellExecutable({
        platform: "win32",
        env: {},
        candidateExists: () => false,
    }), "powershell.exe");
});

test("native shell runtime uses Bash on Unix-like platforms", () => {
    const runtime = resolveShellRuntime({ platform: "linux", env: {} });
    assert.equal(runtime.kind, "bash");
    assert.equal(runtime.executable, "bash");
    assert.deepEqual(runtime.commandArgs, ["-lc"]);
    assert.equal(runtime.displayName, "Bash");
    assert.match(runtime.description, /Bash command/);
    assert.match(runtime.modelGuidance, /Bash syntax/);
    assert.equal(runtime.prepareCommand("printf hello"), "printf hello");
});

test("native shell runtime uses Windows PowerShell by default on Windows", () => {
    const runtime = resolveShellRuntime({
        platform: "win32",
        env: {},
        resolvePowerShell: () => "C:\\Program Files\\PowerShell\\7\\pwsh.exe",
    });
    assert.equal(runtime.kind, "powershell");
    assert.equal(runtime.executable, "C:\\Program Files\\PowerShell\\7\\pwsh.exe");
    assert.deepEqual(runtime.commandArgs, ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command"]);
    assert.equal(runtime.displayName, "PowerShell");
    assert.match(runtime.description, /PowerShell command/);
    assert.match(runtime.modelGuidance, /compatibility tool name "Bash"/);
    assert.match(runtime.modelGuidance, /PowerShell syntax/);
    const prepared = runtime.prepareCommand("Write-Output '中文'");
    assert.match(prepared, /^\[Console\]::OutputEncoding = \[System\.Text\.UTF8Encoding\]::new\(\$false\); /);
    assert.match(prepared, /\$OutputEncoding = \[System\.Text\.UTF8Encoding\]::new\(\$false\); /);
    assert.match(prepared, /Write-Output '中文'$/);
});

test("native shell runtime accepts an explicit executable override", () => {
    const runtime = resolveShellRuntime({ platform: "win32", env: { MSINSIGHT_NATIVE_SHELL_PATH: "pwsh.exe" } });
    assert.equal(runtime.kind, "powershell");
    assert.equal(runtime.executable, "pwsh.exe");
});

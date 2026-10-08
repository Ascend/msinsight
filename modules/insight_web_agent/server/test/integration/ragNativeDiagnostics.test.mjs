/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { test } from "node:test";
import { fileURLToPath } from "node:url";
import { hasNativeLoadFailure, runNativeFailureDiagnostics, windowsProbeArguments } from "../../../scripts/rag-native-diagnostics.mjs";
import { resolveRagTarget } from "../../services/rag/platformSupport.mjs";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));
const scriptsDir = join(packageRoot, "scripts");

test("native probes require a structured ONNX import failure", () => {
    const failure = { event: "rag_smoke_failure", stage: "onnx_import", error: { code: "native_runtime_load_failed" } };
    assert.equal(hasNativeLoadFailure(`unrelated log\n${JSON.stringify(failure)}\n`), true);
    assert.equal(hasNativeLoadFailure("native_runtime_load_failed"), false);
    assert.equal(hasNativeLoadFailure(JSON.stringify({ ...failure, stage: "rag_retrieve" })), false);
    assert.equal(hasNativeLoadFailure(JSON.stringify({ ...failure, error: { code: "onnx_initialization_failed" } })), false);
    assert.equal(hasNativeLoadFailure(null), false);
});

test("native probe orchestration isolates source bundle and Windows processes with bounded redacted output", () => {
    const events = [];
    const calls = [];
    const distDir = join(packageRoot, "private-bundle");
    runNativeFailureDiagnostics({
        packageRoot, distDir, platform: "win32", emit: (event) => events.push(event),
        spawn(command, args, options) {
            calls.push({ command, args, options });
            return { status: 1, signal: null, stdout: JSON.stringify({ error: { code: "ERR_DLOPEN_FAILED", message: `Cannot load ${join(distDir, "rag-runtime", "model.onnx")}` } }), stderr: "password=private-credential" };
        },
    });
    assert.equal(calls.length, 3);
    assert.equal(calls[0].command, process.execPath);
    assert.deepEqual(calls[0].args.slice(1), ["source", packageRoot]);
    assert.deepEqual(calls[1].args.slice(1), ["bundle", distDir]);
    assert.equal(calls[1].options.cwd, distDir);
    assert.match(calls[2].command, /powershell\.exe$/);
    assert.ok(calls.every(({ options }) => options.timeout > 0 && options.timeout <= 20_000));
    const probe = events.find(({ probe }) => probe === "node_bundle");
    assert.equal(probe.records[0].error.code, "ERR_DLOPEN_FAILED");
    assert.equal(probe.records[0].error.message.includes(distDir), false);
    assert.equal(JSON.stringify(events).includes("private-credential"), false);
    assert.equal(events.at(-1).event, "rag_native_probes_end");
});

test("native probes retain partial output on timeout and stop at the shared deadline", () => {
    let clock = 0;
    const events = [];
    const timeouts = [];
    runNativeFailureDiagnostics({
        packageRoot, distDir: packageRoot, platform: "win32", now: () => clock,
        emit: (event) => events.push(event),
        spawn(command, args, options) {
            timeouts.push(options.timeout);
            clock += timeouts.length === 1 ? 50_000 : 10_000;
            return { status: null, signal: "SIGTERM", stdout: '{"event":"partial_evidence"}\n', stderr: "", error: Object.assign(new Error("probe timeout"), { code: "ETIMEDOUT" }) };
        },
    });
    assert.deepEqual(timeouts, [20_000, 10_000]);
    const first = events.find(({ probe }) => probe === "node_source");
    assert.equal(first.error.code, "ETIMEDOUT");
    assert.equal(first.records[0].event, "partial_evidence");
    assert.equal(events.find(({ probe }) => probe === "windows_dll").reason, "budget_exhausted");
});

test("missing diagnostic executables do not abort subsequent probes", () => {
    const events = [];
    runNativeFailureDiagnostics({
        packageRoot, distDir: packageRoot, platform: "win32", emit: (event) => events.push(event),
        spawn() { throw Object.assign(new Error("missing executable"), { code: "ENOENT" }); },
    });
    assert.equal(events.filter(({ outcome }) => outcome === "probe_error").length, 3);
    assert.ok(events.filter(({ error }) => error).every(({ error }) => error.code === "ENOENT"));
    assert.equal(events.at(-1).event, "rag_native_probes_end");
});

test("non-Windows failure diagnostics run only Node probes and bound unexpected output", () => {
    const events = [];
    runNativeFailureDiagnostics({
        packageRoot, distDir: packageRoot, platform: "linux", emit: (event) => events.push(event),
        spawn() { return { status: 0, signal: null, stdout: "x".repeat(30_000), stderr: "y".repeat(10_000) }; },
    });
    assert.equal(events.filter(({ probe }) => probe).length, 2);
    assert.ok(JSON.stringify(events).length < 45_000);
    assert.match(events[1].records[0].text, /truncated/);
});

for (const scope of ["source", "bundle"]) {
    test(`isolated ${scope} Node probe reports native digests and import success`, async (t) => {
        const root = await fixtureRoot(t);
        const bytes = Buffer.from(`native-${scope}`);
        await prepareNodeFixture(root, bytes, "module.exports = {};");
        const result = runNodeProbe(scope, root);
        assert.equal(result.status, 0, result.stderr);
        const records = parseRecords(result.stdout);
        assert.equal(records[0].scope, scope);
        assert.equal(records[0].version, "1.22.0");
        assert.equal(records[0].files[0].sha256, createHash("sha256").update(bytes).digest("hex"));
        assert.equal(records.at(-1).outcome, "passed");
    });
}

test("isolated Node probe reports DLL load failure without exposing root paths or environment secrets", async (t) => {
    const root = await fixtureRoot(t);
    await prepareNodeFixture(root, Buffer.from("native"), `throw Object.assign(new Error(${JSON.stringify(`Cannot initialize ${root}`)}), {code:'ERR_DLOPEN_FAILED'});`);
    const result = runNodeProbe("bundle", root);
    assert.equal(result.status, 1);
    const failure = parseRecords(result.stdout).at(-1);
    assert.equal(failure.phase, "onnx_import");
    assert.equal(failure.error.code, "ERR_DLOPEN_FAILED");
    assert.equal(failure.error.message.includes(root), false);
    assert.equal(result.stdout.includes("private-environment-secret"), false);
});

test("bundle Node probe does not fall back to a source dependency when its packaged entry is missing", async (t) => {
    const root = await fixtureRoot(t);
    const result = runNodeProbe("bundle", root);
    assert.equal(result.status, 1);
    const failure = parseRecords(result.stdout).at(-1);
    assert.equal(failure.phase, "module_resolution");
    assert.equal(failure.error.code, "ENOENT");
});

for (const missing of [false, true]) {
    test(`Windows DLL probe reports a real ${missing ? "loader error number" : "successful DLL initialization"}`, {
        skip: process.platform !== "win32",
    }, async (t) => {
        const root = await fixtureRoot(t);
        const systemRoot = process.env.SystemRoot ?? "C:\\Windows";
        const result = spawnSync(join(systemRoot, "System32", "WindowsPowerShell", "v1.0", "powershell.exe"), windowsProbeArguments(), {
            env: { ...process.env, MSINSIGHT_RAG_PROBE_DLL: missing ? join(root, "missing.dll") : join(systemRoot, "System32", "kernel32.dll"), MSINSIGHT_RAG_PROBE_NODE_DIR: dirname(process.execPath), MSINSIGHT_RAG_PROBE_SCRIPT: join(scriptsDir, "probe-rag-native-windows.ps1") },
            input: JSON.stringify({ files: [{ path: "fixture.node", imports: [
                { dll: "kernel32.dll", kind: "normal", symbols: ["GetCurrentProcessId", "MissingRagProbeFunction", 65535] },
                { dll: "missing-rag-probe.dll", kind: "normal", symbols: [] },
                { dll: "optional-rag-probe.dll", kind: "delay", symbols: [] },
                { dll: "node.exe", kind: "delay", symbols: ["napi_create_object"] },
            ] }] }),
            encoding: "utf8", windowsHide: true, timeout: 20_000, maxBuffer: 256 * 1024,
        });
        assert.equal(result.error, undefined);
        assert.equal(result.status, 0, result.stderr);
        const records = parseRecords(result.stdout);
        assert.equal(records.some(({ event }) => event === "rag_native_windows_environment"), true);
        assert.equal(records.some(({ event }) => event === "rag_native_windows_runtimes"), true);
        const load = records.find(({ event }) => event === "rag_native_windows_dll_load");
        assert.equal(load.host, "powershell");
        assert.equal(load.searchPolicy, "dll-directory+System32");
        assert.equal(load.loaded, !missing);
        assert.equal(load.winError, missing ? 126 : 0);
        const cpu = records.find(({ event }) => event === "rag_native_cpu_features");
        assert.equal(cpu.features.avx.featureId, 39);
        assert.equal(cpu.features.avx2.featureId, 40);
        assert.equal(typeof cpu.features.avx.reportedAvailable, "boolean");
        const dependencies = records.filter(({ event }) => event === "rag_native_dependency");
        assert.equal(dependencies[0].loaded, true);
        assert.deepEqual(dependencies[0].missingSymbols.sort(), ["#65535", "MissingRagProbeFunction"].sort());
        assert.equal(dependencies[1].winError, 126);
        assert.equal(dependencies[2].requiredForImport, false);
        assert.equal(dependencies[3].outcome, "host_bound");
    });
}

async function fixtureRoot(t) {
    const root = await mkdtemp(join(tmpdir(), "rag-native-probe-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    return root;
}

async function prepareNodeFixture(root, bytes, code) {
    const directory = join(root, "node_modules", "onnxruntime-node");
    await mkdir(join(directory, "dist"), { recursive: true });
    await writeFile(join(directory, "package.json"), JSON.stringify({ name: "onnxruntime-node", version: "1.22.0", main: "dist/index.js" }));
    await writeFile(join(directory, "dist", "index.js"), code);
    for (const file of resolveRagTarget().onnx.files) {
        await mkdir(dirname(join(directory, file)), { recursive: true });
        await writeFile(join(directory, file), bytes);
    }
}

function runNodeProbe(scope, root) {
    return spawnSync(process.execPath, [join(scriptsDir, "probe-rag-native.mjs"), scope, root], {
        cwd: packageRoot, env: { ...process.env, PRIVATE_PROBE_TEST_TOKEN: "private-environment-secret" },
        encoding: "utf8", windowsHide: true, timeout: 10_000,
    });
}

function parseRecords(stdout) {
    return stdout.trim().split(/\r?\n/).map((line) => JSON.parse(line));
}

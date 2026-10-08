/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { test } from "node:test";
import { pathToFileURL } from "node:url";
import { EmbeddingRuntimeError } from "../../../services/rag/embeddingRuntime.mjs";
import { formatSmokeError, redactSmokeText, smokeEnvironment, smokeFailureStage, smokeProcessSnapshot } from "../../../services/rag/smokeDiagnostics.mjs";

test("smoke errors retain loader causes while redacting sensitive messages and stacks", () => {
    const directory = join(tmpdir(), "private model");
    const query = "private smoke query";
    const knowledge = "private knowledge\nsecond line";
    const cause = Object.assign(new Error(`Cannot load ${directory}/model.onnx; ${query}; ${knowledge}; token=secret-value`), {
        code: "ERR_DLOPEN_FAILED",
    });
    const error = new EmbeddingRuntimeError("native_runtime_load_failed", "Unable to load native runtime", { cause });
    const diagnostic = formatSmokeError(error, [directory, query, knowledge]);
    assert.equal(diagnostic.code, "native_runtime_load_failed");
    assert.equal(diagnostic.cause.code, "ERR_DLOPEN_FAILED");
    assert.match(diagnostic.cause.message, /Cannot load/);
    assert.match(diagnostic.cause.stack, /Error: Cannot load/);
    const output = JSON.stringify(diagnostic);
    for (const value of [directory, query, "private knowledge", "secret-value"]) assert.equal(output.includes(value), false);
});

test("smoke redaction covers Windows, URL, and JSON-escaped path forms", () => {
    const path = "C:\\private workspace\\rag-runtime";
    for (const value of [path, path.replaceAll("\\", "/"), JSON.stringify(path)]) {
        assert.equal(redactSmokeText(value, [path]).includes("private workspace"), false);
    }
    const localPath = join(tmpdir(), "private workspace", "rag-data");
    assert.equal(redactSmokeText(pathToFileURL(localPath).href, [localPath]).includes("private%20workspace"), false);
    assert.equal(redactSmokeText("Bearer secret-value password=hidden", []).includes("secret-value"), false);
});

test("smoke error formatting bounds long messages and cyclic cause chains", () => {
    const error = new Error("x".repeat(10_000));
    error.cause = error;
    const diagnostic = formatSmokeError(error);
    assert.ok(diagnostic.message.length < 2100);
    assert.ok(diagnostic.stack.length < 4200);
    assert.deepEqual(diagnostic.cause, { message: "[cause chain truncated]" });
    assert.deepEqual(formatSmokeError(null), { name: "Error", message: "" });
    assert.equal(smokeFailureStage({ code: "native_runtime_load_failed" }, "service"), "onnx_import");
    assert.equal(smokeFailureStage({ code: "onnx_initialization_failed" }, "service"), "onnx_initialization");
});

test("smoke error stacks redact multiline knowledge before limiting stack lines", () => {
    const knowledge = Array.from({ length: 20 }, (_, index) => `private knowledge line ${index}`).join("\n");
    const diagnostic = formatSmokeError(new Error(`Retrieval failed: ${knowledge}`), [knowledge]);
    assert.equal(JSON.stringify(diagnostic).includes("private knowledge line"), false);
    assert.match(diagnostic.stack, /Retrieval failed: <redacted>/);
});

test("original process snapshots select module paths and redact sensitive values without dumping report data", () => {
    const privatePath = join(tmpdir(), "private-runtime", "binding.node");
    const snapshot = smokeProcessSnapshot([dirname(privatePath)], () => ({
        sharedObjects: [privatePath], environmentVariables: { token: "private-report-secret" },
    }));
    assert.equal(snapshot.pid, process.pid);
    assert.equal(snapshot.loadedModules[0].path.includes("private-runtime"), false);
    assert.equal(JSON.stringify(snapshot).includes("private-report-secret"), false);
    assert.equal(typeof snapshot.memory.rss, "number");
    const unavailable = smokeProcessSnapshot([], () => { throw new Error("report unavailable"); });
    assert.equal(unavailable.error.message, "report unavailable");
});

test("smoke environment resolves ONNX from the bundle and reports missing native files", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "rag-smoke-environment-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    const entry = join(root, "node_modules", "onnxruntime-node", "dist", "index.js");
    await mkdir(dirname(entry), { recursive: true });
    await writeFile(entry, "throw new Error('startup diagnostics must not load native code');");
    await writeFile(join(root, "node_modules", "onnxruntime-node", "package.json"), JSON.stringify({
        name: "onnxruntime-node", version: "1.22.0", main: "dist/index.js",
    }));
    const present = "node_modules/onnxruntime-node/binding.node";
    await writeFile(join(root, present), "native");
    const environment = smokeEnvironment(root, [present, "missing.dll"]);
    assert.equal(environment.onnx.entry, entry);
    assert.equal(environment.onnx.version, "1.22.0");
    assert.equal(environment.execPath, process.execPath);
    assert.deepEqual(environment.nativeFiles[0], { path: present, exists: true, regularFile: true, sizeBytes: 6 });
    assert.equal(environment.nativeFiles[1].exists, false);
    assert.equal(environment.nativeFiles[1].code, "ENOENT");
});

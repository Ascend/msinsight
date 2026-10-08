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
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { test } from "node:test";
import { fileURLToPath, pathToFileURL } from "node:url";
import { build } from "esbuild";
import { expectedNativeFiles } from "../../services/rag/nativeRuntimeManifest.mjs";
import { resolveRagTarget } from "../../services/rag/platformSupport.mjs";
import { MODEL_CONTRACT } from "../services/rag/packageFixture.mjs";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));
const summary = { status: "passed", failOpen: false, capability: "rag_retrieve", hits: 1, sensitiveLogScan: "passed" };

for (const phase of ["import", "initialization"]) {
    test(`embedding runtime retains the original ONNX ${phase} failure`, async (t) => {
        const root = await fixtureRoot(t);
        const modelDir = join(root, "model");
        const files = ["config.json", "onnx/model.onnx", "tokenizer.json", "tokenizer_config.json", "special_tokens_map.json"];
        const bytes = Buffer.from("fixture");
        for (const file of files) {
            await mkdir(dirname(join(modelDir, file)), { recursive: true });
            await writeFile(join(modelDir, file), bytes);
        }
        await writeFile(join(modelDir, "model-manifest.json"), JSON.stringify({
            ...MODEL_CONTRACT,
            fileDigests: Object.fromEntries(files.map((file) => [file, createHash("sha256").update(bytes).digest("hex")])),
        }));
        const nativeCode = phase === "import"
            ? "throw Object.assign(new Error('The specified module could not be found.'), {code:'ERR_DLOPEN_FAILED'}); export const InferenceSession = {};"
            : "export const InferenceSession = {create: async () => {throw Object.assign(new Error('Invalid ONNX model'), {code:'ORT_INVALID_MODEL'});}};";
        for (const observerThrows of [false, true]) {
            // esbuild 0.28.0 leaves a throwing mock partially initialized; isolate each scenario.
            const scenarioRoot = await fixtureRoot(t);
            const entry = await bundleFixture(scenarioRoot, "services/rag/embeddingRuntime.mjs", { "onnxruntime-node": nativeCode });
            const { createEmbeddingRuntime } = await import(pathToFileURL(entry).href);
            const phases = [];
            const nativeLoadObserver = (phase) => {
                phases.push(phase);
                if (observerThrows) throw new Error("broken observer");
            };
            await assert.rejects(createEmbeddingRuntime({ modelDir, nativeLoadObserver }), (error) => {
                assert.equal(error.code, phase === "import" ? "native_runtime_load_failed" : "onnx_initialization_failed");
                assert.equal(error.cause.code, phase === "import" ? "ERR_DLOPEN_FAILED" : "ORT_INVALID_MODEL");
                assert.match(error.cause.message, phase === "import" ? /specified module/ : /Invalid ONNX model/);
                return true;
            });
            assert.deepEqual(phases, ["before_onnx_import"]);
        }
    });
}

test("smoke entry emits precise loader stage and redacted original cause", async (t) => {
    const root = await fixtureRoot(t);
    const modelDir = join(root, "rag-runtime", "models", "bge-small-zh-v1.5");
    const entry = await bundleFixture(root, "rag-required-smoke.mjs", {
        "./services/rag/ragService.mjs": `export const createRagService = async () => {
            throw Object.assign(new Error('Unable to load native runtime'), {code:'native_runtime_load_failed',
                cause:Object.assign(new Error(${JSON.stringify(`Cannot load ${modelDir}; required-smoke-credential-marker`)}), {code:'ERR_DLOPEN_FAILED'})});
        };`,
    });
    const result = runNode(entry, root);
    assert.equal(result.status, 1);
    assert.equal(result.stdout, "");
    const failure = JSON.parse(result.stderr);
    assert.equal(failure.stage, "onnx_import");
    assert.equal(failure.error.cause.code, "ERR_DLOPEN_FAILED");
    assert.equal(failure.process.pid > 0, true);
    assert.match(failure.error.cause.message, /Cannot load/);
    assert.equal(result.stderr.includes(modelDir), false);
    assert.equal(result.stderr.includes("required-smoke-credential-marker"), false);
});

for (const leak of [false, true]) {
    test(`smoke entry ${leak ? "redacts a sensitive-log failure" : "preserves successful stdout JSON"}`, async (t) => {
        const root = await fixtureRoot(t);
        const source = await readFile(join(packageRoot, "server", "rag-required-smoke.mjs"), "utf8");
        const query = source.match(/const QUERY = "([^"]+)"/)[1];
        const entry = await bundleFixture(root, "rag-required-smoke.mjs", {
            "./services/rag/ragService.mjs": `export const createRagService = async () => {
                ${leak ? `console.warn(${JSON.stringify(query)});` : ""}
                return {isEnabled:()=>true};
            };`,
            "./capability-center/service.mjs": `export const createCapabilityCenter = () => ({
                list:()=>[{name:'rag_retrieve'}], invoke:async()=>({schemaVersion:'1.0',status:'ok',
                    sources:[{sourceLabel:'fixture',knowledgeText:'private knowledge text'}]})
            });`,
        });
        const result = runNode(entry, root);
        assert.equal(result.status, leak ? 1 : 0, result.stderr);
        if (leak) {
            assert.equal(JSON.parse(result.stderr).stage, "sensitive_log_scan");
            assert.equal(result.stderr.includes(query), false);
            assert.equal(result.stderr.includes("private knowledge text"), false);
        } else {
            assert.deepEqual(JSON.parse(result.stdout), summary);
            assert.equal(result.stderr, "");
        }
    });
}

test("smoke wrapper reports input validation failure with startup diagnostics", async (t) => {
    const root = await fixtureRoot(t);
    const result = runWrapper(root);
    assert.equal(result.status, 1);
    assert.equal(result.stdout, "");
    const events = parseEvents(result.stderr);
    assert.equal(events[0].event, "rag_smoke_start");
    assert.equal(events[0].bundleRoot, root);
    assert.equal(events[0].node, process.version);
    assert.equal(events.at(-1).stage, "input_validation");
    assert.match(events.at(-1).error.message, /input is missing/);
});

for (const failure of [false, true]) {
    test(`smoke wrapper ${failure ? "forwards child diagnostics and reports exit code" : "keeps startup logs out of the successful JSON"}`, async (t) => {
        const root = await fixtureRoot(t);
        await prepareWrapperInputs(root);
        const error = { event: "rag_smoke_failure", stage: "onnx_import", error: { code: "native_runtime_load_failed", cause: { code: "ERR_DLOPEN_FAILED", message: `Cannot load ${join(root, "rag-runtime", "model.onnx")}` } } };
        error.process = { loadedModules: Array.from({ length: 128 }, (_, index) => ({
            name: `runtime${index}.dll`, path: `C:\\diagnostic-snapshot\\${"x".repeat(160)}\\runtime${index}.dll`,
        })) };
        await writeFile(join(root, "rag-required-smoke.mjs"), failure
            ? `console.error(${JSON.stringify(JSON.stringify(error))}); process.exitCode=1;`
            : `console.log(${JSON.stringify(JSON.stringify(summary))});`);
        const preload = failure ? join(root, "probe-fixture.cjs") : undefined;
        if (preload) {
            await writeFile(preload, `
                const cp = require('node:child_process');
                const original = cp.spawnSync;
                cp.spawnSync = (command, args, options) =>
                    String(args[0]).endsWith('probe-rag-native.mjs') || command.endsWith('powershell.exe')
                        ? {status:0,signal:null,stdout:'{"event":"probe_fixture"}\\n',stderr:''}
                        : original(command, args, options);
                require('node:module').syncBuiltinESMExports();
            `);
        }
        const result = runWrapper(root, preload);
        assert.equal(result.status, failure ? 1 : 0, result.stderr);
        const events = parseEvents(result.stderr);
        const exit = events.find(({ event }) => event === "rag_smoke_child_exit");
        assert.equal(exit.status, failure ? 1 : 0);
        assert.equal(exit.signal, null);
        if (failure) {
            const childFailure = events.find(({ event }) => event === "rag_smoke_failure");
            assert.equal(childFailure.error.cause.code, "ERR_DLOPEN_FAILED");
            assert.equal(childFailure.process.loadedModules.length, 128);
            assert.equal(result.stderr.includes(JSON.stringify(join(root, "rag-runtime")).slice(1, -1)), false);
            assert.equal(events.at(-1).error.code, "required_rag_smoke_failed");
            assert.equal(result.stdout, "");
            assert.equal(events.some(({ event }) => event === "rag_native_probes_start"), true);
            assert.equal(events.some(({ probe }) => probe === "node_source"), true);
            assert.equal(events.some(({ probe }) => probe === "node_bundle"), true);
        } else {
            assert.deepEqual(JSON.parse(result.stdout), summary);
            assert.equal(events.some(({ event }) => event === "rag_native_probes_start"), false);
        }
    });
}

for (const code of ["ETIMEDOUT", "ENOENT"]) {
    test(`smoke wrapper reports child process ${code} independently of its assertion`, async (t) => {
        const root = await fixtureRoot(t);
        await prepareWrapperInputs(root);
        await writeFile(join(root, "rag-required-smoke.mjs"), "");
        const preload = join(root, "spawn-failure.cjs");
        await writeFile(preload, `
            require('node:child_process').spawnSync = () => ({
                status:null, signal:${code === "ETIMEDOUT" ? "'SIGTERM'" : "null"}, stdout:'', stderr:'',
                error:Object.assign(new Error('spawnSync node ${code}'), {code:${JSON.stringify(code)}})
            });
            require('node:module').syncBuiltinESMExports();
        `);
        const result = spawnSync(process.execPath, ["--require", preload, join(packageRoot, "scripts", "run-required-rag-smoke.mjs")], {
            cwd: packageRoot, env: { ...process.env, MSINSIGHT_DIST_SERVER_DIR: root },
            encoding: "utf8", timeout: 15_000, windowsHide: true,
        });
        assert.equal(result.status, 1, result.stderr);
        const events = parseEvents(result.stderr);
        const exit = events.find(({ event }) => event === "rag_smoke_child_exit");
        assert.equal(exit.status, null);
        assert.equal(exit.error.code, code);
        assert.equal(exit.signal, code === "ETIMEDOUT" ? "SIGTERM" : null);
        assert.equal(events.at(-1).stage, "child_execution");
        assert.equal(events.at(-1).error.code, "required_rag_smoke_failed");
    });
}

async function fixtureRoot(t) {
    const root = await mkdtemp(join(tmpdir(), "rag-smoke-diagnostics-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    return root;
}

async function bundleFixture(root, entry, modules) {
    const outfile = join(root, "rag-required-smoke.mjs");
    await build({
        entryPoints: [join(packageRoot, "server", entry)], outfile,
        bundle: true, format: "esm", platform: "node", target: "node22.14", external: ["node:*"],
        banner: { js: 'import { createRequire as __createRequire } from "node:module"; const require = __createRequire(import.meta.url);' },
        plugins: [{
            name: "smoke-fixture-modules",
            setup(builder) {
                builder.onResolve({ filter: /.*/ }, ({ path }) => Object.hasOwn(modules, path)
                    ? { path, namespace: "smoke-fixture" } : undefined);
                builder.onLoad({ filter: /.*/, namespace: "smoke-fixture" }, ({ path }) => ({ contents: modules[path], loader: "js" }));
            },
        }],
    });
    return outfile;
}

async function prepareWrapperInputs(root) {
    const files = ["rag-build-mode.json", "rag-data/active.json", "rag-runtime/native-runtime-manifest.json",
        "rag-runtime/models/bge-small-zh-v1.5/model-manifest.json", "rag-runtime/models/bge-small-zh-v1.5/onnx/model.onnx",
        ...expectedNativeFiles(resolveRagTarget())];
    for (const file of files) {
        await mkdir(dirname(join(root, file)), { recursive: true });
        await writeFile(join(root, file), "fixture");
    }
}

function runNode(entry, cwd, env = process.env) {
    return spawnSync(process.execPath, [entry], { cwd, env, encoding: "utf8", timeout: 15_000, windowsHide: true });
}

function runWrapper(root, preload) {
    return spawnSync(process.execPath, [
        ...(preload ? ["--require", preload] : []), join(packageRoot, "scripts", "run-required-rag-smoke.mjs"),
    ], {
        cwd: packageRoot, env: { ...process.env, MSINSIGHT_DIST_SERVER_DIR: root },
        encoding: "utf8", timeout: 15_000, windowsHide: true,
    });
}

function parseEvents(stderr) {
    return stderr.trim().split(/\r?\n/).map((line) => JSON.parse(line));
}

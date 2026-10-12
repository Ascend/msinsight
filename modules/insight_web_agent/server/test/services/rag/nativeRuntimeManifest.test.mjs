/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { test } from "node:test";
import {
    loadAndVerifyNativeRuntimeManifest,
    writeNativeRuntimeManifest,
} from "../../../services/rag/nativeRuntimeManifest.mjs";
import { resolveRagTarget } from "../../../services/rag/platformSupport.mjs";

const MODEL_SHA = "a".repeat(64);
const CONTRACT_SHA = "b".repeat(64);

test("native runtime manifest binds one target file closure to model and runtime contracts", async (t) => {
    const fixture = await createFixture(t, "win32", "x64");
    const written = await writeNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        target: fixture.target,
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
        nodeTarget: "22.14",
    });
    assert.deepEqual(written.target, { arch: "x64", id: "win32-x64", platform: "win32" });
    assert.deepEqual(written.dependencies, {
        jieba: "2.0.1",
        napi: 6,
        nodeTarget: "22.14",
        onnxRuntime: "1.22.0",
    });
    assert.deepEqual(written.contracts, {
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
    });
    assert.equal(written.files.length, 3);
    assert.equal(written.files.every(({ path, sha256, sizeBytes }) => path.includes("/") && /^[0-9a-f]{64}$/.test(sha256) && sizeBytes > 0), true);

    const loaded = await loadAndVerifyNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        platform: "win32",
        arch: "x64",
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
    });
    assert.deepEqual(loaded, written);
    assert.deepEqual(JSON.parse(await readFile(join(fixture.runtimeDir, "native-runtime-manifest.json"), "utf8")), written);
});

test("native runtime manifest rejects target, contract, and file tampering", async (t) => {
    const fixture = await createFixture(t, "linux", "x64");
    await writeNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        target: fixture.target,
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
    });
    const verify = (overrides = {}) => loadAndVerifyNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        platform: "linux",
        arch: "x64",
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
        ...overrides,
    });

    await assert.rejects(verify({ arch: "arm64" }), (error) => error?.code === "native_runtime_target_mismatch");
    await assert.rejects(verify({ modelManifestSha256: "c".repeat(64) }), (error) => error?.code === "native_runtime_contract_mismatch");

    const extra = join(fixture.bundleRoot, "node_modules", "onnxruntime-node", "bin", "napi-v6", "win32", "x64", "other.dll");
    await mkdir(dirname(extra), { recursive: true });
    await writeFile(extra, "unexpected", "utf8");
    await assert.rejects(verify(), (error) => error?.code === "native_runtime_manifest_invalid");
    await rm(extra);

    const first = fixture.expectedFiles[0];
    await writeFile(join(fixture.bundleRoot, first), "changed", "utf8");
    await assert.rejects(verify(), (error) => error?.code === "native_runtime_digest_mismatch");
});

test("native runtime manifest rejects malformed fields and unsafe file paths", async (t) => {
    const fixture = await createFixture(t, "darwin", "arm64");
    await writeNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        target: fixture.target,
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
    });
    const path = join(fixture.runtimeDir, "native-runtime-manifest.json");
    const manifest = JSON.parse(await readFile(path, "utf8"));
    manifest.files[0].path = "../outside.node";
    await writeFile(path, `${JSON.stringify(manifest)}\n`, "utf8");

    await assert.rejects(loadAndVerifyNativeRuntimeManifest({
        bundleRoot: fixture.bundleRoot,
        runtimeDir: fixture.runtimeDir,
        platform: "darwin",
        arch: "arm64",
        modelManifestSha256: MODEL_SHA,
        runtimeContractSha256: CONTRACT_SHA,
    }), (error) => error?.code === "native_runtime_manifest_invalid");
});

async function createFixture(t, platform, arch) {
    const bundleRoot = await mkdtemp(join(tmpdir(), "rag-native-runtime-"));
    t.after(() => rm(bundleRoot, { recursive: true, force: true }));
    const runtimeDir = join(bundleRoot, "rag-runtime");
    await mkdir(runtimeDir, { recursive: true });
    const target = resolveRagTarget({ platform, arch, libc: platform === "linux" ? "glibc" : undefined });
    const expectedFiles = [
        ...target.onnx.files.map((file) => `node_modules/onnxruntime-node/${file}`),
        `node_modules/${target.jieba.packageName}/${target.jieba.nativeFile}`,
    ];
    for (const [index, relativePath] of expectedFiles.entries()) {
        const path = join(bundleRoot, relativePath);
        await mkdir(dirname(path), { recursive: true });
        await writeFile(path, `native-${index}`, "utf8");
    }
    return { bundleRoot, runtimeDir, target, expectedFiles };
}

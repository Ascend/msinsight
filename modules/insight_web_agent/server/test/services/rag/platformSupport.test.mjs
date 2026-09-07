/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import {
    listSupportedRagTargets,
    RAG_NATIVE_VERSIONS,
    resolveProductRagTarget,
    resolveRagTarget,
} from "../../../services/rag/platformSupport.mjs";

const EXPECTED_TARGETS = [
    "darwin-arm64",
    "darwin-x64",
    "linux-arm64",
    "linux-x64",
    "win32-x64",
];

test("RAG platform support exposes one immutable definition for every product target", () => {
    assert.deepEqual(listSupportedRagTargets().map(({ id }) => id), EXPECTED_TARGETS);
    assert.deepEqual(RAG_NATIVE_VERSIONS, {
        jieba: "2.0.1",
        napi: 6,
        onnxRuntime: "1.22.0",
    });

    const windows = resolveRagTarget({ platform: "win32", arch: "x64" });
    assert.equal(windows.jieba.packageName, "@node-rs/jieba-win32-x64-msvc");
    assert.deepEqual(windows.onnx.files, [
        "bin/napi-v6/win32/x64/onnxruntime_binding.node",
        "bin/napi-v6/win32/x64/onnxruntime.dll",
    ]);

    const linuxX64 = resolveRagTarget({ platform: "linux", arch: "x64", libc: "glibc" });
    assert.equal(linuxX64.minimumGlibc, "2.27");
    assert.equal(linuxX64.jieba.packageName, "@node-rs/jieba-linux-x64-gnu");
    assert.deepEqual(linuxX64.onnx.files, [
        "bin/napi-v6/linux/x64/onnxruntime_binding.node",
        "bin/napi-v6/linux/x64/libonnxruntime.so.1",
    ]);

    const mac = resolveRagTarget({ platform: "darwin", arch: "arm64" });
    assert.equal(mac.jieba.packageName, "@node-rs/jieba-darwin-arm64");
    assert.deepEqual(mac.onnx.files, [
        "bin/napi-v6/darwin/arm64/onnxruntime_binding.node",
        "bin/napi-v6/darwin/arm64/libonnxruntime.1.22.0.dylib",
    ]);
    assert.equal(Object.isFrozen(mac), true);
    assert.equal(Object.isFrozen(mac.onnx.files), true);
});

test("product architecture names normalize to the Node target matrix", () => {
    assert.equal(resolveProductRagTarget({ platform: "win32", architecture: "x86_64" }).id, "win32-x64");
    assert.equal(resolveProductRagTarget({ platform: "linux", architecture: "aarch64" }).id, "linux-arm64");
    assert.equal(resolveProductRagTarget({ platform: "darwin", architecture: "x86_64" }).id, "darwin-x64");
    assert.equal(resolveProductRagTarget({ platform: "darwin", architecture: "aarch64" }).id, "darwin-arm64");
    assert.equal(resolveProductRagTarget({ platform: "darwin", architecture: "arm64" }).id, "darwin-arm64");
});

test("unsupported RAG targets fail with one stable error contract", () => {
    for (const input of [
        { platform: "win32", arch: "arm64" },
        { platform: "linux", arch: "x64", libc: "musl" },
        { platform: "linux", arch: "riscv64", libc: "glibc" },
        { platform: "freebsd", arch: "x64" },
    ]) {
        assert.throws(
            () => resolveRagTarget(input),
            (error) => error?.code === "unsupported_rag_platform" && !error.message.includes("\\"),
        );
    }
    assert.throws(
        () => resolveProductRagTarget({ platform: "linux", architecture: "ppc64" }),
        (error) => error?.code === "unsupported_rag_platform",
    );
});

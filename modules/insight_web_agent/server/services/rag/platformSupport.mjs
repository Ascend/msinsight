/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const RAG_NATIVE_VERSIONS = Object.freeze({
    jieba: "2.0.1",
    napi: 6,
    onnxRuntime: "1.22.0",
});

export class UnsupportedRagPlatformError extends Error {
    constructor(platform, arch) {
        super(`Unsupported RAG platform: ${stablePart(platform)}-${stablePart(arch)}`);
        this.name = "UnsupportedRagPlatformError";
        this.code = "unsupported_rag_platform";
    }
}

const target = ({ platform, arch, productArchitecture, minimumGlibc, onnxFiles, jiebaPackage, jiebaFile }) => deepFreeze({
    id: `${platform}-${arch}`,
    platform,
    arch,
    productArchitecture,
    ...(minimumGlibc ? { minimumGlibc } : {}),
    onnx: {
        packageName: "onnxruntime-node",
        files: onnxFiles,
    },
    jieba: {
        packageName: jiebaPackage,
        nativeFile: jiebaFile,
    },
});

const TARGETS = new Map([
    target({
        platform: "darwin",
        arch: "arm64",
        productArchitecture: "aarch64",
        onnxFiles: [
            "bin/napi-v6/darwin/arm64/onnxruntime_binding.node",
            "bin/napi-v6/darwin/arm64/libonnxruntime.1.22.0.dylib",
        ],
        jiebaPackage: "@node-rs/jieba-darwin-arm64",
        jiebaFile: "jieba.darwin-arm64.node",
    }),
    target({
        platform: "darwin",
        arch: "x64",
        productArchitecture: "x86_64",
        onnxFiles: [
            "bin/napi-v6/darwin/x64/onnxruntime_binding.node",
            "bin/napi-v6/darwin/x64/libonnxruntime.1.22.0.dylib",
        ],
        jiebaPackage: "@node-rs/jieba-darwin-x64",
        jiebaFile: "jieba.darwin-x64.node",
    }),
    target({
        platform: "linux",
        arch: "arm64",
        productArchitecture: "aarch64",
        minimumGlibc: "2.27",
        onnxFiles: [
            "bin/napi-v6/linux/arm64/onnxruntime_binding.node",
            "bin/napi-v6/linux/arm64/libonnxruntime.so.1",
        ],
        jiebaPackage: "@node-rs/jieba-linux-arm64-gnu",
        jiebaFile: "jieba.linux-arm64-gnu.node",
    }),
    target({
        platform: "linux",
        arch: "x64",
        productArchitecture: "x86_64",
        minimumGlibc: "2.27",
        onnxFiles: [
            "bin/napi-v6/linux/x64/onnxruntime_binding.node",
            "bin/napi-v6/linux/x64/libonnxruntime.so.1",
        ],
        jiebaPackage: "@node-rs/jieba-linux-x64-gnu",
        jiebaFile: "jieba.linux-x64-gnu.node",
    }),
    target({
        platform: "win32",
        arch: "x64",
        productArchitecture: "x86_64",
        onnxFiles: [
            "bin/napi-v6/win32/x64/onnxruntime_binding.node",
            "bin/napi-v6/win32/x64/onnxruntime.dll",
        ],
        jiebaPackage: "@node-rs/jieba-win32-x64-msvc",
        jiebaFile: "jieba.win32-x64-msvc.node",
    }),
].map((value) => [value.id, value]));

const PRODUCT_ARCHITECTURES = Object.freeze({
    aarch64: "arm64",
    arm64: "arm64",
    x86_64: "x64",
    x64: "x64",
});

export const listSupportedRagTargets = () => [...TARGETS.values()];

export const resolveRagTarget = ({ platform = process.platform, arch = process.arch, libc } = {}) => {
    const normalizedPlatform = String(platform ?? "").trim().toLowerCase();
    const normalizedArch = String(arch ?? "").trim().toLowerCase();
    if (normalizedPlatform === "linux" && libc !== undefined && String(libc).trim().toLowerCase() !== "glibc") {
        throw new UnsupportedRagPlatformError(normalizedPlatform, normalizedArch);
    }
    const value = TARGETS.get(`${normalizedPlatform}-${normalizedArch}`);
    if (!value) throw new UnsupportedRagPlatformError(normalizedPlatform, normalizedArch);
    return value;
};

export const resolveProductRagTarget = ({ platform, architecture, libc } = {}) => {
    const normalizedArchitecture = String(architecture ?? "").trim().toLowerCase();
    const arch = PRODUCT_ARCHITECTURES[normalizedArchitecture];
    if (!arch) throw new UnsupportedRagPlatformError(platform, normalizedArchitecture);
    return resolveRagTarget({ platform, arch, libc });
};

function deepFreeze(value) {
    if (!value || typeof value !== "object" || Object.isFrozen(value)) return value;
    for (const child of Object.values(value)) deepFreeze(child);
    return Object.freeze(value);
}

const stablePart = (value) => String(value ?? "unknown").replace(/[^a-zA-Z0-9_-]/g, "_").slice(0, 32) || "unknown";

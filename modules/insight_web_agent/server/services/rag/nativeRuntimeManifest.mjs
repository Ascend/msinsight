/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { lstat, readFile, readdir, writeFile } from "node:fs/promises";
import { join, relative, resolve, sep } from "node:path";
import { RAG_NATIVE_VERSIONS, resolveRagTarget } from "./platformSupport.mjs";

export const NATIVE_RUNTIME_MANIFEST_FILE = "native-runtime-manifest.json";
const SCHEMA_VERSION = "1.0";
const SHA256_RE = /^[0-9a-f]{64}$/;

export class NativeRuntimeManifestError extends Error {
    constructor(code, message) {
        super(message);
        this.name = "NativeRuntimeManifestError";
        this.code = code;
    }
}

export const writeNativeRuntimeManifest = async ({
    bundleRoot,
    runtimeDir,
    target,
    modelManifestSha256,
    runtimeContractSha256,
    nodeTarget = "22.14",
} = {}) => {
    const root = requiredDirectory(bundleRoot, "bundle root");
    const runtime = requiredDirectory(runtimeDir, "runtime directory");
    const activeTarget = target ?? resolveRagTarget();
    const files = [];
    for (const path of expectedNativeFiles(activeTarget)) {
        const absolutePath = safeFile(root, path);
        const info = await regularFileInfo(absolutePath, "native_runtime_missing");
        const bytes = await readFile(absolutePath);
        files.push(Object.freeze({ path, sha256: sha256(bytes), sizeBytes: info.size }));
    }
    await verifyNativeFileClosure(root, files.map(({ path }) => path));
    const manifest = deepFreeze({
        schemaVersion: SCHEMA_VERSION,
        target: {
            arch: activeTarget.arch,
            id: activeTarget.id,
            platform: activeTarget.platform,
        },
        dependencies: {
            jieba: RAG_NATIVE_VERSIONS.jieba,
            napi: RAG_NATIVE_VERSIONS.napi,
            nodeTarget: requiredText(nodeTarget, "node target"),
            onnxRuntime: RAG_NATIVE_VERSIONS.onnxRuntime,
        },
        contracts: {
            modelManifestSha256: requiredSha256(modelManifestSha256, "model manifest"),
            runtimeContractSha256: requiredSha256(runtimeContractSha256, "runtime contract"),
        },
        files,
    });
    await writeFile(join(runtime, NATIVE_RUNTIME_MANIFEST_FILE), `${JSON.stringify(manifest)}\n`, { encoding: "utf8", flag: "wx" });
    return manifest;
};

export const loadAndVerifyNativeRuntimeManifest = async ({
    bundleRoot,
    runtimeDir,
    platform = process.platform,
    arch = process.arch,
    libc,
    modelManifestSha256,
    runtimeContractSha256,
} = {}) => {
    const root = requiredDirectory(bundleRoot, "bundle root");
    const runtime = requiredDirectory(runtimeDir, "runtime directory");
    const target = resolveRagTarget({ platform, arch, libc });
    let manifest;
    try {
        const path = join(runtime, NATIVE_RUNTIME_MANIFEST_FILE);
        await regularFileInfo(path, "native_runtime_manifest_invalid");
        manifest = JSON.parse((await readFile(path, "utf8")).replace(/^\uFEFF/, ""));
    } catch (error) {
        if (error instanceof NativeRuntimeManifestError) throw error;
        throw new NativeRuntimeManifestError("native_runtime_manifest_invalid", "Unable to load RAG native runtime manifest");
    }
    validateManifest(manifest, target, { modelManifestSha256, runtimeContractSha256 });
    await verifyNativeFileClosure(root, manifest.files.map(({ path }) => path));
    for (const file of manifest.files) {
        const absolutePath = safeFile(root, file.path);
        const info = await regularFileInfo(absolutePath, "native_runtime_missing");
        if (info.size !== file.sizeBytes || sha256(await readFile(absolutePath)) !== file.sha256) {
            throw new NativeRuntimeManifestError("native_runtime_digest_mismatch", `RAG native runtime file changed: ${file.path}`);
        }
    }
    return deepFreeze(manifest);
};

const validateManifest = (manifest, target, contracts) => {
    exactObject(manifest, ["contracts", "dependencies", "files", "schemaVersion", "target"], "manifest");
    if (manifest.schemaVersion !== SCHEMA_VERSION) invalid("Unsupported RAG native runtime manifest schema");
    exactObject(manifest.target, ["arch", "id", "platform"], "target");
    if (manifest.target.id !== target.id || manifest.target.platform !== target.platform || manifest.target.arch !== target.arch) {
        throw new NativeRuntimeManifestError("native_runtime_target_mismatch", "RAG native runtime target does not match this product");
    }
    exactObject(manifest.dependencies, ["jieba", "napi", "nodeTarget", "onnxRuntime"], "dependencies");
    if (manifest.dependencies.jieba !== RAG_NATIVE_VERSIONS.jieba
        || manifest.dependencies.napi !== RAG_NATIVE_VERSIONS.napi
        || manifest.dependencies.onnxRuntime !== RAG_NATIVE_VERSIONS.onnxRuntime
        || typeof manifest.dependencies.nodeTarget !== "string"
        || !manifest.dependencies.nodeTarget) invalid("RAG native dependency versions are invalid");
    exactObject(manifest.contracts, ["modelManifestSha256", "runtimeContractSha256"], "contracts");
    const expectedModel = requiredSha256(contracts.modelManifestSha256, "model manifest");
    const expectedRuntime = requiredSha256(contracts.runtimeContractSha256, "runtime contract");
    if (manifest.contracts.modelManifestSha256 !== expectedModel
        || manifest.contracts.runtimeContractSha256 !== expectedRuntime) {
        throw new NativeRuntimeManifestError("native_runtime_contract_mismatch", "RAG native runtime contracts do not match this product");
    }
    const expectedFiles = expectedNativeFiles(target);
    if (!Array.isArray(manifest.files) || manifest.files.length !== expectedFiles.length) invalid("RAG native runtime file closure is invalid");
    for (const [index, file] of manifest.files.entries()) {
        exactObject(file, ["path", "sha256", "sizeBytes"], "file");
        if (file.path !== expectedFiles[index] || !safeRelativeFile(file.path)) invalid("RAG native runtime file path is invalid");
        if (!SHA256_RE.test(file.sha256) || !Number.isSafeInteger(file.sizeBytes) || file.sizeBytes <= 0) invalid("RAG native runtime file identity is invalid");
    }
};

export const expectedNativeFiles = (target) => Object.freeze([
    ...target.onnx.files.map((file) => `node_modules/onnxruntime-node/${file}`),
    `node_modules/${target.jieba.packageName}/${target.jieba.nativeFile}`,
]);

const exactObject = (value, expectedKeys, label) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) invalid(`RAG native runtime ${label} must be an object`);
    const actual = Object.keys(value).sort();
    const expected = [...expectedKeys].sort();
    if (actual.length !== expected.length || actual.some((key, index) => key !== expected[index])) invalid(`RAG native runtime ${label} fields are invalid`);
};

const safeRelativeFile = (value) => {
    const path = String(value ?? "").replaceAll("\\", "/");
    return Boolean(path) && !path.startsWith("/") && !path.includes(":") && !path.split("/").includes("..");
};

const safeFile = (root, relativePath) => {
    if (!safeRelativeFile(relativePath)) invalid("RAG native runtime file path is unsafe");
    const path = resolve(root, relativePath);
    if (!relative(root, path) || !path.startsWith(`${root}${sep}`)) invalid("RAG native runtime file resolves outside the product bundle");
    return path;
};

const regularFileInfo = async (path, code) => {
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
        return info;
    } catch {
        throw new NativeRuntimeManifestError(code, "Required RAG native runtime file is unavailable");
    }
};

const verifyNativeFileClosure = async (root, expectedPaths) => {
    const nativeFiles = await collectNativeFiles(join(root, "node_modules"), root);
    const expected = [...expectedPaths].sort();
    if (nativeFiles.length !== expected.length || nativeFiles.some((path, index) => path !== expected[index])) {
        throw new NativeRuntimeManifestError("native_runtime_manifest_invalid", "RAG native runtime contains an unexpected file closure");
    }
};

const collectNativeFiles = async (directory, root) => {
    let entries;
    try {
        entries = await readdir(directory, { withFileTypes: true });
    } catch {
        throw new NativeRuntimeManifestError("native_runtime_missing", "RAG native runtime dependency directory is unavailable");
    }
    const files = [];
    for (const entry of entries) {
        const path = join(directory, entry.name);
        if (entry.isSymbolicLink()) throw new NativeRuntimeManifestError("native_runtime_manifest_invalid", "RAG native runtime contains a symbolic link");
        if (entry.isDirectory()) {
            files.push(...await collectNativeFiles(path, root));
        } else if (entry.isFile() && isNativeFile(entry.name)) {
            files.push(relative(root, path).replaceAll("\\", "/"));
        }
    }
    return files.sort();
};

const isNativeFile = (name) => /(?:\.node|\.dll|\.dylib|\.so(?:\.\d+)*)$/i.test(name);

const requiredDirectory = (value, label) => {
    const path = String(value ?? "").trim();
    if (!path) invalid(`RAG native runtime ${label} is required`);
    return resolve(path);
};

const requiredText = (value, label) => {
    const text = String(value ?? "").trim();
    if (!text) invalid(`RAG native runtime ${label} is required`);
    return text;
};

const requiredSha256 = (value, label) => {
    const digest = String(value ?? "");
    if (!SHA256_RE.test(digest)) invalid(`RAG native runtime ${label} SHA-256 is invalid`);
    return digest;
};

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

const invalid = (message) => {
    throw new NativeRuntimeManifestError("native_runtime_manifest_invalid", message);
};

function deepFreeze(value) {
    if (!value || typeof value !== "object" || Object.isFrozen(value)) return value;
    for (const child of Object.values(value)) deepFreeze(child);
    return Object.freeze(value);
}

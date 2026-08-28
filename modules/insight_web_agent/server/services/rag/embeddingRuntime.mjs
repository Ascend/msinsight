/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { lstat, readFile } from "node:fs/promises";
import { join, resolve, sep } from "node:path";

const MODEL_MANIFEST_FILE = "model-manifest.json";
const REQUIRED_FILES = new Set([
    "config.json",
    "onnx/model.onnx",
    "tokenizer.json",
    "tokenizer_config.json",
    "special_tokens_map.json",
]);
const SHA256_RE = /^[0-9a-f]{64}$/;

export class EmbeddingRuntimeError extends Error {
    constructor(code, message) {
        super(message);
        this.name = "EmbeddingRuntimeError";
        this.code = code;
    }
}

export const createEmbeddingRuntime = async ({ modelDir, platform = process.platform, arch = process.arch } = {}) => {
    if (platform !== "win32" || arch !== "x64") {
        throw new EmbeddingRuntimeError("unsupported_rag_platform", `Unsupported RAG embedding platform: ${platform}-${arch}`);
    }
    const { modelDir: directory, manifest } = await loadEmbeddingModelContract({ modelDir });
    let session;
    try {
        const ort = await import("onnxruntime-node");
        session = await ort.InferenceSession.create(join(directory, "onnx", "model.onnx"), { executionProviders: ["cpu"] });
    } catch (error) {
        throw new EmbeddingRuntimeError("onnx_initialization_failed", `Unable to initialize ONNX embedding model: ${error.message}`);
    }
    validateSession(session, manifest);
    return { modelDir: directory, manifest, session };
};

export const loadEmbeddingModelContract = async ({ modelDir } = {}) => {
    const directory = resolveRequiredDirectory(modelDir, "model directory");
    const { manifest, bytes } = await loadModelManifest(directory);
    await verifyModelFiles(directory, manifest.fileDigests);
    return {
        modelDir: directory,
        manifest: {
            ...manifest,
            manifestSha256: createHash("sha256").update(bytes).digest("hex"),
        },
    };
};

export const loadModelManifest = async (modelDir) => {
    const directory = resolveRequiredDirectory(modelDir, "model directory");
    let bytes;
    let raw;
    try {
        const path = join(directory, MODEL_MANIFEST_FILE);
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
        bytes = await readFile(path);
        raw = JSON.parse(bytes.toString("utf8").replace(/^\uFEFF/, ""));
    } catch (error) {
        throw new EmbeddingRuntimeError("model_manifest_invalid", `Unable to read model manifest: ${error.message}`);
    }
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) failManifest("must be an object");
    if (raw.schemaVersion !== "1.0") failManifest(`has unsupported schemaVersion: ${raw.schemaVersion}`);
    const modelId = requiredString(raw, "modelId");
    const modelRevision = requiredString(raw, "modelRevision");
    const tokenizerId = requiredString(raw, "tokenizerId");
    const pooling = requiredString(raw, "pooling");
    const outputName = requiredString(raw, "outputName");
    if (!/^[0-9a-f]{40}$/.test(modelRevision)) failManifest("modelRevision must be a lowercase immutable revision");
    if (pooling !== "mean") failManifest("pooling must be 'mean'");
    if (raw.normalize !== true) failManifest("normalize must be true");
    if (!Number.isInteger(raw.dimension) || raw.dimension <= 0) failManifest("dimension must be a positive integer");
    if (!Number.isInteger(raw.maxSequenceLength) || raw.maxSequenceLength <= 0) failManifest("maxSequenceLength must be a positive integer");
    if (typeof raw.queryPrefix !== "string") failManifest("queryPrefix must be a string");
    const inputs = raw.inputs;
    if (!inputs || typeof inputs !== "object" || Array.isArray(inputs)) failManifest("inputs must be an object");
    const normalizedInputs = {
        inputIds: requiredString(inputs, "inputIds", "inputs."),
        attentionMask: requiredString(inputs, "attentionMask", "inputs."),
        tokenTypeIds: requiredString(inputs, "tokenTypeIds", "inputs."),
    };
    const fileDigests = raw.fileDigests;
    if (!fileDigests || typeof fileDigests !== "object" || Array.isArray(fileDigests)) failManifest("fileDigests must be an object");
    if (new Set(Object.keys(fileDigests)).size !== REQUIRED_FILES.size || ![...REQUIRED_FILES].every((file) => file in fileDigests)) {
        failManifest("fileDigests must declare exactly the required model files");
    }
    for (const [file, digest] of Object.entries(fileDigests)) {
        if (!isSafeRelativeFile(file) || !REQUIRED_FILES.has(file) || typeof digest !== "string" || !SHA256_RE.test(digest)) {
            failManifest(`has invalid file digest entry: ${file}`);
        }
    }
    return {
        bytes,
        manifest: {
            schemaVersion: "1.0",
            modelId,
            modelRevision,
            tokenizerId,
            dimension: raw.dimension,
            maxSequenceLength: raw.maxSequenceLength,
            pooling,
            normalize: true,
            queryPrefix: raw.queryPrefix,
            inputs: normalizedInputs,
            outputName,
            fileDigests: { ...fileDigests },
        },
    };
};

const verifyModelFiles = async (directory, fileDigests) => {
    for (const [relativeFile, expectedDigest] of Object.entries(fileDigests)) {
        const path = safeJoin(directory, relativeFile);
        let data;
        try {
            const info = await lstat(path);
            if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
            data = await readFile(path);
        } catch {
            throw new EmbeddingRuntimeError("model_file_missing", `Required model file does not exist: ${relativeFile}`);
        }
        const actualDigest = createHash("sha256").update(modelFileDigestInput(relativeFile, data)).digest("hex");
        if (actualDigest !== expectedDigest) {
            throw new EmbeddingRuntimeError("model_file_digest_mismatch", `Model file digest mismatch: ${relativeFile}`);
        }
    }
};

// Windows worktrees may normalize text assets. The producer digest contract is LF-normalized for JSON files.
const modelFileDigestInput = (relativeFile, data) => relativeFile === "onnx/model.onnx"
    ? data
    : Buffer.from(data.toString("utf8").replace(/\r\n/g, "\n"), "utf8");

const validateSession = (session, manifest) => {
    const inputNames = new Set(session.inputNames);
    if (!Object.values(manifest.inputs).every((name) => inputNames.has(name))) {
        throw new EmbeddingRuntimeError("onnx_contract_mismatch", "ONNX model inputs do not match model manifest");
    }
    if (!session.outputNames.includes(manifest.outputName)) {
        throw new EmbeddingRuntimeError("onnx_contract_mismatch", "ONNX model output does not match model manifest");
    }
    const output = session.outputMetadata.find((item) => item.name === manifest.outputName);
    if (!output || output.type !== "float32" || output.shape?.at(-1) !== manifest.dimension) {
        throw new EmbeddingRuntimeError("onnx_contract_mismatch", "ONNX model output shape does not match model manifest");
    }
};

const resolveRequiredDirectory = (path, label) => {
    const value = String(path ?? "").trim();
    if (!value) throw new EmbeddingRuntimeError("model_directory_missing", `RAG ${label} is required`);
    return resolve(value);
};

const requiredString = (value, field, prefix = "") => {
    const text = value?.[field];
    if (typeof text !== "string" || !text.trim()) failManifest(`${prefix}${field} must be a non-empty string`);
    return text.trim();
};

const failManifest = (message) => {
    throw new EmbeddingRuntimeError("model_manifest_invalid", `Model manifest ${message}`);
};

const isSafeRelativeFile = (value) => {
    const path = String(value ?? "").replaceAll("\\", "/");
    return path && !path.startsWith("/") && !path.includes(":") && !path.split("/").includes("..");
};

const safeJoin = (root, relativeFile) => {
    const path = resolve(root, relativeFile);
    if (!path.startsWith(`${root}${sep}`)) throw new EmbeddingRuntimeError("model_manifest_invalid", "Unsafe model file path");
    return path;
};

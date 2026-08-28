/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { readFile } from "node:fs/promises";
import { join } from "node:path";
import { createEmbeddingRuntime, EmbeddingRuntimeError } from "./embeddingRuntime.mjs";

export const createEmbeddingService = async ({ modelDir, runtime, platform, arch } = {}) => {
    const activeRuntime = runtime ?? await createEmbeddingRuntime({ modelDir, platform, arch });
    const [{ Tokenizer }, ort] = await Promise.all([
        import("@huggingface/tokenizers"),
        import("onnxruntime-node"),
    ]);
    const [tokenizerJson, tokenizerConfig] = await Promise.all([
        readJson(join(activeRuntime.modelDir, "tokenizer.json"), "tokenizer.json"),
        readJson(join(activeRuntime.modelDir, "tokenizer_config.json"), "tokenizer_config.json"),
    ]);
    const tokenizer = new Tokenizer(tokenizerJson, tokenizerConfig);
    return {
        contract: activeRuntime.manifest,
        async embedQueries(texts) {
            return embed(texts, activeRuntime.manifest.queryPrefix, tokenizer, activeRuntime, ort);
        },
        tokenize(text) {
            return tokenizer.encode(String(text ?? ""), { return_token_type_ids: true });
        },
    };
};

const embed = async (texts, prefix, tokenizer, runtime, ort) => {
    if (!Array.isArray(texts)) throw new EmbeddingRuntimeError("embedding_input_invalid", "Embedding texts must be an array");
    if (!texts.length) return [];
    const encodings = texts.map((text) => tokenizer.encode(`${prefix}${String(text ?? "")}`, { return_token_type_ids: true }));
    const sequenceLength = Math.max(...encodings.map((encoding) => encoding.ids.length));
    if (sequenceLength > runtime.manifest.maxSequenceLength) {
        throw new EmbeddingRuntimeError("embedding_input_invalid", "Tokenizer output exceeds the model sequence length");
    }
    const inputIds = new BigInt64Array(encodings.length * sequenceLength);
    const attentionMask = new BigInt64Array(encodings.length * sequenceLength);
    const tokenTypeIds = new BigInt64Array(encodings.length * sequenceLength);
    for (const [row, encoding] of encodings.entries()) {
        for (let column = 0; column < sequenceLength; column += 1) {
            const target = row * sequenceLength + column;
            inputIds[target] = BigInt(encoding.ids[column] ?? 0);
            attentionMask[target] = BigInt(encoding.attention_mask[column] ?? 0);
            tokenTypeIds[target] = BigInt(encoding.token_type_ids?.[column] ?? 0);
        }
    }
    const feeds = {
        [runtime.manifest.inputs.inputIds]: new ort.Tensor("int64", inputIds, [encodings.length, sequenceLength]),
        [runtime.manifest.inputs.attentionMask]: new ort.Tensor("int64", attentionMask, [encodings.length, sequenceLength]),
        [runtime.manifest.inputs.tokenTypeIds]: new ort.Tensor("int64", tokenTypeIds, [encodings.length, sequenceLength]),
    };
    const outputs = await runtime.session.run(feeds);
    const hidden = outputs[runtime.manifest.outputName];
    if (!hidden?.data || hidden.dims?.length !== 3 || hidden.dims[2] !== runtime.manifest.dimension) {
        throw new EmbeddingRuntimeError("embedding_output_invalid", "ONNX output does not satisfy the embedding contract");
    }
    return encodings.map((encoding, row) => meanPoolAndNormalize(hidden.data, encoding.attention_mask, row, sequenceLength, runtime.manifest.dimension));
};

const meanPoolAndNormalize = (hidden, attentionMask, row, sequenceLength, dimension) => {
    const vector = new Float32Array(dimension);
    let tokens = 0;
    for (let column = 0; column < sequenceLength; column += 1) {
        if (!attentionMask[column]) continue;
        tokens += 1;
        const offset = (row * sequenceLength + column) * dimension;
        for (let index = 0; index < dimension; index += 1) vector[index] += hidden[offset + index];
    }
    if (!tokens) throw new EmbeddingRuntimeError("embedding_output_invalid", "Tokenizer produced an empty attention mask");
    let squaredNorm = 0;
    for (let index = 0; index < dimension; index += 1) {
        vector[index] /= tokens;
        squaredNorm += vector[index] * vector[index];
    }
    const norm = Math.sqrt(squaredNorm);
    if (!Number.isFinite(norm) || norm <= 0) throw new EmbeddingRuntimeError("embedding_output_invalid", "Embedding output cannot be normalized");
    for (let index = 0; index < dimension; index += 1) vector[index] /= norm;
    return vector;
};

const readJson = async (path, label) => {
    try {
        return JSON.parse((await readFile(path, "utf8")).replace(/^\uFEFF/, ""));
    } catch (error) {
        throw new EmbeddingRuntimeError("tokenizer_invalid", `Unable to read ${label}: ${error.message}`);
    }
};

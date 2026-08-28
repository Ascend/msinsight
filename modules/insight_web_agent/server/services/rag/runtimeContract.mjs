/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { lstat, readFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { PACKAGE_MEMBERS } from "./wire/packageContracts.mjs";

export const loadRuntimeContract = async (runtimeDir) => {
    const root = resolveRequiredDirectory(runtimeDir);
    const bytes = await readRegularFile(join(root, "runtime-contract.json"), "runtime contract");
    const contract = parseTrustedJson(bytes, "runtime contract");
    validateRuntimeContract(contract);
    return {
        runtimeDir: root,
        contract: {
            ...contract,
            contractSha256: sha256(bytes),
        },
    };
};

export const validateRuntimeContract = (contract) => {
    if (!plainObject(contract)
        || !exactKeys(contract, [
            "embedding",
            "packageMembers",
            "packageSchemaVersion",
            "resourceLimits",
            "retrieval",
            "schemaVersion",
        ])
        || contract.schemaVersion !== "1.0"
        || contract.packageSchemaVersion !== "4.0"
        || !exactArray(contract.packageMembers, PACKAGE_MEMBERS)) {
        throw new Error("RAG runtime contract identity is invalid");
    }
    const expectedLimits = {
        maxArchiveBytes: 512 * 1024 * 1024,
        maxJsonlLineBytes: 400_000,
        maxMemberBytes: 512 * 1024 * 1024,
        maxUncompressedBytes: 1024 * 1024 * 1024,
    };
    exactValues(contract.resourceLimits, expectedLimits, "resource limits");
    const expectedEmbedding = {
        dimension: 512,
        maxSequenceLength: 512,
        modelId: "BAAI/bge-small-zh-v1.5",
        modelManifestSha256: "4e4eb912426215a0a7b0b5aec49da786d81910cbd7f3cf76561581eb69acd123",
        modelRevision: "75c43b069aac4d136ba6bc1122f995fedcfd2781",
        normalize: true,
        pooling: "mean",
        queryPrefix: "",
        tokenizerId: "BAAI/bge-small-zh-v1.5",
    };
    exactValues(contract.embedding, expectedEmbedding, "embedding");
    const expectedRetrieval = {
        bm25B: 0.75,
        bm25K1: 1.5,
        bm25Tokenizer: "jieba_search_hmm0+domain_terms+latin",
        bm25TopK: 30,
        finalTopK: 5,
        fusionType: "rrf",
        rrfK: 60,
        vectorMetric: "cosine",
        vectorStoredAs: "normalized-dot-product",
        vectorTopK: 30,
    };
    exactValues(contract.retrieval, expectedRetrieval, "retrieval");
    return contract;
};

const exactValues = (actual, expected, label) => {
    if (!plainObject(actual)
        || !exactKeys(actual, Object.keys(expected))
        || Object.entries(expected).some(([field, value]) => actual[field] !== value)) {
        throw new Error(`RAG runtime contract ${label} are invalid`);
    }
};
const readRegularFile = async (path, label) => {
    try {
        const info = await lstat(path);
        if (info.isSymbolicLink() || !info.isFile()) throw new Error("not a regular file");
        return await readFile(path);
    } catch (error) {
        throw new Error(`Unable to read RAG ${label}: ${error.message}`);
    }
};
const parseTrustedJson = (bytes, label) => {
    try {
        return JSON.parse(bytes.toString("utf8").replace(/^\uFEFF/, ""));
    } catch (error) {
        throw new Error(`Unable to parse ${label}: ${error.message}`);
    }
};
const resolveRequiredDirectory = (value) => {
    const path = String(value ?? "").trim();
    if (!path) throw new Error("RAG runtime directory is required");
    return resolve(path);
};
const exactKeys = (value, expected) => {
    if (!plainObject(value)) return false;
    const actual = Object.keys(value).sort();
    const sortedExpected = [...expected].sort();
    return actual.length === sortedExpected.length
        && actual.every((field, index) => field === sortedExpected[index]);
};
const exactArray = (actual, expected) => Array.isArray(actual)
    && actual.length === expected.length
    && actual.every((value, index) => value === expected[index]);
const plainObject = (value) => Boolean(value) && typeof value === "object" && !Array.isArray(value);
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

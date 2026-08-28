/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { parseCanonicalJson, parseCanonicalJsonl, strictObjectKeys } from "./strictJsonParser.mjs";

export const PACKAGE_SCHEMA_VERSION = "4.0";
export const KB_ID = "mindstudio-insight-ascend";
export const PACKAGE_MEMBERS = Object.freeze([
    "manifest.json",
    "sources.jsonl",
    "documents.jsonl",
    "chunks.jsonl",
    "vectors.f32",
    "bm25-domain-dict.txt",
    "bm25.json",
    "build-audit.json",
    "checksums.json",
]);
export const CHECKSUM_MEMBERS = Object.freeze(PACKAGE_MEMBERS.filter((name) => name !== "checksums.json").sort());

const SHA256_RE = /^[0-9a-f]{64}$/;
const SOURCE_ID_RE = /^[^/\\:\0]+\/[^/\\:\0]+$/;
const PROJECT_ID_RE = /^[^/\\:\0]+$/;
const DOC_ID_RE = /^d_[0-9a-f]{64}$/;
const CHUNK_ID_RE = /^c_[0-9a-f]{64}$/;
const FAQ_ID_RE = /^q_[0-9a-f]{64}$/;
const KB_VERSION_RE = /^\d{2}\.[012]\.[1-9]\d*$/;
const MAX_DOMAIN_DICTIONARY_BYTES = 1024 * 1024;
const MAX_DOMAIN_DICTIONARY_TERMS = 10000;

export class PackageContractError extends Error {
    constructor(code, message) {
        super(message);
        this.name = "PackageContractError";
        this.code = code;
    }
}

export const validatePackageMemberBytes = ({ members, runtimeContract, modelContract }) => {
    requireMemberSet(members);
    const checksums = parseCanonicalJson(members.get("checksums.json"), "checksums.json");
    validateChecksums(checksums, members);
    const manifest = parseCanonicalJson(members.get("manifest.json"), "manifest.json");
    const sources = parseCanonicalJsonl(members.get("sources.jsonl"), "sources.jsonl");
    const documents = parseCanonicalJsonl(members.get("documents.jsonl"), "documents.jsonl");
    const chunks = parseCanonicalJsonl(members.get("chunks.jsonl"), "chunks.jsonl");
    const domainDictionaryBytes = members.get("bm25-domain-dict.txt");
    const domainDictionary = validateDomainDictionary(domainDictionaryBytes);
    const bm25 = parseCanonicalJson(members.get("bm25.json"), "bm25.json");
    const buildAuditBytes = members.get("build-audit.json");
    validateLocalRuntime(runtimeContract, modelContract);
    validateManifest(manifest, runtimeContract, sha256(domainDictionaryBytes));
    validateSources(sources);
    validateDocuments(documents, sources);
    validateChunks(chunks, documents, sources);
    const vectors = validateVectors(
        members.get("vectors.f32"),
        runtimeContract.embedding.dimension,
        chunks.length,
    );
    validateBm25(bm25, chunks.length, runtimeContract.retrieval);
    return {
        manifest,
        sources,
        documents,
        chunks,
        vectors,
        domainDictionary,
        bm25,
        buildAuditBytes,
        checksums,
        contract: runtimeContract,
        ...buildIndexes(sources, documents, chunks),
    };
};

const validateChecksums = (checksums, members) => {
    exactObject(checksums, ["algorithm", "files", "schemaVersion"], "checksums.json");
    equal(checksums.schemaVersion, PACKAGE_SCHEMA_VERSION, "checksums schemaVersion");
    equal(checksums.algorithm, "sha256", "checksums algorithm");
    object(checksums.files, "checksums files");
    exactKeys(strictObjectKeys(checksums.files), CHECKSUM_MEMBERS, "checksums files");
    for (const name of CHECKSUM_MEMBERS) {
        sha256String(checksums.files[name], `checksums ${name}`);
        if (sha256(members.get(name)) !== checksums.files[name]) fail("checksum_mismatch", `Package checksum mismatch: ${name}`);
    }
};

const validateManifest = (manifest, runtimeContract, domainDictionarySha256) => {
    object(manifest, "manifest");
    equal(manifest.schemaVersion, PACKAGE_SCHEMA_VERSION, "manifest schemaVersion", "unsupported_package_schema");
    stringMatch(manifest.kbVersion, KB_VERSION_RE, "manifest kbVersion");
    equal(runtimeContract.packageSchemaVersion, PACKAGE_SCHEMA_VERSION, "runtime Package schema", "package_contract_unsupported");
    object(manifest.retrieval, "manifest retrieval");
    object(manifest.retrieval.keyword, "manifest keyword retrieval");
    equal(manifest.retrieval.keyword.tokenizer, runtimeContract.retrieval.bm25Tokenizer, "manifest BM25 tokenizer");
    sha256String(manifest.retrieval.keyword.domainDictionarySha256, "manifest domain dictionary SHA-256");
    equal(manifest.retrieval.keyword.domainDictionarySha256, domainDictionarySha256, "manifest domain dictionary", "checksum_mismatch");
};

const validateDomainDictionary = (bytes) => {
    if (!Buffer.isBuffer(bytes) || bytes.length === 0 || bytes.length > MAX_DOMAIN_DICTIONARY_BYTES) {
        fail("package_semantics_invalid", "BM25 domain dictionary size is invalid");
    }
    let text;
    try {
        text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    } catch {
        fail("package_semantics_invalid", "BM25 domain dictionary is not UTF-8");
    }
    if (text.startsWith("\uFEFF") || text.includes("\r") || text.includes("\0") || !text.endsWith("\n")) {
        fail("package_semantics_invalid", "BM25 domain dictionary is not canonical LF text");
    }
    const terms = text.slice(0, -1).split("\n");
    if (!terms.length || terms.length > MAX_DOMAIN_DICTIONARY_TERMS
        || terms.some((term) => !term || term !== term.trim() || term.length > 256)) {
        fail("package_semantics_invalid", "BM25 domain dictionary terms are invalid");
    }
    const normalized = terms.map((term) => term.toLowerCase());
    if (new Set(normalized).size !== normalized.length) {
        fail("package_semantics_invalid", "BM25 domain dictionary contains duplicate terms");
    }
    return text;
};

const validateLocalRuntime = (contract, model) => {
    if (!contract || typeof contract !== "object" || !model || typeof model !== "object") {
        fail("model_contract_mismatch", "Installed runtime and embedding model contracts are required");
    }
    const expected = {
        dimension: model.dimension,
        maxSequenceLength: model.maxSequenceLength,
        modelId: model.modelId,
        modelManifestSha256: model.manifestSha256,
        modelRevision: model.modelRevision,
        normalize: model.normalize,
        pooling: model.pooling,
        queryPrefix: model.queryPrefix,
        tokenizerId: model.tokenizerId,
    };
    for (const [field, value] of Object.entries(expected)) {
        equal(contract.embedding[field], value, `local runtime embedding.${field}`, "model_contract_mismatch");
    }
};

const validateSources = (sources) => {
    if (!sources.length) fail("package_semantics_invalid", "Package must contain at least one source");
    for (const source of sources) {
        object(source, "source record");
        stringMatch(source.sourceId, SOURCE_ID_RE, "sourceId");
        stringMatch(source.projectId, PROJECT_ID_RE, "projectId");
    }
    assertUnique(sources.map(({ sourceId }) => sourceId), "sourceId");
    assertSorted(sources, (source) => [source.sourceId.toLowerCase(), source.sourceId], "sources.jsonl");
};

const validateDocuments = (documents, sources) => {
    const sourceIds = new Set(sources.map(({ sourceId }) => sourceId));
    for (const document of documents) {
        object(document, "document record");
        stringMatch(document.docId, DOC_ID_RE, "docId");
        if (!sourceIds.has(document.sourceId)) fail("package_semantics_invalid", "document references an unknown source");
        validateSourcePath(document.sourcePath);
        equal(document.docId, documentId(document.sourceId, document.sourcePath), "document identity");
        sha256String(document.contentSha256, "document contentSha256");
        nonEmptyString(document.documentCategory, "document category");
        if (document.duplicateOfDocId !== null) stringMatch(document.duplicateOfDocId, DOC_ID_RE, "duplicateOfDocId");
    }
    assertUnique(documents.map(({ docId }) => docId), "docId");
    assertUnique(documents.map(({ sourceId, sourcePath }) => `${sourceId}\0${sourcePath}`), "document provenance");
    assertSorted(documents, (document) => [document.docId], "documents.jsonl");
    const byId = new Map(documents.map((document) => [document.docId, document]));
    for (const document of documents) {
        if (document.duplicateOfDocId === null) continue;
        const canonical = byId.get(document.duplicateOfDocId);
        if (!canonical || canonical.duplicateOfDocId !== null || canonical.contentSha256 !== document.contentSha256) {
            fail("package_semantics_invalid", "document duplicate does not bind a canonical document");
        }
    }
};

const validateChunks = (chunks, documents, sources) => {
    const documentsById = new Map(documents.map((document) => [document.docId, document]));
    const sourceById = new Map(sources.map((source) => [source.sourceId, source]));
    const indexes = new Map();
    const faqIndexes = new Map();
    for (const chunk of chunks) {
        const faqKeys = ["answerStatus", "faqId", "faqPartIndex"];
        const hasFaq = faqKeys.some((key) => strictObjectKeys(chunk).includes(key));
        object(chunk, "chunk record");
        stringMatch(chunk.chunkId, CHUNK_ID_RE, "chunkId");
        stringMatch(chunk.docId, DOC_ID_RE, "chunk docId");
        stringMatch(chunk.projectId, PROJECT_ID_RE, "chunk projectId");
        nonEmptyString(chunk.documentCategory, "chunk category");
        safeBasename(chunk.docName, "chunk docName");
        for (const field of ["title", "sourceLabel", "textSummary"]) nonEmptyText(chunk[field], `chunk ${field}`);
        nonEmptyText(chunk.knowledgeText, "chunk knowledgeText");
        if (!Array.isArray(chunk.sectionPath) || !chunk.sectionPath.length) fail("package_semantics_invalid", "chunk sectionPath is invalid");
        for (const section of chunk.sectionPath) nonEmptyString(section, "chunk sectionPath");
        integer(chunk.chunkIndex, "chunkIndex", { min: 0 });
        nonEmptyString(chunk.contentFormat, "chunk contentFormat");
        const document = documentsById.get(chunk.docId);
        if (!document || document.duplicateOfDocId !== null) fail("package_semantics_invalid", "chunk must reference a canonical document");
        const source = sourceById.get(document.sourceId);
        if (!source || source.projectId !== chunk.projectId || document.documentCategory !== chunk.documentCategory) {
            fail("package_semantics_invalid", "chunk provenance does not match document and source records");
        }
        indexes.set(chunk.docId, [...(indexes.get(chunk.docId) ?? []), chunk.chunkIndex]);
        if (hasFaq) {
            if (!faqKeys.every((key) => strictObjectKeys(chunk).includes(key))) fail("package_semantics_invalid", "FAQ fields must be all present or all absent");
            stringMatch(chunk.faqId, FAQ_ID_RE, "faqId");
            integer(chunk.faqPartIndex, "faqPartIndex", { min: 0 });
            if (!["answered", "missing"].includes(chunk.answerStatus)) fail("package_semantics_invalid", "answerStatus is invalid");
            faqIndexes.set(chunk.faqId, [...(faqIndexes.get(chunk.faqId) ?? []), chunk.faqPartIndex]);
        }
    }
    assertUnique(chunks.map(({ chunkId }) => chunkId), "chunkId");
    assertSorted(chunks, (chunk) => [chunk.docId, chunk.chunkIndex, chunk.chunkId], "chunks.jsonl");
    for (const values of [...indexes.values(), ...faqIndexes.values()]) {
        if (values.some((value, index) => value !== index)) fail("package_semantics_invalid", "chunk or FAQ indexes are not contiguous");
    }
};

const validateVectors = (bytes, dimension, chunkCount) => {
    if (!Buffer.isBuffer(bytes)) fail("package_semantics_invalid", "vectors.f32 is not binary data");
    if (bytes.length !== chunkCount * dimension * 4) fail("package_semantics_invalid", "vectors.f32 row count does not match chunks");
    const values = new Float32Array(chunkCount * dimension);
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    for (let index = 0; index < values.length; index += 1) values[index] = view.getFloat32(index * 4, true);
    for (let row = 0; row < chunkCount; row += 1) {
        for (let column = 0; column < dimension; column += 1) {
            const value = values[row * dimension + column];
            if (!Number.isFinite(value)) fail("package_semantics_invalid", "vectors.f32 contains a non-finite value");
        }
    }
    return values;
};

const validateBm25 = (bm25, chunkCount, retrieval) => {
    exactObject(bm25, ["avgDocLength", "b", "docCount", "docLengths", "k1", "schemaVersion", "terms", "tokenizer"], "bm25");
    equal(bm25.schemaVersion, PACKAGE_SCHEMA_VERSION, "BM25 schemaVersion");
    equal(bm25.tokenizer, retrieval.bm25Tokenizer, "BM25 tokenizer");
    equal(bm25.k1, retrieval.bm25K1, "BM25 k1");
    equal(bm25.b, retrieval.bm25B, "BM25 b");
    equal(bm25.docCount, chunkCount, "BM25 docCount");
    if (!Array.isArray(bm25.docLengths) || bm25.docLengths.length !== chunkCount) fail("package_semantics_invalid", "BM25 docLengths does not match chunks");
    bm25.docLengths.forEach((value) => integer(value, "BM25 doc length", { min: 0 }));
    finiteNumber(bm25.avgDocLength, "BM25 avgDocLength", { min: 0 });
    const expectedAverage = chunkCount ? bm25.docLengths.reduce((total, value) => total + value, 0) / chunkCount : 0;
    if (Math.abs(bm25.avgDocLength - expectedAverage) > 1e-12) fail("package_semantics_invalid", "BM25 avgDocLength is inconsistent");
    object(bm25.terms, "BM25 terms");
    const terms = strictObjectKeys(bm25.terms);
    exactKeys(terms, [...terms].sort(compareCodePoints), "BM25 term order");
    for (const term of terms) {
        if (!term) fail("package_semantics_invalid", "BM25 term must not be empty");
        const info = bm25.terms[term];
        exactObject(info, ["idf", "postings"], "BM25 term");
        finiteNumber(info.idf, "BM25 idf", { min: 0 });
        if (!Array.isArray(info.postings)) fail("package_semantics_invalid", "BM25 postings must be an array");
        let previous = -1;
        for (const posting of info.postings) {
            if (!Array.isArray(posting) || posting.length !== 2) fail("package_semantics_invalid", "BM25 posting is invalid");
            integer(posting[0], "BM25 posting index", { min: 0, max: chunkCount - 1 });
            integer(posting[1], "BM25 posting frequency", { min: 1 });
            if (posting[0] <= previous) fail("package_semantics_invalid", "BM25 postings are not strictly ordered");
            previous = posting[0];
        }
    }
};

const buildIndexes = (sources, documents, chunks) => {
    const sourceById = new Map(sources.map((source) => [source.sourceId, source]));
    const documentById = new Map(documents.map((document) => [document.docId, document]));
    const chunkById = new Map();
    const chunksByDocId = new Map();
    const chunksByFaqId = new Map();
    for (const chunk of chunks) {
        chunkById.set(chunk.chunkId, chunk);
        chunksByDocId.set(chunk.docId, [...(chunksByDocId.get(chunk.docId) ?? []), chunk]);
        if (chunk.faqId) chunksByFaqId.set(chunk.faqId, [...(chunksByFaqId.get(chunk.faqId) ?? []), chunk]);
    }
    return { sourceById, documentById, chunkById, chunksByDocId, chunksByFaqId };
};

const requireMemberSet = (members) => {
    if (!(members instanceof Map)) fail("invalid_archive_entries", "Package members must be supplied as a Map");
    exactKeys([...members.keys()], PACKAGE_MEMBERS, "Package members");
    for (const [name, value] of members) if (!Buffer.isBuffer(value)) fail("invalid_archive", `Package member is not bytes: ${name}`);
};

const exactObject = (value, keys, label) => {
    object(value, label);
    exactKeys([...strictObjectKeys(value)].sort(compareCodePoints), [...keys].sort(compareCodePoints), label);
};

const exactKeys = (actual, expected, label) => {
    if (actual.length !== expected.length || actual.some((value, index) => value !== expected[index])) {
        fail("package_semantics_invalid", `${label} fields do not match the Package v4 contract`);
    }
};

const object = (value, label) => {
    if (!value || typeof value !== "object" || Array.isArray(value)) fail("package_semantics_invalid", `${label} must be an object`);
};

const equal = (actual, expected, label, code = "package_semantics_invalid") => {
    if (actual !== expected) fail(code, `${label} does not match the Package v4 contract`);
};

const nonEmptyString = (value, label) => {
    if (typeof value !== "string" || !value || value !== value.trim() || value.includes("\0")) fail("package_semantics_invalid", `${label} must be a non-empty trimmed string`);
};

const nonEmptyText = (value, label) => {
    if (typeof value !== "string" || !value || value.includes("\0")) fail("package_semantics_invalid", `${label} must be non-empty and NUL-free`);
};

const stringMatch = (value, expression, label) => {
    if (typeof value !== "string" || !expression.test(value)) fail("package_semantics_invalid", `${label} has an invalid value`);
};

const sha256String = (value, label) => stringMatch(value, SHA256_RE, label);

const integer = (value, label, { min = Number.MIN_SAFE_INTEGER, max = Number.MAX_SAFE_INTEGER } = {}) => {
    if (!Number.isSafeInteger(value) || value < min || value > max) fail("package_semantics_invalid", `${label} must be an integer in range`);
};

const finiteNumber = (value, label, { min = -Infinity, max = Infinity } = {}) => {
    if (typeof value !== "number" || !Number.isFinite(value) || value < min || value > max) fail("package_semantics_invalid", `${label} must be a finite number in range`);
};

const assertUnique = (values, label) => {
    if (new Set(values).size !== values.length) fail("package_semantics_invalid", `Package contains duplicate ${label}`);
};

const assertSorted = (values, key, label) => {
    const previous = values.map(key);
    const sorted = [...previous].sort(compareTuples);
    if (previous.some((value, index) => compareTuples(value, sorted[index]) !== 0)) fail("package_semantics_invalid", `${label} records are not sorted`);
};

const compareTuples = (left, right) => {
    for (let index = 0; index < Math.min(left.length, right.length); index += 1) {
        if (typeof left[index] === "number") {
            if (left[index] !== right[index]) return left[index] - right[index];
        } else {
            const result = compareCodePoints(left[index], right[index]);
            if (result) return result;
        }
    }
    return left.length - right.length;
};

const compareCodePoints = (left, right) => {
    const leftPoints = Array.from(String(left), (value) => value.codePointAt(0));
    const rightPoints = Array.from(String(right), (value) => value.codePointAt(0));
    for (let index = 0; index < Math.min(leftPoints.length, rightPoints.length); index += 1) {
        if (leftPoints[index] !== rightPoints[index]) return leftPoints[index] - rightPoints[index];
    }
    return leftPoints.length - rightPoints.length;
};

const validateSourcePath = (value) => {
    if (typeof value !== "string" || !value || value.startsWith("/") || value.endsWith("/") || value.includes("\\") || value.includes("\0") || value.split("/").some((part) => !part || part === "." || part === "..")) {
        fail("package_semantics_invalid", "sourcePath must be a normalized relative POSIX path");
    }
};

const safeBasename = (value, label) => {
    if (typeof value !== "string" || !value || /[\\/:\0]/.test(value) || value === "." || value === "..") fail("package_semantics_invalid", `${label} must be a safe basename`);
};

const documentId = (sourceId, sourcePath) => `d_${sha256(Buffer.from(`${sourceId}\0${sourcePath}`, "utf8"))}`;
const sha256 = (value) => createHash("sha256").update(value).digest("hex");
const fail = (code, message) => { throw new PackageContractError(code, message); };

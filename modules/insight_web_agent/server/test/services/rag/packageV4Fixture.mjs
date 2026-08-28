/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { deflateRawSync } from "node:zlib";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import { CHECKSUM_MEMBERS, PACKAGE_MEMBERS } from "../../../services/rag/wire/packageContracts.mjs";

export const MODEL_CONTRACT = Object.freeze({
    schemaVersion: "1.0",
    modelId: "BAAI/bge-small-zh-v1.5",
    modelRevision: "75c43b069aac4d136ba6bc1122f995fedcfd2781",
    tokenizerId: "BAAI/bge-small-zh-v1.5",
    dimension: 512,
    maxSequenceLength: 512,
    pooling: "mean",
    normalize: true,
    queryPrefix: "",
    inputs: { inputIds: "input_ids", attentionMask: "attention_mask", tokenTypeIds: "token_type_ids" },
    outputName: "last_hidden_state",
    fileDigests: {},
    manifestSha256: "4e4eb912426215a0a7b0b5aec49da786d81910cbd7f3cf76561581eb69acd123",
});

export const PROFILE_SHA256 = "ec0cdf7879f9bd2f2ea9315e7bb22a420277b99369c2bb4466046a8f20db5d66";
const CONTRACT_SHA256 = "d".repeat(64);
export const DOMAIN_DICTIONARY_TEXT = "data collection\ndistributed training\nmemory analysis\nperformance analysis\n数据处理\n系统性能\n";
const DOMAIN_DICTIONARY_BYTES = Buffer.from(DOMAIN_DICTIONARY_TEXT, "utf8");
const DOMAIN_DICTIONARY_SHA256 = createHash("sha256").update(DOMAIN_DICTIONARY_BYTES).digest("hex");
const ALLOWED_CATEGORIES = ["user_guide", "install_guide", "quick_start", "best_practices", "support", "overview"];
export const SUMMARY_CONTRACT = Object.freeze({
    provider: "opencode",
    providerAdapter: "opencode-cli-v1",
    defaultModel: "CodeasierRouterOpenAIResponses/gpt-5.5",
    allowedModels: ["CodeasierRouterOpenAIResponses/gpt-5.5"],
    opencodeVersionConstraint: ">=1.18.18",
    promptVersion: "mindstudio-summary-prompt-v1",
    outputSchemaVersion: "1.0",
    tokenCounter: "bge-small-zh-v1.5-tokenizer-v1",
    maxSummaryTokens: 400,
});
const MANIFEST_SUMMARY = Object.freeze({
    provider: SUMMARY_CONTRACT.provider,
    providerAdapter: SUMMARY_CONTRACT.providerAdapter,
    opencodeVersion: "1.18.18",
    model: SUMMARY_CONTRACT.defaultModel,
    promptVersion: SUMMARY_CONTRACT.promptVersion,
    outputSchemaVersion: SUMMARY_CONTRACT.outputSchemaVersion,
    maxSummaryTokens: SUMMARY_CONTRACT.maxSummaryTokens,
});
const EMBEDDING_CONTRACT = {
    provider: "onnx-runtime",
    modelId: MODEL_CONTRACT.modelId,
    modelRevision: MODEL_CONTRACT.modelRevision,
    tokenizerId: MODEL_CONTRACT.tokenizerId,
    dimension: MODEL_CONTRACT.dimension,
    maxSequenceLength: MODEL_CONTRACT.maxSequenceLength,
    pooling: MODEL_CONTRACT.pooling,
    normalize: MODEL_CONTRACT.normalize,
    dtype: "float32",
    layout: "row-major",
    endianness: "little",
    queryPrefix: MODEL_CONTRACT.queryPrefix,
    modelManifestSha256: MODEL_CONTRACT.manifestSha256,
};
const RETRIEVAL_CONTRACT = {
    vectorMetric: "cosine",
    vectorStoredAs: "normalized-dot-product",
    bm25Tokenizer: "jieba_search_hmm0+domain_terms+latin",
    bm25K1: 1.5,
    bm25B: 0.75,
    domainDictionarySha256: DOMAIN_DICTIONARY_SHA256,
    fusionType: "rrf",
    rrfK: 60,
    vectorTopK: 30,
    bm25TopK: 30,
    finalTopK: 5,
};

export const RUNTIME_CONTRACT = Object.freeze({
    schemaVersion: "1.0",
    packageSchemaVersion: "4.0",
    packageMembers: PACKAGE_MEMBERS,
    resourceLimits: {
        maxArchiveBytes: 512 * 1024 * 1024,
        maxMemberBytes: 512 * 1024 * 1024,
        maxUncompressedBytes: 1024 * 1024 * 1024,
        maxJsonlLineBytes: 400_000,
    },
    embedding: {
        modelId: MODEL_CONTRACT.modelId,
        modelRevision: MODEL_CONTRACT.modelRevision,
        tokenizerId: MODEL_CONTRACT.tokenizerId,
        dimension: MODEL_CONTRACT.dimension,
        maxSequenceLength: MODEL_CONTRACT.maxSequenceLength,
        pooling: MODEL_CONTRACT.pooling,
        normalize: MODEL_CONTRACT.normalize,
        queryPrefix: MODEL_CONTRACT.queryPrefix,
        modelManifestSha256: MODEL_CONTRACT.manifestSha256,
    },
    retrieval: {
        vectorMetric: RETRIEVAL_CONTRACT.vectorMetric,
        vectorStoredAs: RETRIEVAL_CONTRACT.vectorStoredAs,
        bm25Tokenizer: RETRIEVAL_CONTRACT.bm25Tokenizer,
        bm25K1: RETRIEVAL_CONTRACT.bm25K1,
        bm25B: RETRIEVAL_CONTRACT.bm25B,
        fusionType: RETRIEVAL_CONTRACT.fusionType,
        rrfK: RETRIEVAL_CONTRACT.rrfK,
        vectorTopK: RETRIEVAL_CONTRACT.vectorTopK,
        bm25TopK: RETRIEVAL_CONTRACT.bm25TopK,
        finalTopK: RETRIEVAL_CONTRACT.finalTopK,
    },
    contractSha256: CONTRACT_SHA256,
});

export const createPackageV4Members = ({
    kbVersion = "26.1.1",
    builderVersion = "0.1.0",
    buildScopeMode = "project-id",
    includeSkipped = false,
} = {}) => {
    const sourceSet = { sourceSetId: `ss_${"1".repeat(64)}`, sha256: "2".repeat(64) };
    const buildProfile = { profileId: "msinsight-default", sha256: PROFILE_SHA256 };
    const source = {
        sourceId: "group/repo",
        projectId: "msinsight",
        remote: "https://example.com/repo.git",
        branch: "main",
        commit: "a".repeat(40),
        inventorySha256: "b".repeat(64),
        lockSha256: "c".repeat(64),
        sourceRole: "primary",
        dataClassification: "public",
    };
    const sourcePath = "docs/user_guide/overview.md";
    const docId = `d_${sha256(Buffer.from(`${source.sourceId}\0${sourcePath}`, "utf8"))}`;
    const document = {
        docId,
        sourceId: source.sourceId,
        sourcePath,
        contentSha256: "3".repeat(64),
        mediaType: "text/markdown",
        version: { label: "dev", inferredFrom: "branch_name", confidence: "high" },
        documentCategory: "user_guide",
        duplicateOfDocId: null,
    };
    const knowledgeText = "# 系统概览\n\nMindStudio Insight 支持离线分析。\n";
    const chunk = {
        chunkId: "c_29f0604d2564c0203e873b3d8b5d0571f3f188f105835b8edafc7a7502796295",
        docId,
        projectId: "msinsight",
        documentCategory: "user_guide",
        docName: "overview.md",
        title: "系统概览",
        sourceLabel: "MindStudio Insight / 用户指南 / 系统概览",
        sectionPath: ["系统概览"],
        chunkIndex: 0,
        contentFormat: "text",
        textSummary: "介绍 MindStudio Insight 的离线分析工作流。",
        knowledgeText,
    };
    const sourcesBytes = canonicalJsonlBytes([source]);
    const documentsBytes = canonicalJsonlBytes([document]);
    const chunksBytes = canonicalJsonlBytes([chunk]);
    const vectors = Buffer.alloc(MODEL_CONTRACT.dimension * 4);
    vectors.writeFloatLE(1, 0);
    const bm25 = {
        schemaVersion: "4.0",
        tokenizer: RETRIEVAL_CONTRACT.bm25Tokenizer,
        k1: RETRIEVAL_CONTRACT.bm25K1,
        b: RETRIEVAL_CONTRACT.bm25B,
        docCount: 1,
        docLengths: [0],
        avgDocLength: 0,
        terms: {},
    };
    const selectedDocuments = includeSkipped ? 2 : 1;
    const parseFailed = includeSkipped ? 1 : 0;
    const corpusCounts = {
        discoveredDocuments: selectedDocuments,
        selectedDocuments,
        excludedDocuments: 0,
        canonicalAttempted: selectedDocuments,
        canonicalIndexed: 1,
        duplicates: 0,
        parseFailed,
        parseFailureDocuments: parseFailed,
        catalogDocuments: 1,
        chunks: 1,
        faqChunks: 0,
        maxChunkChars: knowledgeText.length,
    };
    const summarySetDigest = "7".repeat(64);
    const parseFailure = {
        sourceId: source.sourceId,
        sourcePath: "docs/user_guide/skipped.md",
        contentSha256: "d".repeat(64),
        documentCategory: "user_guide",
        reasonCode: "parse-failed",
        errorCode: "unclosed-fence",
        disposition: "skipped",
        blocking: false,
    };
    const corpusGates = {
        canonicalContentOwnerFailures: 0,
        sourceCoverageFailures: 0,
        sourceDuplicationFailures: 0,
        codeReconstructionFailures: 0,
        tableReconstructionFailures: 0,
        chunkTooLargeFailures: 0,
        identityCollisionFailures: 0,
        localPathLeakageFailures: 0,
        passed: true,
    };
    const audit = {
        schemaVersion: "1.1",
        artifactType: "rag-package-build-audit",
        sourceSet,
        buildProfile,
        buildScope: buildScopeMode === "all-enabled"
            ? {
                mode: "all-enabled",
                projectIds: [],
                enabledSourceCount: 1,
                selectedSourceCount: 1,
                releaseEligible: true,
            }
            : {
                mode: "project-id",
                projectIds: [source.projectId],
                enabledSourceCount: 113,
                selectedSourceCount: 1,
                releaseEligible: false,
            },
        corpus: {
            schemaVersion: "1.1",
            artifactType: "rag-corpus-build-audit",
            sourceSet,
            profile: buildProfile,
            counts: corpusCounts,
            categories: { selected: { user_guide: selectedDocuments }, excluded: {} },
            excludedDocuments: [],
            parse: {
                overall: {
                    attempted: selectedDocuments,
                    failed: parseFailed,
                    failureRate: parseFailed / selectedDocuments,
                    warningThreshold: 0.005,
                    warningThresholdExceeded: parseFailed > 0,
                },
                sources: [{
                    sourceId: source.sourceId,
                    attempted: selectedDocuments,
                    failed: parseFailed,
                    failureRate: parseFailed / selectedDocuments,
                    warningThreshold: 0.05,
                    warningThresholdExceeded: parseFailed > 0,
                }],
                failures: includeSkipped ? [parseFailure] : [],
            },
            chunkFacts: {
                semanticUnitsByType: { document: 1 },
                contentFormats: { text: 1 },
                splitSemanticUnits: 0,
                faqMissingAnswers: 0,
            },
            semanticDigests: {
                selectedDocumentsSha256: "4".repeat(64),
                documentsSha256: sha256(documentsBytes),
                chunksSha256: sha256(chunksBytes),
            },
            gates: corpusGates,
        },
        summary: {
            model: MANIFEST_SUMMARY.model,
            opencodeVersion: MANIFEST_SUMMARY.opencodeVersion,
            chunks: 1,
            remoteHits: 1,
            localConfirmations: 0,
            generated: 0,
            batches: 0,
            maxSummaryTokens: 8,
            summarySetDigest,
        },
        embedding: { inputs: 1, rows: 1, maxInputTokens: 41, inputsOverLimit: 0, truncatedInputs: 0 },
        bm25: { inputs: 1, docCount: 1, termCount: 0 },
        records: { sources: 1, documents: 1, chunks: 1, vectorRows: 1, bm25Documents: 1 },
        semanticDigests: {
            sourcesSha256: sha256(sourcesBytes),
            documentsSha256: sha256(documentsBytes),
            chunksSha256: sha256(chunksBytes),
            embeddingInputsSha256: "8".repeat(64),
            bm25InputsSha256: "9".repeat(64),
        },
        gates: {
            corpusPassed: true,
            embeddingInputLimitPassed: true,
            embeddingVectorsPassed: true,
            bm25BindingPassed: true,
            localPathLeakageFailures: 0,
            dlpFindings: 0,
            passed: true,
        },
    };
    const manifest = {
        schemaVersion: "4.0",
        kbId: "mindstudio-insight-ascend",
        kbVersion,
        sourceSet,
        builder: {
            name: "ascend-rag-knowledge",
            version: builderVersion,
            repositoryCommit: "f".repeat(40),
            dependencyLockSha256: "a40b93e8782b88798b629d8daa61950d9827cfcb322704a6f51e03cc4424e575",
        },
        buildProfile,
        corpusPolicy: { allowedDocumentCategories: ALLOWED_CATEGORIES },
        chunk: {
            strategy: "document-source-range-v4",
            maxChars: 50000,
            embeddingMaxTokens: 512,
            parentDocumentsIncluded: false,
            adjacentContentDuplicated: false,
            agentContextMaxChars: 100000,
        },
        summary: { ...MANIFEST_SUMMARY, summarySetDigest },
        embedding: EMBEDDING_CONTRACT,
        retrieval: {
            vector: { metric: RETRIEVAL_CONTRACT.vectorMetric, storedAs: RETRIEVAL_CONTRACT.vectorStoredAs },
            keyword: {
                type: "bm25",
                tokenizer: RETRIEVAL_CONTRACT.bm25Tokenizer,
                k1: RETRIEVAL_CONTRACT.bm25K1,
                b: RETRIEVAL_CONTRACT.bm25B,
                domainDictionarySha256: RETRIEVAL_CONTRACT.domainDictionarySha256,
            },
            fusion: {
                type: RETRIEVAL_CONTRACT.fusionType,
                k: RETRIEVAL_CONTRACT.rrfK,
                vectorTopK: RETRIEVAL_CONTRACT.vectorTopK,
                bm25TopK: RETRIEVAL_CONTRACT.bm25TopK,
                finalTopK: RETRIEVAL_CONTRACT.finalTopK,
            },
        },
        stats: {
            sources: 1,
            selectedDocuments,
            catalogDocuments: 1,
            canonicalAttempted: selectedDocuments,
            canonicalIndexed: 1,
            duplicates: 0,
            parseFailed,
            chunks: 1,
            faqChunks: 0,
            maxChunkChars: knowledgeText.length,
        },
    };
    const members = new Map([
        ["manifest.json", canonicalJsonBytes(manifest)],
        ["sources.jsonl", sourcesBytes],
        ["documents.jsonl", documentsBytes],
        ["chunks.jsonl", chunksBytes],
        ["vectors.f32", vectors],
        ["bm25-domain-dict.txt", DOMAIN_DICTIONARY_BYTES],
        ["bm25.json", canonicalJsonBytes(bm25)],
        ["build-audit.json", canonicalJsonBytes(audit)],
    ]);
    members.set("checksums.json", canonicalJsonBytes({
        schemaVersion: "4.0",
        algorithm: "sha256",
        files: Object.fromEntries(CHECKSUM_MEMBERS.map((name) => [name, sha256(members.get(name))])),
    }));
    return members;
};

export const writePackageV4Handoff = async (root, options = {}) => {
    const handoffDir = join(root, options.directory ?? options.kbVersion ?? "26.1.1");
    await mkdir(handoffDir, { recursive: true });
    const members = options.members ?? createPackageV4Members(options);
    const archive = createCanonicalZip(members, options.zipOptions);
    const digest = sha256(archive);
    const archivePath = join(handoffDir, "knowledge-pack-v4.zip");
    const sidecarPath = join(handoffDir, "knowledge-pack-v4.zip.sha256");
    await writeFile(archivePath, archive);
    await writeFile(sidecarPath, options.sidecarBytes ?? Buffer.from(`${digest}  knowledge-pack-v4.zip\n`, "ascii"));
    return { archive, archivePath, sidecarPath, digest, members, handoffDir };
};

export const createCanonicalZip = (members, {
    names = PACKAGE_MEMBERS,
    comment = Buffer.alloc(0),
    lastModTime = 0,
    lastModDate = 33,
    externalAttributes = 0o100644 << 16,
} = {}) => {
    const locals = [];
    const central = [];
    let offset = 0;
    for (const name of names) {
        const payload = Buffer.from(members.get(name) ?? Buffer.from("unexpected", "utf8"));
        const fileName = Buffer.from(name, "utf8");
        const compressed = deflateRawSync(payload, { level: 9 });
        const crc = crc32(payload);
        const local = Buffer.concat([
            u32(0x04034b50), u16(20), u16(0), u16(8), u16(lastModTime), u16(lastModDate),
            u32(crc), u32(compressed.length), u32(payload.length), u16(fileName.length), u16(0), fileName, compressed,
        ]);
        locals.push(local);
        central.push(Buffer.concat([
            u32(0x02014b50), u16(0x0314), u16(20), u16(0), u16(8), u16(lastModTime), u16(lastModDate),
            u32(crc), u32(compressed.length), u32(payload.length), u16(fileName.length), u16(0), u16(0),
            u16(0), u16(0), u32(externalAttributes), u32(offset), fileName,
        ]));
        offset += local.length;
    }
    const centralBytes = Buffer.concat(central);
    const archiveComment = Buffer.from(comment);
    return Buffer.concat([
        ...locals,
        centralBytes,
        u32(0x06054b50), u16(0), u16(0), u16(names.length), u16(names.length),
        u32(centralBytes.length), u32(offset), u16(archiveComment.length), archiveComment,
    ]);
};

const canonicalJsonlBytes = (values) => Buffer.concat(values.map(canonicalJsonBytes));
export const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const u16 = (value) => Buffer.from([value & 0xff, (value >>> 8) & 0xff]);
const u32 = (value) => Buffer.from([value & 0xff, (value >>> 8) & 0xff, (value >>> 16) & 0xff, (value >>> 24) & 0xff]);

const crc32 = (data) => {
    let crc = 0xffffffff;
    for (const byte of data) {
        crc ^= byte;
        for (let index = 0; index < 8; index += 1) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
    return (crc ^ 0xffffffff) >>> 0;
};

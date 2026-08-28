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

import { loadActiveKnowledgePack } from "./knowledgePackLoader.mjs";
import { createBm25Retriever } from "./bm25Retriever.mjs";
import {
    BM25_TOP_K,
    buildProjectEntityCatalog,
    detectExplicitProjects,
    FINAL_TOP_K,
    fuseRrf,
    rerankCandidates,
    VECTOR_TOP_K,
} from "./hybridRetriever.mjs";
import { createVectorRetriever } from "./vectorRetriever.mjs";

const DEFAULT_TOP_K = 5;
const DEFAULT_MAX_CONTEXT_CHARS = 100000;
const MAX_RESULTS_PER_DOCUMENT = 2;
const MAX_WAITING_QUERIES = 2;

export class RagBusyError extends Error {
    constructor() {
        super("RAG query queue is full");
        this.name = "RagBusyError";
        this.code = "rag_busy";
    }
}

export const createRagService = async ({
    config = {},
    logger = console,
    loadPack = loadActiveKnowledgePack,
    createEmbedding,
    platform = process.platform,
    arch = process.arch,
} = {}) => {
    const ragConfig = normalizeConfig(config.rag ?? config);
    if (!ragConfig.enabled) return createDisabledRagService("disabled");
    if (platform !== "win32" || arch !== "x64") return createDisabledRagService("unsupported_rag_platform");
    try {
        const createService = createEmbedding ?? (await import("./embeddingService.mjs")).createEmbeddingService;
        const embeddingService = await createService({ modelDir: ragConfig.modelDir, platform, arch });
        const knowledgePack = await loadPack(ragConfig.ragDataDir, {
            runtimeDir: ragConfig.runtimeDir,
            modelContract: embeddingService.contract,
        });
        logger?.info?.(`RAG loaded: ${knowledgePack.manifest.kbId}/${knowledgePack.manifest.kbVersion}; chunks=${knowledgePack.chunks.length}`);
        return createEnabledRagService({ config: ragConfig, knowledgePack, embeddingService, logger });
    } catch (error) {
        logger?.warn?.(`RAG disabled: ${stableError(error)}`);
        if (!ragConfig.failOpen) throw error;
        return createDisabledRagService(error?.code ?? "load_failed");
    }
};

export const createDisabledRagService = (reason = "disabled") => ({
    isEnabled: () => false,
    getStatus: () => ({ enabled: false, reason }),
    retrieve: async () => undefined,
});

export const createEnabledRagService = ({ config = {}, knowledgePack, embeddingService, logger = console }) => {
    const ragConfig = normalizeConfig(config);
    const projectCatalog = buildProjectEntityCatalog(knowledgePack.chunks);
    const vectorRetriever = createVectorRetriever({
        vectors: knowledgePack.vectors,
        dimension: knowledgePack.manifest.embedding.dimension,
        chunkCount: knowledgePack.chunks.length,
    });
    const bm25Retriever = createBm25Retriever(knowledgePack.domainDictionary);
    const queue = createFifoQueue(MAX_WAITING_QUERIES);
    const status = {
        enabled: true,
        kbId: knowledgePack.manifest.kbId,
        kbVersion: knowledgePack.manifest.kbVersion,
        packageSha256: knowledgePack.install?.package?.sha256,
        chunks: knowledgePack.chunks.length,
        retrievalMode: "hybrid_rrf",
    };
    return {
        isEnabled: () => true,
        getStatus: () => ({ ...status, queue: queue.status() }),
        async retrieve(query, options = {}) {
            const normalizedQuery = String(query ?? "").trim();
            if (!normalizedQuery) return undefined;
            try {
                return await queue.run(async () => {
                    const { retrievalQuery, inheritedProjectId } = completeRetrievalQuery(normalizedQuery, options.previousUserText, projectCatalog);
                    const [queryVector] = await embeddingService.embedQueries([retrievalQuery]);
                    const vectorRanked = vectorRetriever.rank(queryVector, VECTOR_TOP_K);
                    const bm25Ranked = bm25Retriever.rank(knowledgePack.bm25, retrievalQuery, BM25_TOP_K);
                    const fused = fuseRrf([vectorRanked, bm25Ranked], { limit: VECTOR_TOP_K + BM25_TOP_K });
                    const reranked = rerankCandidates(retrievalQuery, fused, knowledgePack.chunks, vectorRanked, bm25Ranked, projectCatalog);
                    const resultLimit = Math.min(ragConfig.topK, FINAL_TOP_K);
                    const primaryRanked = diversifyByDocument(reranked, knowledgePack.chunks, resultLimit);
                    const primaryTop5 = primaryRanked.map((item, index) => toRetrievedChunk(item, index, knowledgePack, "hit"));
                    const retrievedChunks = assembleContext(primaryRanked, knowledgePack, ragConfig.maxContextChars);
                    const result = {
                        schemaVersion: "1.0",
                        status: primaryTop5.length ? "ok" : "no_match",
                        retrievalMode: "hybrid_rrf",
                        kbId: knowledgePack.manifest.kbId,
                        kbVersion: knowledgePack.manifest.kbVersion,
                        query: normalizedQuery,
                        ...(inheritedProjectId ? { retrievalQuery, inheritedProjectId } : {}),
                        primaryTop5,
                        retrievedChunks,
                    };
                    if (ragConfig.debug) logger?.debug?.("RAG retrieval completed", {
                        status: result.status,
                        primaryHits: primaryTop5.map(({ rank, chunkId, docId }) => ({ rank, chunkId, docId })),
                    });
                    return result;
                });
            } catch (error) {
                logger?.warn?.(`RAG retrieval failed: ${stableError(error)}`);
                if (error?.code === "rag_busy" && options.propagateBusy === true) throw error;
                if (ragConfig.failOpen) return undefined;
                throw error;
            }
        },
    };
};

const createFifoQueue = (maxWaiting) => {
    const waiting = [];
    let active = false;
    const drain = () => {
        if (active || !waiting.length) return;
        active = true;
        const { task, resolve, reject } = waiting.shift();
        Promise.resolve().then(task).then(resolve, reject).finally(() => {
            active = false;
            drain();
        });
    };
    return {
        run(task) {
            if (active && waiting.length >= maxWaiting) return Promise.reject(new RagBusyError());
            return new Promise((resolve, reject) => {
                waiting.push({ task, resolve, reject });
                drain();
            });
        },
        status: () => ({ active, waiting: waiting.length, maxWaiting }),
    };
};

const normalizeConfig = (config = {}) => ({
    enabled: config.enabled !== false,
    ragDataDir: String(config.ragDataDir ?? "").trim(),
    modelDir: String(config.modelDir ?? "").trim(),
    runtimeDir: String(config.runtimeDir ?? "").trim(),
    topK: normalizePositiveInteger(config.topK, DEFAULT_TOP_K),
    maxContextChars: normalizePositiveInteger(config.maxContextChars, DEFAULT_MAX_CONTEXT_CHARS),
    failOpen: config.failOpen !== false,
    debug: config.debug === true,
});

const normalizePositiveInteger = (value, fallback) => {
    const number = Number(value);
    return Number.isInteger(number) && number > 0 ? number : fallback;
};

const toRetrievedChunk = ({ index, score }, rankIndex, knowledgePack, expansion) => {
    const chunk = knowledgePack.chunks[index];
    const sectionPath = Array.isArray(chunk.sectionPath) ? chunk.sectionPath : [];
    return {
        rank: rankIndex + 1,
        chunkId: chunk.chunkId,
        docId: chunk.docId,
        projectId: chunk.projectId,
        documentCategory: chunk.documentCategory,
        sourceLabel: chunk.sourceLabel,
        title: chunk.title,
        section: sectionPath.at(-1) ?? chunk.title,
        contentFormat: chunk.contentFormat,
        score,
        textSummary: chunk.textSummary,
        ...(chunk.answerStatus ? { answerStatus: chunk.answerStatus } : {}),
        knowledgeText: chunk.knowledgeText,
        expansion,
    };
};

const assembleContext = (ranked, knowledgePack, maxContextChars) => {
    const expanded = [];
    const seen = new Set();
    const add = (index, rank, score, expansion) => {
        const chunk = knowledgePack.chunks[index];
        if (!chunk || seen.has(chunk.chunkId)) return;
        seen.add(chunk.chunkId);
        expanded.push(toRetrievedChunk({ index, score }, rank - 1, knowledgePack, expansion));
    };
    for (const [rankIndex, item] of ranked.entries()) {
        const hit = knowledgePack.chunks[item.index];
        if (!hit) continue;
        add(item.index, rankIndex + 1, item.score, "hit");
        for (const chunk of knowledgePack.chunksByFaqId.get(hit.faqId) ?? []) {
            add(knowledgePack.chunks.indexOf(chunk), rankIndex + 1, item.score, "faq_part");
        }
    }
    const constrained = [];
    let remaining = maxContextChars;
    for (const chunk of expanded) {
        const cost = serializedChunkChars(chunk);
        if (cost > remaining) continue;
        constrained.push(chunk);
        remaining -= cost;
    }
    return constrained;
};

const completeRetrievalQuery = (query, previousUserText, projectCatalog) => {
    if (detectExplicitProjects(query, projectCatalog).size) return { retrievalQuery: query };
    const previousProjects = detectExplicitProjects(previousUserText, projectCatalog);
    if (previousProjects.size !== 1) return { retrievalQuery: query };
    const [inheritedProjectId] = previousProjects;
    return { retrievalQuery: `${inheritedProjectId} ${query}`, inheritedProjectId };
};

const serializedChunkChars = (chunk) => [
    chunk.sourceLabel,
    chunk.title,
    chunk.section,
    chunk.knowledgeText,
    chunk.answerStatus,
].reduce((total, value) => total + String(value ?? "").length, 0) + 8;

const diversifyByDocument = (ranked, chunks, limit) => {
    const counts = new Map();
    const selected = [];
    for (const item of ranked) {
        const docId = chunks[item.index]?.docId;
        const count = counts.get(docId) ?? 0;
        if (count >= MAX_RESULTS_PER_DOCUMENT) continue;
        selected.push(item);
        counts.set(docId, count + 1);
        if (selected.length >= limit) break;
    }
    return selected;
};

const stableError = (error) => String(error?.code ?? error?.name ?? "rag_error");

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { createEnabledRagService, createRagService } from "../../../services/rag/ragService.mjs";

test("primary Top-5 remains separate from FAQ expansion and whole-chunk context budgeting", async () => {
    const chunks = [
        chunk({ chunkId: "hit", docId: "faq-doc", faqId: "q1", faqPartIndex: 0, answerStatus: "answered", knowledgeText: "命中" }),
        chunk({ chunkId: "large", docId: "faq-doc", chunkIndex: 1, faqId: "q1", faqPartIndex: 1, answerStatus: "answered", knowledgeText: "x".repeat(100) }),
        chunk({ chunkId: "small", docId: "faq-doc", chunkIndex: 2, faqId: "q1", faqPartIndex: 2, answerStatus: "answered", knowledgeText: "后续" }),
    ];
    const service = createEnabledRagService({
        knowledgePack: pack(chunks, { postings: [[0, 2]] }),
        embeddingService: immediateEmbedding(),
        config: { topK: 1, maxContextChars: serializedCost(chunks[0]) + serializedCost(chunks[2]) },
    });

    const result = await service.retrieve("analysis");

    assert.deepEqual(result.primaryTop5.map(({ chunkId }) => chunkId), ["hit"]);
    assert.deepEqual(result.retrievedChunks.map(({ chunkId, expansion }) => [chunkId, expansion]), [
        ["hit", "hit"],
        ["small", "faq_part"],
    ]);
});

test("document diversity permits at most two primary hits from one document", async () => {
    const chunks = [
        chunk({ chunkId: "a0", docId: "a", chunkIndex: 0 }),
        chunk({ chunkId: "a1", docId: "a", chunkIndex: 1 }),
        chunk({ chunkId: "a2", docId: "a", chunkIndex: 2 }),
        chunk({ chunkId: "b0", docId: "b", chunkIndex: 0 }),
        chunk({ chunkId: "c0", docId: "c", chunkIndex: 0 }),
    ];
    const service = createEnabledRagService({
        knowledgePack: pack(chunks),
        embeddingService: immediateEmbedding(),
        config: { topK: 5, maxContextChars: 100_000 },
    });

    const result = await service.retrieve("unmatched");

    assert.deepEqual(result.primaryTop5.map(({ chunkId }) => chunkId), ["a0", "a1", "b0", "c0"]);
});

test("one active query and two waiting queries execute in FIFO order", async () => {
    const embedding = deferredEmbedding();
    const service = createEnabledRagService({
        knowledgePack: pack([chunk()]),
        embeddingService: embedding.service,
        config: { failOpen: false },
    });

    const first = service.retrieve("first");
    const second = service.retrieve("second");
    const third = service.retrieve("third");
    await embedding.waitForStarts(1);
    assert.deepEqual(service.getStatus().queue, { active: true, waiting: 2, maxWaiting: 2 });

    embedding.release("first");
    await embedding.waitForStarts(2);
    embedding.release("second");
    await embedding.waitForStarts(3);
    embedding.release("third");
    await Promise.all([first, second, third]);

    assert.deepEqual(embedding.starts, ["first", "second", "third"]);
    assert.deepEqual(service.getStatus().queue, { active: false, waiting: 0, maxWaiting: 2 });
});

test("a full query queue returns rag_busy in fail-closed mode and skips RAG in fail-open mode", async () => {
    for (const failOpen of [false, true]) {
        const embedding = deferredEmbedding();
        const service = createEnabledRagService({
            knowledgePack: pack([chunk()]),
            embeddingService: embedding.service,
            config: { failOpen },
            logger: { warn: () => {} },
        });
        const pending = [service.retrieve("one"), service.retrieve("two"), service.retrieve("three")];
        const overflow = service.retrieve("overflow");
        const capabilityOverflow = service.retrieve("capability-overflow", { propagateBusy: true });

        if (failOpen) assert.equal(await overflow, undefined);
        else await assert.rejects(overflow, (error) => error?.code === "rag_busy");
        await assert.rejects(capabilityOverflow, (error) => error?.code === "rag_busy");

        for (const [index, query] of ["one", "two", "three"].entries()) {
            await embedding.waitForStarts(index + 1);
            embedding.release(query);
        }
        await Promise.all(pending);
    }
});

test("query completion inherits exactly one previous project without mutating the visible query", async () => {
    const queries = [];
    const chunks = [
        chunk({ projectId: "project-alpha", sourceLabel: "Project Alpha / Guide" }),
        chunk({ chunkId: "beta", docId: "beta", projectId: "project-beta", sourceLabel: "Project Beta / Guide" }),
    ];
    const service = createEnabledRagService({
        knowledgePack: pack(chunks),
        embeddingService: { embedQueries: async ([query]) => { queries.push(query); return [unitVector()]; } },
        config: { topK: 1 },
    });

    const result = await service.retrieve("next steps", { previousUserText: "Project Alpha setup" });

    assert.equal(result.query, "next steps");
    assert.equal(result.retrievalQuery, "project-alpha next steps");
    assert.equal(result.inheritedProjectId, "project-alpha");
    assert.deepEqual(queries, ["project-alpha next steps"]);
});

test("createRagService disables unsupported platforms and honors load fail-open semantics", async () => {
    const unsupported = await createRagService({
        config: { enabled: true },
        platform: "linux",
        arch: "x64",
        createEmbedding: async () => { throw new Error("must not load"); },
    });
    assert.deepEqual(unsupported.getStatus(), { enabled: false, reason: "unsupported_rag_platform" });

    const failed = await createRagService({
        config: { enabled: true, failOpen: true },
        platform: "win32",
        arch: "x64",
        createEmbedding: async () => ({ contract: {}, embedQueries: async () => [] }),
        loadPack: async () => { throw Object.assign(new Error("missing"), { code: "pack_unreadable" }); },
        logger: { warn: () => {} },
    });
    assert.deepEqual(failed.getStatus(), { enabled: false, reason: "pack_unreadable" });
});

const pack = (chunks, { postings = [] } = {}) => {
    const vectors = new Float32Array(chunks.length * 2);
    for (let index = 0; index < chunks.length; index += 1) vectors[index * 2] = 1 - index * 0.1;
    return {
        manifest: { kbId: "mindstudio-insight-ascend", kbVersion: "26.1.1", embedding: { dimension: 2 } },
        install: { package: { sha256: "a".repeat(64) } },
        chunks,
        vectors,
        domainDictionary: "project alpha\nproject beta\n",
        bm25: {
            docCount: chunks.length,
            k1: 1.5,
            b: 0.75,
            avgDocLength: 1,
            docLengths: Array(chunks.length).fill(1),
            terms: postings.length ? { analysis: { idf: 1, postings } } : {},
        },
        chunksByFaqId: groupBy(chunks.filter(({ faqId }) => faqId), "faqId", "faqPartIndex"),
    };
};

const chunk = (overrides = {}) => ({
    chunkId: "chunk",
    docId: "doc",
    projectId: "project-alpha",
    documentCategory: "guide",
    sourceLabel: "Project Alpha / Guide",
    title: "Guide",
    sectionPath: ["Guide"],
    chunkIndex: 0,
    contentFormat: "text",
    textSummary: "Summary",
    knowledgeText: "Reference",
    ...overrides,
});

const groupBy = (items, key, order) => {
    const groups = new Map();
    for (const item of items) groups.set(item[key], [...(groups.get(item[key]) ?? []), item]);
    for (const values of groups.values()) values.sort((left, right) => left[order] - right[order]);
    return groups;
};

const unitVector = () => new Float32Array([1, 0]);
const immediateEmbedding = () => ({ embedQueries: async () => [unitVector()] });
const serializedCost = (value) => value.sourceLabel.length + value.title.length
    + value.sectionPath.at(-1).length + value.knowledgeText.length + String(value.answerStatus ?? "").length + 8;

const deferredEmbedding = () => {
    const pending = new Map();
    const starts = [];
    const waiters = [];
    const notify = () => {
        for (const waiter of [...waiters]) if (starts.length >= waiter.count) {
            waiters.splice(waiters.indexOf(waiter), 1);
            waiter.resolve();
        }
    };
    return {
        starts,
        service: {
            async embedQueries([query]) {
                starts.push(query);
                notify();
                return new Promise((resolve) => pending.set(query, () => resolve([unitVector()])));
            },
        },
        release(query) {
            const resolve = pending.get(query);
            assert.ok(resolve, `query did not start: ${query}`);
            pending.delete(query);
            resolve();
        },
        waitForStarts(count) {
            if (starts.length >= count) return Promise.resolve();
            return new Promise((resolve) => waiters.push({ count, resolve }));
        },
    };
};

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import {
    BM25_TOP_K,
    buildProjectEntityCatalog,
    detectExplicitProjects,
    FINAL_TOP_K,
    fuseRrf,
    rerankCandidates,
    RRF_K,
    VECTOR_TOP_K,
} from "../../../services/rag/hybridRetriever.mjs";

const fixture = JSON.parse(readFileSync(new URL("./fixtures/rerank-context-v1.json", import.meta.url), "utf8"));

test("hybrid candidate constants match the frozen rerank contract", () => {
    assert.deepEqual({ VECTOR_TOP_K, BM25_TOP_K, FINAL_TOP_K, RRF_K }, {
        VECTOR_TOP_K: fixture.candidateGeneration.vectorTopK,
        BM25_TOP_K: fixture.candidateGeneration.bm25TopK,
        FINAL_TOP_K: fixture.candidateGeneration.finalTopK,
        RRF_K: fixture.candidateGeneration.rrfK,
    });
});

test("RRF deduplicates candidates and resolves exact ties by ascending chunk index", () => {
    const rankings = fixture.fixtures.rrfStableTie.rankings.map((indexes) => indexes.map((index) => ({ index })));

    assert.deepEqual(fuseRrf(rankings, { limit: 2 }).map(({ index }) => index), fixture.fixtures.rrfStableTie.expectedIndexes);
});

test("project detection applies longest-span shadowing for punctuated project IDs", () => {
    const catalog = buildProjectEntityCatalog([
        chunk({ projectId: "project-alpha", sourceLabel: "Project Alpha / Guide" }),
        chunk({ projectId: "project-alpha-analyze", sourceLabel: "Project Alpha Analyze / Guide" }),
    ]);

    assert.deepEqual([...detectExplicitProjects(fixture.fixtures.explicitProject.query, catalog)], fixture.fixtures.explicitProject.expectedProjectIds);
});

test("reranking follows generic project and document evidence", () => {
    const inputs = fixture.fixtures.genericRerank;
    const candidates = inputs.candidates.map(({ index, score }) => ({ index, score }));
    const chunks = inputs.candidates.map((item) => chunk(item));
    const vector = inputs.candidates.filter(({ inVector }) => inVector).map(({ index, score }) => ({ index, score }));
    const bm25 = inputs.candidates.filter(({ inBm25 }) => inBm25).map(({ index, score }) => ({ index, score }));

    assert.deepEqual(
        rerankCandidates(inputs.query, candidates, chunks, vector, bm25).map(({ index }) => index),
        inputs.expectedIndexes,
    );
});

const chunk = (overrides = {}) => ({
    projectId: "project-alpha",
    sourceLabel: "Project Alpha / Guide",
    documentCategory: "guide",
    title: "Guide",
    sectionPath: ["Overview"],
    docName: "guide.md",
    knowledgeText: "Reference.",
    ...overrides,
});

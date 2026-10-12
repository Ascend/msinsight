/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const VECTOR_TOP_K = 30;
export const BM25_TOP_K = 30;
export const FINAL_TOP_K = 5;
export const RRF_K = 60;

export const fuseRrf = (rankings, { k = RRF_K, limit = FINAL_TOP_K } = {}) => {
    const scores = new Map();
    for (const ranking of rankings) {
        for (const [rank, item] of (Array.isArray(ranking) ? ranking : []).entries()) {
            if (!Number.isInteger(item?.index) || item.index < 0) continue;
            scores.set(item.index, (scores.get(item.index) ?? 0) + 1 / (k + rank + 1));
        }
    }
    return [...scores.entries()]
        .map(([index, score]) => ({ index, score }))
        .sort((left, right) => right.score - left.score || left.index - right.index)
        .slice(0, Math.max(0, Number(limit) || 0));
};

export const buildProjectEntityCatalog = (chunks = []) => {
    const projects = new Map();
    for (const chunk of chunks) {
        const projectId = String(chunk?.projectId ?? "").trim().toLowerCase();
        if (!projectId) continue;
        if (!projects.has(projectId)) projects.set(projectId, new Set([projectId]));
        const sourceLabel = String(chunk?.sourceLabel ?? "").split("/")[0].trim();
        if (sourceLabel) projects.get(projectId).add(sourceLabel);
    }
    return [...projects].map(([projectId, identifiers]) => ({
        projectId,
        identifiers: [...identifiers].map(compactEntity).filter(Boolean).sort((left, right) => right.length - left.length),
    }));
};

export const detectExplicitProjects = (query, catalog = []) => {
    const compactQuery = compactEntity(query);
    if (!compactQuery) return new Set();
    const matches = [];
    for (const project of catalog) {
        for (const identifier of project.identifiers ?? []) {
            let start = compactQuery.indexOf(identifier);
            while (start >= 0) {
                matches.push({ projectId: project.projectId, start, end: start + identifier.length });
                start = compactQuery.indexOf(identifier, start + 1);
            }
        }
    }
    const nonShadowed = matches.filter((match) => !matches.some((other) => (
        other.end - other.start > match.end - match.start
        && other.start <= match.start
        && other.end >= match.end
    )));
    return new Set(nonShadowed.sort((left, right) => left.start - right.start || right.end - left.end).map(({ projectId }) => projectId));
};

export const rerankCandidates = (query, candidates, chunks, vectorRanked = [], bm25Ranked = [], projectCatalog) => {
    const queryLower = String(query ?? "").toLowerCase();
    const explicitProjects = detectExplicitProjects(query, projectCatalog ?? buildProjectEntityCatalog(chunks));
    const vectorIndexes = new Set(vectorRanked.map((item) => item?.index));
    const bm25Indexes = new Set(bm25Ranked.map((item) => item?.index));
    const evidence = (item) => {
        const chunk = chunks?.[item.index] ?? {};
        const projectId = String(chunk.projectId ?? "").toLowerCase();
        const section = String(chunk.sectionPath?.at?.(-1) ?? chunk.title ?? "");
        const heading = `${chunk.title ?? ""} ${section}`.toLowerCase();
        const documentName = String(chunk.docName ?? "").toLowerCase();
        const documentMatch = Math.min(3, scoreDocumentMatch(queryLower, `${documentName} ${heading}`));
        return [
            Number(explicitProjects.size === 0 || explicitProjects.has(projectId)),
            documentMatch,
            Number(vectorIndexes.has(item.index) && bm25Indexes.has(item.index)),
            Number(item.score ?? 0),
            -item.index,
        ];
    };
    return [...candidates].sort((left, right) => compareEvidence(evidence(right), evidence(left)));
};

const scoreDocumentMatch = (query, documentText) => {
    const queryTerms = extractMatchTerms(query);
    if (!queryTerms.length) return 0;
    const documentTerms = new Set(extractMatchTerms(documentText));
    const matched = queryTerms.filter((term) => documentTerms.has(term));
    return matched.length * 2 + Number(matched.length === queryTerms.length);
};

const extractMatchTerms = (value) => {
    const terms = new Set();
    const normalized = String(value ?? "").toLowerCase();
    for (const word of normalized.match(/[a-z0-9][a-z0-9_+.-]*/g) ?? []) if (word.length >= 2) terms.add(word.replace(/[_-]+/g, " "));
    for (const run of normalized.match(/[\u4e00-\u9fff]{2,}/g) ?? []) {
        for (let index = 0; index < run.length - 1; index += 1) terms.add(run.slice(index, index + 2));
    }
    return [...terms];
};

const compareEvidence = (left, right) => {
    for (let index = 0; index < left.length; index += 1) if (left[index] !== right[index]) return left[index] - right[index];
    return 0;
};

const compactEntity = (value) => String(value ?? "").toLowerCase().replace(/[^a-z0-9\u4e00-\u9fff]+/g, "");

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createRequire } from "node:module";

export const TOKENIZER_NAME = "jieba_search_hmm0+domain_terms+latin";
export const DEFAULT_K1 = 1.5;
export const DEFAULT_B = 0.75;

const LATIN_RE = /[a-zA-Z][a-zA-Z0-9_.\/:\-]*/g;
const CJK_RE = /[一-鿿]+/g;
const TRIM_RE = /^[.,;:()[\]{}<>，。；：、“”‘’]+|[.,;:()[\]{}<>，。；：、“”‘’]+$/g;
const TOKEN_RE = /[\p{L}\p{N}_]/u;
const require = createRequire(import.meta.url);

export const createBm25Retriever = (domainDictionary) => {
    const tokenize = createTokenizer(domainDictionary);
    const score = (index, query) => scoreBm25(index, query, tokenize);
    return {
        tokenize,
        score,
        rank: (index, query, topK = 30) => score(index, query)
            .map((value, index) => ({ index, score: value }))
            .filter((item) => item.score > 0)
            .sort((left, right) => right.score - left.score || left.index - right.index)
            .slice(0, Math.max(0, Number(topK) || 0)),
    };
};

const createTokenizer = (domainDictionary) => {
    const { Jieba } = require("@node-rs/jieba");
    const { dict } = require("@node-rs/jieba/dict.js");
    const domainTerms = String(domainDictionary ?? "").split("\n")
        .map((line) => line.trim().toLowerCase())
        .filter(Boolean);
    const chineseDictionary = domainTerms
        .filter((term) => /^[一-鿿]+$/u.test(term))
        .map((term) => `${term} 100000 nz`)
        .join("\n");
    const jieba = Jieba.withDict(dict);
    jieba.loadDict(Buffer.from(`${chineseDictionary}\n`));
    const multiWordTerms = domainTerms.filter((term) => term.includes(" "));
    return (text) => {
        const lowered = String(text ?? "").toLowerCase();
        const tokens = [];
        for (const match of lowered.matchAll(LATIN_RE)) {
            const token = match[0].replace(TRIM_RE, "");
            if (token) tokens.push(token);
        }
        for (const match of lowered.matchAll(CJK_RE)) tokens.push(...jieba.cutForSearch(match[0], false));
        for (const term of multiWordTerms) if (lowered.includes(term)) tokens.push(term);
        return tokens.filter((token) => token && TOKEN_RE.test(token));
    };
};

const scoreBm25 = (index, query, tokenize) => {
    const docCount = Number(index?.docCount ?? 0);
    const scores = Array.from({ length: docCount }, () => 0);
    if (!docCount) return scores;
    const k1 = Number(index.k1 ?? DEFAULT_K1);
    const b = Number(index.b ?? DEFAULT_B);
    const avgDocLength = Number(index.avgDocLength ?? 0) || 1;
    const docLengths = Array.isArray(index.docLengths) ? index.docLengths : [];
    const queryCounts = countTokens(tokenize(query));
    for (const [term, queryFrequency] of queryCounts.entries()) {
        const termInfo = index.terms?.[term];
        if (!termInfo) continue;
        const idf = Number(termInfo.idf ?? 0);
        for (const posting of termInfo.postings ?? []) {
            const [docIndex, termFrequency] = posting;
            const docLength = Number(docLengths[docIndex] ?? avgDocLength) || avgDocLength;
            const denominator = termFrequency + k1 * (1 - b + b * docLength / avgDocLength);
            scores[docIndex] += queryFrequency * idf * (termFrequency * (k1 + 1)) / denominator;
        }
    }
    return scores;
};

const countTokens = (tokens) => {
    const counts = new Map();
    for (const token of tokens) counts.set(token, (counts.get(token) ?? 0) + 1);
    return counts;
};

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
import { createBm25Retriever } from "../../../services/rag/bm25Retriever.mjs";

test("frozen BM25 tokenizer fixture preserves domain terms and excludes sliding CJK noise", () => {
    const fixture = JSON.parse(readFileSync(new URL("./fixtures/bm25-tokenizer-fixture.json", import.meta.url), "utf8"));
    const retriever = createBm25Retriever(`${fixture.domainTerms.join("\n")}\n`);
    const tokens = retriever.tokenize(fixture.text);

    for (const token of fixture.requiredTokens) assert.ok(tokens.includes(token), `missing token: ${token}`);
    for (const token of fixture.forbiddenTokens) assert.equal(tokens.includes(token), false, `forbidden token: ${token}`);
});

test("BM25 ranks the frozen expected document deterministically", () => {
    const fixture = JSON.parse(readFileSync(new URL("./fixtures/bm25-tokenizer-fixture.json", import.meta.url), "utf8"));
    const retriever = createBm25Retriever(`${fixture.domainTerms.join("\n")}\n`);
    const docTokens = fixture.documents.map(retriever.tokenize);
    const terms = {};
    for (const term of [...new Set(docTokens.flat())].sort()) {
        const postings = docTokens.map((tokens, index) => [index, tokens.filter((token) => token === term).length])
            .filter(([, frequency]) => frequency > 0);
        terms[term] = { idf: 1, postings };
    }
    const index = {
        docCount: docTokens.length,
        k1: 1.5,
        b: 0.75,
        avgDocLength: docTokens.reduce((total, tokens) => total + tokens.length, 0) / docTokens.length,
        docLengths: docTokens.map((tokens) => tokens.length),
        terms,
    };

    const ranked = retriever.rank(index, fixture.query, fixture.documents.length);

    assert.equal(ranked[0].index, fixture.expectedTopDocument);
    assert.deepEqual(retriever.score({ docCount: 0 }, fixture.query), []);
});

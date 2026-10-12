/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { createVectorRetriever } from "../../../services/rag/vectorRetriever.mjs";

test("vector retriever ranks normalized dot products with stable chunk-index ties", () => {
    const retriever = createVectorRetriever({
        vectors: new Float32Array([1, 0, 1, 0, 0, 1]),
        dimension: 2,
        chunkCount: 3,
    });

    assert.deepEqual(retriever.rank(new Float32Array([1, 0]), 3), [
        { index: 0, score: 1 },
        { index: 1, score: 1 },
        { index: 2, score: 0 },
    ]);
});

test("vector retriever rejects row and query dimension mismatches", () => {
    assert.throws(
        () => createVectorRetriever({ vectors: new Float32Array([1, 0]), dimension: 3, chunkCount: 1 }),
        /vector rows do not match/,
    );
    const retriever = createVectorRetriever({ vectors: new Float32Array([1, 0]), dimension: 2, chunkCount: 1 });
    assert.throws(() => retriever.rank(new Float32Array([1])), /query vector does not match/);
});

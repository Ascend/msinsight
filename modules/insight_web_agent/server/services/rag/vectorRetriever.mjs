/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export const createVectorRetriever = ({ vectors, dimension, chunkCount }) => {
    if (!(vectors instanceof Float32Array)) throw new TypeError("vectors must be a Float32Array");
    if (!Number.isInteger(dimension) || dimension <= 0) throw new TypeError("dimension must be a positive integer");
    if (!Number.isInteger(chunkCount) || chunkCount <= 0 || vectors.length !== dimension * chunkCount) {
        throw new TypeError("vector rows do not match chunks and embedding dimension");
    }
    return {
        rank(queryVector, limit = 30) {
            if (!(queryVector instanceof Float32Array) || queryVector.length !== dimension) {
                throw new TypeError("query vector does not match embedding dimension");
            }
            const results = [];
            for (let row = 0; row < chunkCount; row += 1) {
                let score = 0;
                const offset = row * dimension;
                for (let column = 0; column < dimension; column += 1) score += vectors[offset + column] * queryVector[column];
                results.push({ index: row, score });
            }
            return results.sort((left, right) => right.score - left.score || left.index - right.index).slice(0, positiveLimit(limit));
        },
    };
};

const positiveLimit = (limit) => Math.max(0, Number.isInteger(Number(limit)) ? Number(limit) : 0);

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { createRagCapability } from "../../capability-center/ragCapability.mjs";

const enabledService = (retrieve) => ({
    isEnabled: () => true,
    getStatus: () => ({ enabled: true }),
    retrieve,
});

test("RAG capability projects retrieved chunks into a stable redacted result", async () => {
    const calls = [];
    const capability = createRagCapability({
        ragService: enabledService(async (query, options) => {
            calls.push({ query, options });
            return {
                status: "ok",
                kbId: "mindstudio-insight",
                kbVersion: "26.1.3",
                packageSha256: "secret-sha",
                retrievedChunks: [{
                    score: 0.98,
                    chunkId: "internal-chunk",
                    docId: "internal-document",
                    sourceLabel: "用户指南/数据导入",
                    projectId: "msinsight",
                    documentCategory: "user-guide",
                    title: "数据导入",
                    section: "导入性能数据",
                    contentFormat: "markdown",
                    textSummary: "导入步骤",
                    answerStatus: "complete",
                    knowledgeText: "在数据管理页选择待分析目录。",
                }],
            };
        }),
    });

    const signal = new AbortController().signal;
    const result = await capability.execute({ query: "  如何导入数据  " }, { signal });

    assert.equal(calls[0].query, "如何导入数据");
    assert.equal(calls[0].options.signal, signal);
    assert.equal(calls[0].options.propagateBusy, true);
    assert.deepEqual(result, {
        schemaVersion: "1.0",
        status: "ok",
        query: "如何导入数据",
        knowledgeBase: { id: "mindstudio-insight", version: "26.1.3" },
        sources: [{
            sourceLabel: "用户指南/数据导入",
            projectId: "msinsight",
            documentCategory: "user-guide",
            title: "数据导入",
            section: "导入性能数据",
            contentFormat: "markdown",
            textSummary: "导入步骤",
            answerStatus: "complete",
            knowledgeText: "在数据管理页选择待分析目录。",
        }],
    });
    const serialized = JSON.stringify(result);
    for (const forbidden of ["score", "chunkId", "docId", "packageSha256", "secret-sha"]) {
        assert.equal(serialized.includes(forbidden), false);
    }
});

test("RAG capability validates its query-only input contract", () => {
    const capability = createRagCapability();

    for (const input of [undefined, [], {}, { query: " " }, { query: "ok", topK: 1 }, { query: "x".repeat(8001) }]) {
        assert.throws(() => capability.validate(input), { code: "CAPABILITY_INVALID_ARGUMENT" });
    }
    assert.doesNotThrow(() => capability.validate({ query: "x".repeat(8000) }));
});

test("RAG capability returns stable unavailable and no-match results", async () => {
    const unavailable = createRagCapability({
        ragService: {
            isEnabled: () => false,
            getStatus: () => ({ enabled: false, reason: "unsupported_rag_platform" }),
        },
    });
    const noMatch = createRagCapability({
        ragService: enabledService(async () => ({
            status: "no_match",
            kbId: "mindstudio-insight",
            kbVersion: "26.1.3",
            retrievedChunks: [],
        })),
    });

    assert.deepEqual(await unavailable.execute({ query: "query" }, {}), {
        schemaVersion: "1.0",
        status: "unavailable",
        query: "query",
        sources: [],
        reason: "unsupported_rag_platform",
    });
    assert.deepEqual(await noMatch.execute({ query: "query" }, {}), {
        schemaVersion: "1.0",
        status: "no_match",
        query: "query",
        knowledgeBase: { id: "mindstudio-insight", version: "26.1.3" },
        sources: [],
    });
});

test("RAG capability maps queue saturation to a retryable capability error", async () => {
    const capability = createRagCapability({
        ragService: enabledService(async () => {
            throw Object.assign(new Error("queue full"), { code: "rag_busy" });
        }),
    });

    await assert.rejects(
        capability.execute({ query: "query" }, {}),
        { code: "RAG_BUSY", retryable: true },
    );
});

test("RAG capability stops before retrieval when invocation is cancelled", async () => {
    let calls = 0;
    const controller = new AbortController();
    controller.abort();
    const capability = createRagCapability({
        ragService: enabledService(async () => { calls += 1; }),
    });

    await assert.rejects(capability.execute({ query: "query" }, { signal: controller.signal }), { name: "AbortError" });
    assert.equal(calls, 0);
});

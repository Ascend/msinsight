/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { fixedRagPaths } from "../../../services/rag/runtimePaths.mjs";

test("fixed RAG paths derive only from source or bundled entry location", () => {
    assert.deepEqual(
        fixedRagPaths("C:/Product/resources/profiler/server/insight_web_agent/rag-cli.mjs"),
        {
            ragDataDir: "C:\\Product\\resources\\profiler\\server\\insight_web_agent\\rag-data",
            runtimeDir: "C:\\Product\\resources\\profiler\\server\\insight_web_agent\\rag-runtime",
            modelDir: "C:\\Product\\resources\\profiler\\server\\insight_web_agent\\rag-runtime\\models\\bge-small-zh-v1.5",
        },
    );
    assert.deepEqual(
        fixedRagPaths("C:/source/insight_web_agent/server/rag-cli.mjs"),
        {
            ragDataDir: "C:\\source\\insight_web_agent\\rag-data",
            runtimeDir: "C:\\source\\insight_web_agent\\rag-runtime",
            modelDir: "C:\\source\\insight_web_agent\\rag-runtime\\models\\bge-small-zh-v1.5",
        },
    );
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { join, resolve } from "node:path";
import { test } from "node:test";
import { fixedRagPaths } from "../../../services/rag/runtimePaths.mjs";

test("fixed RAG paths derive only from source or bundled entry location", () => {
    const bundledRoot = resolve("product", "resources", "profiler", "server", "insight_web_agent");
    assert.deepEqual(
        fixedRagPaths(join(bundledRoot, "rag-cli.mjs")),
        {
            ragDataDir: join(bundledRoot, "rag-data"),
            runtimeDir: join(bundledRoot, "rag-runtime"),
            modelDir: join(bundledRoot, "rag-runtime", "models", "bge-small-zh-v1.5"),
            nativeManifestRequired: true,
        },
    );
    const sourceRoot = resolve("source", "insight_web_agent");
    assert.deepEqual(
        fixedRagPaths(join(sourceRoot, "server", "rag-cli.mjs")),
        {
            ragDataDir: join(sourceRoot, "rag-data"),
            runtimeDir: join(sourceRoot, "rag-runtime"),
            modelDir: join(sourceRoot, "rag-runtime", "models", "bge-small-zh-v1.5"),
            nativeManifestRequired: false,
        },
    );
});

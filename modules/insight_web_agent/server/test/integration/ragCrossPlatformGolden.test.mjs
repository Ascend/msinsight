/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { test } from "node:test";
import { createRagService } from "../../services/rag/ragService.mjs";

const packageRoot = fileURLToPath(new URL("../../../", import.meta.url));
const distDir = resolve(process.env.MSINSIGHT_DIST_SERVER_DIR ?? join(packageRoot, "dist-server"));
const baselinePath = fileURLToPath(new URL("../services/rag/fixtures/cross-platform-golden-v1.json", import.meta.url));

test("approved Knowledge Package produces one cross-platform Top-5 golden result", {
    skip: !existsSync(join(distDir, "rag-data", "active.json")),
    timeout: 120_000,
}, async () => {
    const baseline = JSON.parse(await readFile(baselinePath, "utf8"));
    const service = await createRagService({
        config: {
            enabled: true,
            failOpen: false,
            ragDataDir: join(distDir, "rag-data"),
            runtimeDir: join(distDir, "rag-runtime"),
            modelDir: join(distDir, "rag-runtime", "models", "bge-small-zh-v1.5"),
            nativeManifestRequired: true,
        },
        logger: { info() {}, warn() {} },
    });
    const status = service.getStatus();
    assert.equal(status.kbVersion, baseline.kbVersion);
    assert.equal(status.retrievalMode, baseline.retrievalMode);

    const results = [];
    for (const { query } of baseline.results) {
        const result = await service.retrieve(query);
        results.push({
            query,
            hits: result.primaryTop5.map(({ projectId, title, section }) => ({ projectId, title, section })),
        });
    }
    assert.deepEqual(results, baseline.results);
});

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { readFile, writeFile, mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { test } from "node:test";
import { createBackgroundRagService } from "../../../services/rag/backgroundRagService.mjs";
import { createRagCapability } from "../../../capability-center/ragCapability.mjs";
import { ragStatusReason } from "../../../services/rag/ragDiagnostic.mjs";

test("real Worker handler keeps concurrent diagnostics isolated and does not clone callbacks", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "insight-rag-diagnostics-"));
    const productionEntry = new URL("../../../rag-worker.mjs", import.meta.url);
    // Run the production message handler in a real Worker; replace only retrieval dependencies.
    const source = (await readFile(productionEntry, "utf8"))
        .replace('"./services/rag/ragService.mjs"', JSON.stringify(pathToFileURL(join(root, "fixture.mjs")).href))
        .replace('"./services/rag/ragDiagnostic.mjs"', JSON.stringify(new URL("../../../services/rag/ragDiagnostic.mjs", import.meta.url).href));
    await writeFile(join(root, "worker.mjs"), source);
    await writeFile(join(root, "fixture.mjs"), `
export const createRagService = async () => ({
    getStatus: () => ({ enabled: true }), isEnabled: () => true,
    async retrieve(query, options) {
        await new Promise(resolve => setTimeout(resolve, query === 'embedding' ? 30 : 1));
        if (query === 'ok') return { status:'no_match', retrievedChunks:[] };
        options.onDiagnostic({ reason: query === 'embedding' ? 'embedding_unavailable' : 'index_unavailable' });
        if (query === 'hard') throw new Error('token=private-secret');
        return undefined;
    },
});`);
    const logs = [];
    const service = createBackgroundRagService({
        workerUrl: pathToFileURL(join(root, "worker.mjs")),
        logger: { info() {}, warn: (value) => logs.push(value) },
    });
    t.after(async () => { await service.close(); await rm(root, { recursive: true, force: true }); });
    await service.start();
    const capability = createRagCapability({ ragService: service, logger: { warn: (value) => logs.push(value) } });
    const [embedding, index, ok] = await Promise.all(["embedding", "index", "ok"].map((query) => capability.execute({ query }, {})));
    assert.equal(embedding.reason, "embedding_unavailable");
    assert.equal(index.reason, "index_unavailable");
    assert.equal(ok.status, "no_match");
    assert.deepEqual(Object.keys(embedding).sort(), ["query", "reason", "schemaVersion", "sources", "status"]);
    await assert.rejects(capability.execute({ query: "hard" }, {}), (error) =>
        error.code === "RAG_RETRIEVAL_FAILED" && error.details.reason === "index_unavailable");
    const controller = new AbortController();
    let notifications = 0;
    const cancelled = service.retrieve("embedding", { signal: controller.signal, onDiagnostic: () => { notifications++; } });
    controller.abort();
    await assert.rejects(cancelled, { name: "AbortError" });
    await new Promise((resolve) => setTimeout(resolve, 50));
    assert.equal(notifications, 0);
    assert.doesNotMatch(logs.join("\n"), /private-secret/);
    assert.equal(ragStatusReason("secret_lowercase"), "unavailable");
});

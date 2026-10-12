/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { fileURLToPath } from "node:url";
import { createCapabilityCenter } from "./capability-center/service.mjs";
import { createRagService } from "./services/rag/ragService.mjs";
import { fixedRagPaths } from "./services/rag/runtimePaths.mjs";

const QUERY = "MindStudio Insight 内存分析如何定位异常分配";
const CREDENTIAL_MARKER = "required-smoke-credential-marker";
const originalConsole = Object.fromEntries(
    ["log", "info", "warn", "error", "debug"].map((name) => [name, console[name].bind(console)]),
);
const capturedLogs = [];

for (const name of Object.keys(originalConsole)) {
    console[name] = (...values) => capturedLogs.push(values.map(safeLogValue).join(" "));
}

try {
    const paths = fixedRagPaths(fileURLToPath(import.meta.url));
    const ragService = await createRagService({
        config: {
            enabled: true,
            failOpen: false,
            debug: true,
            ...paths,
        },
    });
    assert.equal(ragService.isEnabled(), true, "required smoke must not fail open");

    const capabilityCenter = createCapabilityCenter({
        ragService,
        frontendCommandService: { request() { throw new Error("frontend command is not used by RAG smoke"); } },
    });
    assert.equal(capabilityCenter.list().some(({ name }) => name === "rag_retrieve"), true);
    const result = await capabilityCenter.invoke({ name: "rag_retrieve", input: { query: QUERY } });
    assert.equal(result.schemaVersion, "1.0");
    assert.equal(result.status, "ok");
    assert.ok(result.sources.length > 0);
    assert.ok(result.sources.every(({ sourceLabel, knowledgeText }) => sourceLabel && knowledgeText));

    const forbidden = [
        QUERY,
        CREDENTIAL_MARKER,
        paths.ragDataDir,
        paths.runtimeDir,
        paths.modelDir,
        ...result.sources.map(({ knowledgeText }) => knowledgeText),
    ].filter(Boolean);
    const logs = capturedLogs.join("\n");
    for (const value of forbidden) assert.equal(logs.includes(value), false, "sensitive smoke value leaked to logs");

    originalConsole.log(JSON.stringify({
        status: "passed",
        failOpen: false,
        capability: "rag_retrieve",
        hits: result.sources.length,
        sensitiveLogScan: "passed",
    }));
} catch (error) {
    originalConsole.error(JSON.stringify({ error: { code: error?.code ?? "required_rag_smoke_failed" } }));
    process.exitCode = 1;
}

function safeLogValue(value) {
    if (value instanceof Error) return String(value.code ?? value.name ?? "Error");
    if (typeof value === "object" && value !== null) return JSON.stringify(value);
    return String(value);
}

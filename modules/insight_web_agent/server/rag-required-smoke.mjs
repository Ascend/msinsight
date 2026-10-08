/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { createCapabilityCenter } from "./capability-center/service.mjs";
import { createRagService } from "./services/rag/ragService.mjs";
import { fixedRagPaths } from "./services/rag/runtimePaths.mjs";
import { formatSmokeError, smokeFailureStage, smokeProcessSnapshot } from "./services/rag/smokeDiagnostics.mjs";

const QUERY = "MindStudio Insight 内存分析如何定位异常分配";
const CREDENTIAL_MARKER = "required-smoke-credential-marker";
const originalConsole = Object.fromEntries(
    ["log", "info", "warn", "error", "debug"].map((name) => [name, console[name].bind(console)]),
);
const capturedLogs = [];
const entryPath = fileURLToPath(import.meta.url);
const paths = fixedRagPaths(entryPath);
const sensitiveValues = [QUERY, CREDENTIAL_MARKER, paths.ragDataDir, paths.runtimeDir, paths.modelDir];
const diagnosticRedactions = () => [...sensitiveValues, dirname(entryPath), process.cwd()];
let stage = "rag_service_initialization";

for (const name of Object.keys(originalConsole)) {
    console[name] = (...values) => capturedLogs.push(values.map(safeLogValue).join(" "));
}

try {
    const ragService = await createRagService({
        nativeLoadObserver: (stage) => originalConsole.error(JSON.stringify({
            event: "rag_smoke_process", stage, process: smokeProcessSnapshot(diagnosticRedactions()),
        })),
        config: {
            enabled: true,
            failOpen: false,
            debug: true,
            ...paths,
        },
    });
    assert.equal(ragService.isEnabled(), true, "required smoke must not fail open");

    stage = "capability_registration";
    const capabilityCenter = createCapabilityCenter({
        ragService,
        frontendCommandService: { request() { throw new Error("frontend command is not used by RAG smoke"); } },
    });
    assert.equal(capabilityCenter.list().some(({ name }) => name === "rag_retrieve"), true);
    stage = "rag_retrieve";
    const result = await capabilityCenter.invoke({ name: "rag_retrieve", input: { query: QUERY } });
    if (Array.isArray(result?.sources)) sensitiveValues.push(...result.sources.map((source) => source?.knowledgeText));
    stage = "result_validation";
    assert.equal(result.schemaVersion, "1.0");
    assert.equal(result.status, "ok");
    assert.ok(result.sources.length > 0);
    assert.ok(result.sources.every(({ sourceLabel, knowledgeText }) => sourceLabel && knowledgeText));

    stage = "sensitive_log_scan";
    const forbidden = sensitiveValues.filter(Boolean);
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
    originalConsole.error(JSON.stringify({
        event: "rag_smoke_failure",
        stage: smokeFailureStage(error, stage),
        error: { code: "required_rag_smoke_failed", ...formatSmokeError(error, diagnosticRedactions()) },
        process: smokeProcessSnapshot(diagnosticRedactions()),
    }));
    process.exitCode = 1;
}

function safeLogValue(value) {
    if (value instanceof Error) return String(value.code ?? value.name ?? "Error");
    if (typeof value === "object" && value !== null) return JSON.stringify(value);
    return String(value);
}

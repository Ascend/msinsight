/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { cpus, release, version } from "node:os";
import { dirname, join } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";
import { formatSmokeError, redactSmokeText, sanitizeSmokeDiagnostic } from "../server/services/rag/smokeDiagnostics.mjs";
import { inspectNativePe } from "./inspect-native-pe.mjs";
import { nativeDiagnosis } from "./summarize-native-diagnosis.mjs";

const scriptsDir = dirname(fileURLToPath(import.meta.url));
const TOTAL_BUDGET_MS = 60_000;
const PROBE_TIMEOUT_MS = 20_000;

export const hasNativeLoadFailure = (stderr) => String(stderr ?? "").split(/\r?\n/).some((line) => {
    try {
        const value = JSON.parse(line);
        return value?.event === "rag_smoke_failure" && value.stage === "onnx_import"
            && value.error?.code === "native_runtime_load_failed";
    } catch {
        return false;
    }
});

export const runNativeFailureDiagnostics = ({
    packageRoot, distDir, emit, primaryFailure,
    platform = process.platform,
    spawn = spawnSync,
    now = () => performance.now(),
}) => {
    const deadline = now() + TOTAL_BUDGET_MS;
    const evidence = [];
    const publish = (event) => { evidence.push(event); emit(event); };
    const sensitiveValues = [join(distDir, "rag-runtime"), join(distDir, "rag-data"), distDir, packageRoot, dirname(packageRoot)];
    emit({
        event: "rag_native_probes_start", budgetMs: TOTAL_BUDGET_MS, execPath: process.execPath,
        osRelease: release(), osVersion: version(), cpuModels: [...new Set(cpus().map(({ model }) => model))].slice(0, 8),
    });
    const probes = [
        { name: "node_source", command: process.execPath, args: [join(scriptsDir, "probe-rag-native.mjs"), "source", packageRoot], cwd: packageRoot },
        { name: "node_bundle", command: process.execPath, args: [join(scriptsDir, "probe-rag-native.mjs"), "bundle", distDir], cwd: distDir },
    ];
    if (platform === "win32") {
        probes.push({
            name: "windows_dll",
            command: join(process.env.SystemRoot ?? "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe"),
            args: windowsProbeArguments(),
            input: () => windowsPeRequest(distDir, sensitiveValues),
            cwd: distDir,
            env: {
                ...process.env,
                MSINSIGHT_RAG_PROBE_DLL: join(distDir, "node_modules", "onnxruntime-node", "bin", "napi-v6", "win32", "x64", "onnxruntime.dll"),
                MSINSIGHT_RAG_PROBE_NODE_DIR: dirname(process.execPath),
                MSINSIGHT_RAG_PROBE_SCRIPT: join(scriptsDir, "probe-rag-native-windows.ps1"),
            },
        });
    }
    for (const probe of probes) {
        const started = now();
        const remaining = deadline - started;
        if (remaining <= 0) {
            publish({ event: "rag_native_probe", probe: probe.name, outcome: "not_run", reason: "budget_exhausted" });
            continue;
        }
        try {
            const args = typeof probe.args === "function" ? probe.args() : probe.args;
            const input = probe.input?.();
            const timeout = Math.min(PROBE_TIMEOUT_MS, Math.floor(deadline - now()));
            if (timeout <= 0) {
                publish({ event: "rag_native_probe", probe: probe.name, outcome: "not_run", reason: "budget_exhausted" });
                continue;
            }
            const result = spawn(probe.command, args, {
                cwd: probe.cwd, env: probe.env ?? process.env,
                encoding: "utf8", windowsHide: true, timeout, maxBuffer: 256 * 1024,
                ...(input ? { input } : {}),
            });
            publish({
                event: "rag_native_probe", probe: probe.name,
                outcome: result.error ? "probe_error" : result.status === 0 ? "completed" : "probe_failed",
                durationMs: Math.round(now() - started), timeoutMs: timeout,
                status: result.status, signal: result.signal,
                ...(result.error ? { error: formatSmokeError(result.error, sensitiveValues) } : {}),
                records: probeRecords(result.stdout, sensitiveValues),
                ...(result.stderr ? { stderr: redactSmokeText(result.stderr, sensitiveValues, 4096) } : {}),
            });
        } catch (error) {
            publish({ event: "rag_native_probe", probe: probe.name, outcome: "probe_error", error: formatSmokeError(error, sensitiveValues) });
        }
    }
    emit(nativeDiagnosis(evidence, primaryFailure));
    emit({ event: "rag_native_probes_end" });
};

export const windowsProbeArguments = () => [
    "-NoProfile", "-NonInteractive", "-Command",
    "& ([scriptblock]::Create([IO.File]::ReadAllText($env:MSINSIGHT_RAG_PROBE_SCRIPT)))",
];

function windowsPeRequest(distDir, sensitiveValues) {
    const directory = join(distDir, "node_modules", "onnxruntime-node", "bin", "napi-v6", "win32", "x64");
    return JSON.stringify({ files: ["onnxruntime_binding.node", "onnxruntime.dll"].map((path) => {
        try { return { path, ...inspectNativePe(readFileSync(join(directory, path))) }; }
        catch (error) { return { path, error: formatSmokeError(error, sensitiveValues) }; }
    }) });
}

function probeRecords(stdout, sensitiveValues) {
    const lines = String(stdout ?? "").trim().split(/\r?\n/).filter(Boolean);
    const selected = lines.length <= 64 ? lines : [...lines.slice(0, 40), ...lines.slice(-24)];
    const records = selected.map((line) => {
        try {
            return sanitizeSmokeDiagnostic(JSON.parse(line), sensitiveValues);
        } catch {
            return { text: redactSmokeText(line, sensitiveValues, 16_384) };
        }
    });
    if (lines.length > 64) records.push({ event: "rag_native_output_truncated", omittedRecords: lines.length - 64 });
    return records;
}

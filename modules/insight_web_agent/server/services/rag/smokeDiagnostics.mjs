/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { lstatSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";
import { basename, join } from "node:path";
import { pathToFileURL } from "node:url";

export const redactSmokeText = (value, sensitiveValues = [], limit = 2048) => {
    let text = String(value ?? "");
    const replacements = new Set();
    for (const value of sensitiveValues.filter((value) => typeof value === "string" && value)) {
        replacements.add(value);
        replacements.add(value.replaceAll("\\", "/"));
        replacements.add(value.replaceAll("/", "\\"));
        if (/^(?:[a-z]:[\\/]|\/)/i.test(value)) replacements.add(pathToFileURL(value).href);
    }
    for (const value of [...replacements]) replacements.add(JSON.stringify(value).slice(1, -1));
    for (const value of [...replacements].sort((left, right) => right.length - left.length)) {
        text = text.split(value).join("<redacted>");
    }
    text = text
        .replace(/\b(Bearer\s+)[^\s"']+/gi, "$1<redacted>")
        .replace(/\b((?:api[_-]?key|token|password|credential)\s*[=:]\s*)[^\s,"']+/gi, "$1<redacted>");
    return text.length > limit ? `${text.slice(0, limit)}...[truncated]` : text;
};

export const formatSmokeError = (error, sensitiveValues = [], seen = new Set()) => {
    if (seen.has(error) || seen.size >= 4) return { message: "[cause chain truncated]" };
    seen.add(error);
    const clean = (value, limit) => redactSmokeText(value, sensitiveValues, limit);
    const result = {
        name: clean(error?.name ?? "Error", 128),
        ...(error?.code !== undefined ? { code: clean(error.code, 128) } : {}),
        message: clean(error?.message ?? error),
    };
    // Redact complete values before truncating multiline exception messages.
    if (typeof error?.stack === "string") result.stack = clean(error.stack, 4096).split("\n").slice(0, 9).join("\n");
    if (error?.cause !== undefined) result.cause = formatSmokeError(error.cause, sensitiveValues, seen);
    return result;
};

export const sanitizeSmokeDiagnostic = (value, sensitiveValues = [], depth = 0) => {
    if (depth > 12) return "[truncated]";
    if (typeof value === "string") return redactSmokeText(value, sensitiveValues, 4096);
    if (Array.isArray(value)) return value.slice(0, 256).map((item) => sanitizeSmokeDiagnostic(item, sensitiveValues, depth + 1));
    if (value && typeof value === "object") return Object.fromEntries(Object.entries(value).slice(0, 64)
        .map(([key, item]) => [redactSmokeText(key, sensitiveValues, 128), sanitizeSmokeDiagnostic(item, sensitiveValues, depth + 1)]));
    return value;
};

export const smokeFailureStage = (error, fallback) => ({
    native_runtime_load_failed: "onnx_import",
    onnx_initialization_failed: "onnx_initialization",
}[error?.code] ?? fallback);

export const smokeProcessSnapshot = (sensitiveValues = [], report = () => process.report.getReport()) => {
    const clean = (value) => redactSmokeText(value, sensitiveValues);
    try {
        const modules = report().sharedObjects ?? [];
        return {
            pid: process.pid,
            node: process.version,
            arch: process.arch,
            execPath: clean(process.execPath),
            cwd: clean(process.cwd()),
            execArgvFlags: process.execArgv.filter((argument) => argument.startsWith("-")).map((argument) => argument.split("=")[0]),
            nodeOptionsFlags: [...new Set((process.env.NODE_OPTIONS ?? "").match(/--[a-z][a-z0-9-]*(?==|\s|$)/gi) ?? [])],
            memory: process.memoryUsage(),
            loadedModules: modules.slice(0, 128).map((path) => ({ name: clean(basename(path)), path: clean(path) })),
            modulesTruncated: modules.length > 128,
        };
    } catch (error) {
        return { error: formatSmokeError(error, sensitiveValues) };
    }
};

export const smokeEnvironment = (bundleRoot, nativeFiles = []) => {
    const resolver = createRequire(join(bundleRoot, "rag-required-smoke.mjs"));
    let onnx;
    try {
        onnx = {
            entry: resolver.resolve("onnxruntime-node"),
            version: JSON.parse(readFileSync(resolver.resolve("onnxruntime-node/package.json"), "utf8")).version,
        };
    } catch (error) {
        onnx = { resolutionError: formatSmokeError(error, [bundleRoot, process.cwd()]) };
    }
    return {
        event: "rag_smoke_start",
        node: process.version,
        platform: process.platform,
        arch: process.arch,
        napi: process.versions.napi,
        execPath: process.execPath,
        bundleRoot,
        onnx,
        nativeFiles: nativeFiles.map((path) => {
            try {
                const info = lstatSync(join(bundleRoot, path));
                return { path, exists: true, regularFile: info.isFile(), sizeBytes: info.size };
            } catch (error) {
                return { path, exists: false, code: error.code };
            }
        }),
    };
};

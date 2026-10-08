/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { createHash } from "node:crypto";
import { lstatSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";
import { basename, dirname, join, relative, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { resolveRagTarget } from "../server/services/rag/platformSupport.mjs";
import { formatSmokeError } from "../server/services/rag/smokeDiagnostics.mjs";
import { inspectNativePe, peLogSummary } from "./inspect-native-pe.mjs";

const [scope, rootArgument] = process.argv.slice(2);
const root = resolve(rootArgument ?? ".");
const sensitiveValues = [join(root, "rag-runtime"), join(root, "rag-data"), root, dirname(root)];
const emit = (value) => process.stdout.write(`${JSON.stringify(value)}\n`);
let phase = "module_resolution";

try {
    if (!["source", "bundle"].includes(scope) || !rootArgument) throw new Error("Native probe requires a source or bundle root");
    const resolver = createRequire(join(root, "package.json"));
    const packageDir = scope === "source"
        ? dirname(resolver.resolve("onnxruntime-node/package.json"))
        : join(root, "node_modules", "onnxruntime-node");
    const entry = scope === "source" ? resolver.resolve("onnxruntime-node") : join(packageDir, "dist", "index.js");
    const version = JSON.parse(readFileSync(join(packageDir, "package.json"), "utf8")).version;
    phase = "native_file_inventory";
    emit({
        event: "rag_native_files",
        scope,
        node: process.version,
        arch: process.arch,
        entry: relative(root, entry),
        version,
        files: resolveRagTarget().onnx.files.map((path) => {
            try {
                const file = join(packageDir, path);
                const info = lstatSync(file);
                if (!info.isFile()) throw new Error("Native probe input is not a regular file");
                const bytes = readFileSync(file);
                let pe;
                if (process.platform === "win32") {
                    try { pe = peLogSummary(inspectNativePe(bytes)); }
                    catch (error) { pe = { error: formatSmokeError(error, sensitiveValues) }; }
                }
                return { path, sizeBytes: info.size, sha256: createHash("sha256").update(bytes).digest("hex"), ...(pe ? { pe } : {}) };
            } catch (error) {
                return { path, error: formatSmokeError(error, sensitiveValues) };
            }
        }),
    });
    phase = "onnx_import";
    await import(pathToFileURL(entry).href);
    emit({ event: "rag_native_import", scope, outcome: "passed", loadedModules: loadedModules() });
} catch (error) {
    emit({ event: "rag_native_import", scope, phase, outcome: "failed", error: formatSmokeError(error, sensitiveValues), loadedModules: loadedModules() });
    process.exitCode = 1;
}

function loadedModules() {
    try {
        return process.report.getReport().sharedObjects
            .filter((path) => /onnxruntime|vcruntime140|msvcp140|ucrtbase/i.test(basename(path)))
            .map((path) => ({ name: basename(path), path }));
    } catch (error) {
        return { error: formatSmokeError(error, sensitiveValues) };
    }
}

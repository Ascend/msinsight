/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { build } from "esbuild";
import { randomUUID } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { copyFile, cp, mkdir, rename, rm } from "node:fs/promises";
import { createRequire } from "node:module";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createKnowledgePackageService, validatePackageArchive } from "../server/services/rag/knowledgePackageService.mjs";
import { loadEmbeddingModelContract } from "../server/services/rag/embeddingRuntime.mjs";
import { loadRuntimeContract } from "../server/services/rag/runtimeContract.mjs";

const rootDir = dirname(fileURLToPath(new URL("../package.json", import.meta.url)));
const require = createRequire(import.meta.url);
const outputDir = join(rootDir, "dist-server");
const externalRagModules = ["onnxruntime-node", "@huggingface/tokenizers", "@node-rs/jieba", "@node-rs/jieba/dict.js"];
const esmRequireBanner = 'import { createRequire as __createRequire } from "node:module"; const require = __createRequire(import.meta.url);';
const ragEnvironment = Object.freeze({
    pack: "MSINSIGHT_RAG_PACKAGE",
    sidecar: "MSINSIGHT_RAG_PACKAGE_SHA256",
    modelDir: "MSINSIGHT_RAG_MODEL_DIR",
});

const options = parseBuildOptions(process.argv.slice(2));
const preflight = options.mode === "development" ? await preflightDevelopment(options) : null;
const staging = join(rootDir, `.dist-server.${process.pid}.${randomUUID()}.staging`);

try {
    await assembleBundle(staging, options, preflight);
    await publishBundle(staging);
} finally {
    await rm(staging, { recursive: true, force: true }).catch(() => {});
}

console.log(`Server bundle written to ${join(outputDir, "index.mjs")}`);
console.log(`Native agent bundle written to ${join(outputDir, "native-agent", "index.mjs")}`);
console.log(`RAG CLI bundle written to ${join(outputDir, "rag-cli.mjs")}`);
console.log(`RAG CLI wrapper copied to ${join(outputDir, "mindstudio-insight-rag.cmd")}`);
console.log(`Capability center config copied to ${join(outputDir, "capability-center.json")}`);
console.log(`Docs copied to ${join(outputDir, "docs")}`);
console.log(`Skills copied to ${join(outputDir, "skills")}`);

async function preflightDevelopment({ ragDevPack, ragDevSidecar, ragModelDir }) {
    const model = await loadEmbeddingModelContract({ modelDir: ragModelDir });
    const runtime = await loadRuntimeContract(join(rootDir, "rag-runtime"));
    const validated = await validatePackageArchive({
        archivePath: ragDevPack,
        sidecarPath: ragDevSidecar,
        modelContract: model.manifest,
        runtimeContract: runtime.contract,
    });
    return { model, runtime, validated };
}

async function assembleBundle(target, buildOptions, preflightResult) {
    await mkdir(target, { recursive: false });
    await buildEntries(target);
    await copyStaticRuntime(target);
    if (process.platform === "win32" && process.arch === "x64") {
        await copyWindowsRagDependencies(target);
    }
    if (buildOptions.mode === "development") {
        await assembleDevelopmentRag(target, buildOptions, preflightResult);
    }
    if (existsSync(join(target, "rag-seed")) || existsSync(join(target, "rag-build-mode.json"))) {
        throw new Error("Server bundle contains a forbidden seed or placeholder RAG metadata");
    }
}

async function buildEntries(target) {
    const entries = [
        [join(rootDir, "server", "index.mjs"), join(target, "index.mjs")],
        [join(rootDir, "server", "rag-cli.mjs"), join(target, "rag-cli.mjs")],
        [join(rootDir, "server", "rag-required-smoke.mjs"), join(target, "rag-required-smoke.mjs")],
        [join(rootDir, "server", "native-agent", "index.mjs"), join(target, "native-agent", "index.mjs")],
    ];
    for (const [entry, outfile] of entries) {
        await mkdir(dirname(outfile), { recursive: true });
        await build({
            banner: { js: esmRequireBanner },
            bundle: true,
            entryPoints: [entry],
            external: entry.includes("native-agent") ? ["node:*"] : ["node:*", ...externalRagModules],
            format: "esm",
            platform: "node",
            outfile,
            sourcemap: false,
            target: "node22.14",
        });
    }
}

async function copyStaticRuntime(target) {
    for (const name of ["agent-servers.json", "acp-session-conf.json", "msinsight-native.json", "capability-center.json"]) {
        await copyFile(join(rootDir, name), join(target, name));
    }
    for (const name of ["prompts", "agents"]) {
        await cp(join(rootDir, name), join(target, name), { recursive: true });
    }
    await cp(join(rootDir, "..", "..", "docs"), join(target, "docs"), { recursive: true });
    await cp(join(rootDir, "..", "..", "skills"), join(target, "skills"), { recursive: true });
    await cp(join(rootDir, "rag-runtime"), join(target, "rag-runtime"), { recursive: true });
    await copyFile(
        join(rootDir, "scripts", "mindstudio-insight-rag.cmd"),
        join(target, "mindstudio-insight-rag.cmd"),
    );
}

async function assembleDevelopmentRag(target, options, preflightResult) {
    const modelOutput = join(target, "rag-runtime", "models", "bge-small-zh-v1.5");
    await copyReviewedModel(preflightResult.model, modelOutput);
    const service = createKnowledgePackageService({
        ragDataDir: join(target, "rag-data"),
        modelDir: modelOutput,
        runtimeDir: join(target, "rag-runtime"),
    });
    const imported = await service.importPackage(options.ragDevPack, {
        mode: "development",
        sidecarPath: options.ragDevSidecar,
    });
    await service.activate(imported.version, { sha256: imported.sha256 });
    const verified = await service.verify();
    if (verified.installMode !== "development-local") {
        throw new Error("Preactivated RAG install is not development-local");
    }
}

async function copyReviewedModel({ modelDir, manifest }, target) {
    const files = ["model-manifest.json", ...Object.keys(manifest.fileDigests)];
    for (const relativeFile of files) {
        const destination = join(target, relativeFile);
        await mkdir(dirname(destination), { recursive: true });
        await copyFile(join(modelDir, relativeFile), destination);
    }
}

async function publishBundle(stagingDirectory) {
    const backup = join(rootDir, `.dist-server.${process.pid}.${randomUUID()}.backup`);
    const hadOutput = existsSync(outputDir);
    if (hadOutput) await rename(outputDir, backup);
    try {
        await rename(stagingDirectory, outputDir);
    } catch (error) {
        if (hadOutput && existsSync(backup) && !existsSync(outputDir)) await rename(backup, outputDir);
        throw error;
    }
    if (hadOutput) await rm(backup, { recursive: true, force: true });
}

function parseBuildOptions(args, env = process.env) {
    const legacyValues = parseLegacyBuildValues(args);
    const environmentConfigured = Object.values(ragEnvironment).some((name) => Object.hasOwn(env, name));
    if (environmentConfigured) {
        if (legacyValues.provided) {
            console.warn("MSINSIGHT_RAG_* environment variables override deprecated RAG command-line options.");
        }
        return developmentBuildOptions({
            pack: env[ragEnvironment.pack],
            sidecar: env[ragEnvironment.sidecar],
            modelDir: env[ragEnvironment.modelDir],
        }, `${Object.values(ragEnvironment).join(", ")} are all required when any RAG environment variable is set`);
    }
    if (!legacyValues.provided) return { mode: "code-only" };
    console.warn("RAG command-line options are deprecated; use MSINSIGHT_RAG_* environment variables.");
    return developmentBuildOptions({
        pack: legacyValues.values["--rag-dev-pack"],
        sidecar: legacyValues.values["--rag-dev-sidecar"],
        modelDir: legacyValues.values["--rag-model-dir"],
    }, "--rag-dev-pack, --rag-dev-sidecar, and --rag-model-dir are all required");
}

function parseLegacyBuildValues(args) {
    const allowed = new Set(["--rag-dev-pack", "--rag-dev-sidecar", "--rag-model-dir"]);
    const values = {};
    let provided = false;
    for (let index = 0; index < args.length; index += 1) {
        const argument = args[index];
        if (argument === "--") continue;
        provided = true;
        const [flag, inline] = argument.split("=", 2);
        if (!allowed.has(flag) || values[flag] !== undefined) {
            throw new Error(`Unknown or duplicate server build option: ${flag}`);
        }
        const value = inline ?? args[++index];
        if (!value || value.startsWith("--")) throw new Error(`${flag} requires a path`);
        values[flag] = resolve(value);
    }
    return { provided, values };
}

function developmentBuildOptions(values, missingMessage) {
    const normalized = Object.fromEntries(Object.entries(values).map(([name, value]) => [name, String(value ?? "").trim()]));
    if (Object.values(normalized).some((value) => !value)) throw new Error(missingMessage);
    return {
        mode: "development",
        ragDevPack: resolve(normalized.pack),
        ragDevSidecar: resolve(normalized.sidecar),
        ragModelDir: resolve(normalized.modelDir),
    };
}

async function copyWindowsRagDependencies(target) {
    const onnxRoot = packageDirectory("onnxruntime-node");
    const onnxFiles = [
        "package.json",
        "dist/index.js",
        "dist/backend.js",
        "dist/binding.js",
        "dist/version.js",
        "bin/napi-v6/win32/x64/onnxruntime_binding.node",
        "bin/napi-v6/win32/x64/onnxruntime.dll",
    ];
    for (const file of onnxFiles) {
        await copyFileTree(
            join(onnxRoot, file),
            join(target, "node_modules", "onnxruntime-node", file),
        );
    }
    for (const dependency of ["onnxruntime-common", "@huggingface/tokenizers", "@node-rs/jieba"]) {
        await cp(packageDirectory(dependency), join(target, "node_modules", dependency), {
            recursive: true,
            dereference: true,
        });
    }
    const jiebaRequire = createRequire(require.resolve("@node-rs/jieba"));
    await cp(
        packageDirectory("@node-rs/jieba-win32-x64-msvc", jiebaRequire),
        join(target, "node_modules", "@node-rs/jieba-win32-x64-msvc"),
        { recursive: true, dereference: true },
    );
}

function packageDirectory(name, resolver = require) {
    let directory = dirname(resolver.resolve(name));
    while (true) {
        const manifestPath = join(directory, "package.json");
        if (existsSync(manifestPath)) {
            const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
            if (manifest.name === name) return directory;
        }
        const parent = dirname(directory);
        if (parent === directory) throw new Error(`Unable to locate installed package root: ${name}`);
        directory = parent;
    }
}

async function copyFileTree(source, destination) {
    await mkdir(dirname(destination), { recursive: true });
    await copyFile(source, destination);
}

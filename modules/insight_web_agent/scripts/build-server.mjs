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
import { copyFile, cp, mkdir, rename, rm, writeFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createKnowledgePackageService, validatePackageArchive } from "../server/services/rag/knowledgePackageService.mjs";
import { stageBundleForWorkspace } from "../server/services/rag/bundleWorkspace.mjs";
import { loadEmbeddingModelContract } from "../server/services/rag/embeddingRuntime.mjs";
import { writeNativeRuntimeManifest } from "../server/services/rag/nativeRuntimeManifest.mjs";
import { resolveRagTarget } from "../server/services/rag/platformSupport.mjs";
import { loadRuntimeContract } from "../server/services/rag/runtimeContract.mjs";

const rootDir = dirname(fileURLToPath(new URL("../package.json", import.meta.url)));
const require = createRequire(import.meta.url);
const outputDir = join(rootDir, "dist-server");
const externalRagModules = ["onnxruntime-node", "@huggingface/tokenizers", "@node-rs/jieba", "@node-rs/jieba/dict.js"];
const esmRequireBanner = 'import { createRequire as __createRequire } from "node:module"; const require = __createRequire(import.meta.url);';
const ragEnvironment = Object.freeze({
    mode: "MSINSIGHT_RAG_MODE",
    pack: "MSINSIGHT_RAG_PACKAGE",
    sidecar: "MSINSIGHT_RAG_PACKAGE_SHA256",
    modelDir: "MSINSIGHT_RAG_MODEL_DIR",
});
const targetEnvironment = Object.freeze({
    arch: "MSINSIGHT_RAG_TARGET_ARCH",
    libc: "MSINSIGHT_RAG_TARGET_LIBC",
    platform: "MSINSIGHT_RAG_TARGET_PLATFORM",
});

const options = parseBuildOptions(process.argv.slice(2));
const preflight = options.mode === "code-only" ? null : await preflightBundledRag(options);
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
console.log(`Skills copied to ${join(outputDir, "skills")}`);

async function preflightBundledRag({ ragPack, ragSidecar, ragModelDir }) {
    const model = await loadEmbeddingModelContract({ modelDir: ragModelDir });
    const runtime = await loadRuntimeContract(join(rootDir, "rag-runtime"));
    const validated = await validatePackageArchive({
        archivePath: ragPack,
        sidecarPath: ragSidecar,
        modelContract: model.manifest,
        runtimeContract: runtime.contract,
    });
    return { model, runtime, validated };
}

async function assembleBundle(target, buildOptions, preflightResult) {
    await mkdir(target, { recursive: false });
    await buildEntries(target);
    await copyStaticRuntime(target);
    await copyPlatformRagDependencies(target, buildOptions.target);
    if (buildOptions.mode !== "code-only") {
        await assembleBundledRag(target, buildOptions, preflightResult);
        await mergeBundleSkillsIntoProductSkills(target, preflightResult);
        await writeNativeRuntimeManifest({
            bundleRoot: target,
            runtimeDir: join(target, "rag-runtime"),
            target: buildOptions.target,
            modelManifestSha256: preflightResult.model.manifest.manifestSha256,
            runtimeContractSha256: preflightResult.runtime.contract.contractSha256,
            nodeTarget: "22.14",
        });
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
    await cp(join(rootDir, "..", "..", "skills"), join(target, "skills"), { recursive: true });
    await cp(join(rootDir, "rag-runtime"), join(target, "rag-runtime"), { recursive: true });
    await copyFile(
        join(rootDir, "scripts", "mindstudio-insight-rag.cmd"),
        join(target, "mindstudio-insight-rag.cmd"),
    );
}

async function assembleBundledRag(target, options, preflightResult) {
    const modelOutput = join(target, "rag-runtime", "models", "bge-small-zh-v1.5");
    await copyReviewedModel(preflightResult.model, modelOutput);
    const service = createKnowledgePackageService({
        ragDataDir: join(target, "rag-data"),
        modelDir: modelOutput,
        runtimeDir: join(target, "rag-runtime"),
    });
    const imported = await service.importPackage(options.ragPack, {
        mode: options.mode,
        sidecarPath: options.ragSidecar,
    });
    await service.activate(imported.version, { sha256: imported.sha256 });
    const verified = await service.verify();
    const expectedInstallMode = options.mode === "development" ? "development-local" : "product-bundled";
    if (verified.installMode !== expectedInstallMode) {
        throw new Error("Preactivated RAG install mode is invalid");
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

/**
 * Materialize reviewed Bundle skills into the product skills directory.
 *
 * The install tree keeps exactly one copy per skill name; Bundle records win
 * over static product skills on collision so the shipped set always matches
 * the gated Package. Overridden names are reported as build evidence.
 */
async function mergeBundleSkillsIntoProductSkills(target, preflightResult) {
    const { pack, members } = preflightResult.validated;
    const payload = {};
    for (const [name, data] of members) {
        if (name.startsWith("bundles/")) payload[name] = data;
    }
    const staged = await stageBundleForWorkspace({
        skills: pack.bundle.skills,
        mcps: pack.bundle.mcps,
        files: pack.bundle.files,
        skips: pack.bundle.skips,
        payload,
    });
    const productSkills = join(target, "skills");
    const overridden = [];
    for (const [name, tree] of staged.skillFiles) {
        const destination = join(productSkills, name);
        if (existsSync(destination)) overridden.push(name);
        await rm(destination, { recursive: true, force: true });
        for (const [relativePath, file] of tree) {
            const filePath = join(destination, ...relativePath.split("/"));
            await mkdir(dirname(filePath), { recursive: true });
            await writeFile(filePath, file.data);
        }
    }
    console.log(`Bundle skills merged into product skills: installed=${staged.skillFiles.size} overridden=[${[...overridden].sort().join(", ")}]`);
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
    const target = resolveRagTarget({
        platform: env[targetEnvironment.platform] ?? process.platform,
        arch: env[targetEnvironment.arch] ?? process.arch,
        libc: env[targetEnvironment.libc],
    });
    // The server build accepts no command-line options; RAG inputs come from MSINSIGHT_RAG_* only.
    // A lone `--` separator is a conventional no-op (e.g. `npm run server:build --`) and is ignored.
    const effectiveArgs = args.length === 1 && args[0] === "--" ? [] : args;
    if (effectiveArgs.length > 0) {
        throw new Error(`Unknown server build option: ${effectiveArgs[0]} (the server build accepts no command-line options)`);
    }
    const environmentConfigured = Object.values(ragEnvironment).some((name) => Object.hasOwn(env, name));
    if (environmentConfigured) {
        return { ...bundledBuildOptions(env[ragEnvironment.mode] || "development", {
            pack: env[ragEnvironment.pack],
            sidecar: env[ragEnvironment.sidecar],
            modelDir: env[ragEnvironment.modelDir],
        }, `${ragEnvironment.pack}, ${ragEnvironment.sidecar}, and ${ragEnvironment.modelDir} are required for a bundled RAG build`), target };
    }
    return { mode: "code-only", target };
}

function bundledBuildOptions(mode, values, missingMessage) {
    if (!["development", "product-bundled"].includes(mode)) throw new Error(`Unsupported RAG build mode: ${mode}`);
    const normalized = Object.fromEntries(Object.entries(values).map(([name, value]) => [name, String(value ?? "").trim()]));
    if (Object.values(normalized).some((value) => !value)) throw new Error(missingMessage);
    return {
        mode,
        ragPack: resolve(normalized.pack),
        ragSidecar: resolve(normalized.sidecar),
        ragModelDir: resolve(normalized.modelDir),
    };
}

async function copyPlatformRagDependencies(target, ragTarget) {
    const onnxRoot = packageDirectory("onnxruntime-node");
    const onnxFiles = [
        "package.json",
        "dist/index.js",
        "dist/backend.js",
        "dist/binding.js",
        "dist/version.js",
        ...ragTarget.onnx.files,
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
        packageDirectory(ragTarget.jieba.packageName, jiebaRequire),
        join(target, "node_modules", ragTarget.jieba.packageName),
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

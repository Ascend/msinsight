/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createKnowledgePackageService } from "./services/rag/knowledgePackageService.mjs";
import { fixedRagPaths } from "./services/rag/runtimePaths.mjs";

const USAGE = [
    "Usage:",
    "  mindstudio-insight-rag import --mode development --pack <knowledge-pack-v4.zip> [--sidecar <knowledge-pack-v4.zip.sha256>]",
    "  mindstudio-insight-rag activate --version <YY.release.revision> --sha256 <digest>",
    "  mindstudio-insight-rag verify",
    "  mindstudio-insight-rag rollback",
    "  mindstudio-insight-rag status",
].join("\n");
const entryPath = fileURLToPath(import.meta.url);

export const run = async ({
    args = process.argv.slice(2),
    output = console,
    service,
    paths = fixedRagPaths(entryPath),
} = {}) => {
    const command = parseCommand(args);
    if (command.type === "help") {
        output.log(USAGE);
        return 0;
    }
    const packageService = service ?? createKnowledgePackageService(paths);
    let result;
    if (command.type === "import") {
        result = await packageService.importPackage(command.pack, {
            mode: command.mode,
            sidecarPath: command.sidecar ?? `${command.pack}.sha256`,
        });
    } else if (command.type === "activate") {
        result = await packageService.activate(command.version, { sha256: command.sha256 });
    } else if (command.type === "verify") {
        result = await packageService.verify();
    } else if (command.type === "rollback") {
        result = await packageService.rollback();
    } else {
        result = await packageService.getStatus();
    }
    output.log(JSON.stringify(result));
    return 0;
};

const parseCommand = (args) => {
    if (!args.length || args.some((value) => ["--help", "-h"].includes(value))) return { type: "help" };
    const type = args[0];
    if (!["import", "activate", "verify", "rollback", "status"].includes(type)) throw new Error(USAGE);
    const allowedOptions = {
        import: ["mode", "pack", "sidecar"],
        activate: ["version", "sha256"],
        verify: [],
        rollback: [],
        status: [],
    };
    const options = parseOptions(args.slice(1), new Set(allowedOptions[type]));
    if (type === "import") {
        const mode = required(options, "mode");
        if (mode !== "development") throw new Error(USAGE);
        return { type, mode, pack: required(options, "pack"), sidecar: options.sidecar };
    }
    if (type === "activate") {
        return {
            type,
            version: required(options, "version"),
            sha256: required(options, "sha256"),
        };
    }
    return { type };
};

const parseOptions = (args, allowed) => {
    if (args.length % 2 !== 0) throw new Error(USAGE);
    const options = {};
    for (let index = 0; index < args.length; index += 2) {
        const key = args[index];
        const value = args[index + 1];
        if (!key?.startsWith("--") || value === undefined || value.startsWith("--")) throw new Error(USAGE);
        const name = key.slice(2);
        if (!allowed.has(name) || options[name] !== undefined) throw new Error(USAGE);
        options[name] = value;
    }
    return options;
};

const required = (options, name) => {
    const value = String(options[name] ?? "").trim();
    if (!value) throw new Error(USAGE);
    return value;
};

if (process.argv[1] && resolve(process.argv[1]) === entryPath) {
    run().catch((error) => {
        console.error(JSON.stringify({ error: { code: error?.code ?? "rag_cli_failed", message: error.message } }));
        process.exitCode = 1;
    });
}

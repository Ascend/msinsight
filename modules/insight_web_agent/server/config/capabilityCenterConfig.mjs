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
import { accessSync, constants, readFileSync, statSync } from "node:fs";
import { extname, isAbsolute, resolve } from "node:path";

export const loadCapabilityCenterConfig = ({ configPath, resourceDir, env = process.env, platform = process.platform }) => {
    const config = readConfig(configPath);
    if (!config || typeof config !== "object" || Array.isArray(config)) {
        throw new Error("Capability center config must be an object.");
    }
    const unknownRootField = Object.keys(config).find((field) => !["schemaVersion", "capabilities"].includes(field));
    if (unknownRootField) throw new Error(`Capability center config has unknown field '${unknownRootField}'.`);
    if (config.schemaVersion !== 1) throw new Error(`Unsupported capability center schemaVersion '${config.schemaVersion}'.`);
    if (!Array.isArray(config.capabilities)) throw new Error("Capability center capabilities must be an array.");

    const names = new Set();
    const capabilities = [];
    for (const [index, entry] of config.capabilities.entries()) {
        const capability = normalizeCapability(entry, index, { resourceDir, env, platform });
        if (names.has(capability.name)) throw new Error(`Capability '${capability.name}' is configured more than once.`);
        names.add(capability.name);
        if (capability.executable) capabilities.push(capability);
    }
    return capabilities;
};

const readConfig = (configPath) => {
    try {
        return JSON.parse(readFileSync(configPath, "utf8").replace(/^﻿/, ""));
    } catch (error) {
        throw new Error(`Failed to load capability center config ${configPath}: ${error.message}`);
    }
};

const normalizeCapability = (entry, index, context) => {
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
        throw new Error(`Capability at index ${index} must be an object.`);
    }
    if (entry.type !== "cli") throw new Error(`Capability at index ${index} has unsupported type '${entry.type}'.`);
    const supportedFields = new Set(["type", "name", "description", "executable"]);
    const unknownField = Object.keys(entry).find((field) => !supportedFields.has(field));
    if (unknownField) throw new Error(`CLI capability at index ${index} has unknown field '${unknownField}'.`);
    if (typeof entry.name !== "string" || !entry.name.trim()) {
        throw new Error(`Capability at index ${index} requires a name.`);
    }
    if (entry.description !== undefined && typeof entry.description !== "string") {
        throw new Error(`CLI capability '${entry.name}' requires description to be a string.`);
    }
    const name = entry.name.trim();
    const candidates = platformExecutableCandidates(entry.executable, context.platform);
    if (!candidates.length) throw new Error(`CLI capability '${name}' requires at least one executable candidate.`);
    const executable = resolveFirstExecutable(candidates, context);
    if (!executable) console.warn(`CLI capability '${name}' is unavailable; checked: ${candidates.join(", ")}`);
    return {
        type: "cli",
        name,
        description: entry.description ?? "",
        executable,
    };
};

const platformExecutableCandidates = (configured, platform) => {
    if (configured && typeof configured === "object" && !Array.isArray(configured)) {
        const unknownPlatform = Object.keys(configured).find((key) => !["win32", "darwin", "linux", "default"].includes(key));
        if (unknownPlatform) throw new Error(`Executable config has unknown platform '${unknownPlatform}'.`);
    } else if (typeof configured !== "string" && !Array.isArray(configured)) {
        return [];
    }
    const selected = configured && typeof configured === "object" && !Array.isArray(configured)
        ? configured[platform] ?? configured.default
        : configured;
    const candidates = Array.isArray(selected) ? selected : [selected];
    if (candidates.some((candidate) => typeof candidate !== "string")) {
        throw new Error("Executable candidates must be strings.");
    }
    return candidates.map((candidate) => candidate.trim()).filter(Boolean);
};

const resolveFirstExecutable = (candidates, context) => {
    for (const candidate of candidates) {
        // 配置中的路径相对产品资源目录解析；裸命令名仅用于从当前进程 PATH 查找实际绝对路径。
        const resolved = isPathCandidate(candidate)
            ? resolvePathCandidate(candidate, context.resourceDir, context.platform)
            : resolvePathCommand(candidate, context.env, context.platform);
        if (resolved) return resolved;
    }
    return undefined;
};

const isPathCandidate = (candidate) => isAbsolute(candidate)
    || candidate.includes("/")
    || candidate.includes("\\");

const resolvePathCandidate = (candidate, resourceDir, platform) => {
    const executable = isAbsolute(candidate) ? candidate : resolve(resourceDir, candidate);
    return isExecutable(executable, platform) ? executable : undefined;
};

const resolvePathCommand = (command, env, platform) => {
    const pathValue = platform === "win32" ? env.PATH ?? env.Path : env.PATH;
    if (!pathValue) return undefined;
    const extensions = platform === "win32" && !extname(command)
        ? String(env.PATHEXT ?? ".COM;.EXE").split(";")
            .filter((extension) => [".COM", ".EXE"].includes(extension.toUpperCase()))
        : [""];
    const pathDelimiter = platform === "win32" ? ";" : ":";
    for (const directory of String(pathValue).split(pathDelimiter).filter(Boolean)) {
        for (const extension of extensions) {
            const executable = resolve(directory, `${command}${extension}`);
            if (isExecutable(executable, platform)) return executable;
        }
    }
    return undefined;
};

const isExecutable = (path, platform) => {
    try {
        if (!statSync(path).isFile()) return false;
        if (platform !== "win32") accessSync(path, constants.X_OK);
        return true;
    } catch {
        return false;
    }
};

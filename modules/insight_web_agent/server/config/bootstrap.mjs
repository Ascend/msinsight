/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";
import { existsSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const CONFIG_FILE = "agent-servers.json";

const readJson = (path) => JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, ""));

const writeJsonAtomic = (path, value) => {
    const tempPath = `${path}.${process.pid}.${randomUUID()}.tmp`;
    try {
        writeFileSync(tempPath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
        renameSync(tempPath, path);
    } catch (error) {
        rmSync(tempPath, { force: true });
        throw error;
    }
};

export const bootstrapAgentServersConfig = (rootDir, resourceDir) => {
    const packagedPath = join(resourceDir, CONFIG_FILE);
    const userPath = join(rootDir, CONFIG_FILE);
    const packaged = readJson(packagedPath);

    if (!existsSync(userPath)) {
        writeJsonAtomic(userPath, packaged);
        return packaged;
    }

    const user = readJson(userPath);
    const userServers = Array.isArray(user.agentServers) ? user.agentServers : [];
    const names = new Set(userServers.map((server) => String(server?.name ?? "").trim()));
    const missingBuiltIns = (Array.isArray(packaged.agentServers) ? packaged.agentServers : [])
        .filter((server) => !names.has(String(server?.name ?? "").trim()));
    if (!missingBuiltIns.length) return user;

    const merged = { ...user, agentServers: [...userServers, ...missingBuiltIns] };
    writeJsonAtomic(userPath, merged);
    return merged;
};

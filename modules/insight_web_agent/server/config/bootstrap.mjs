/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const CONFIG_FILE = "agent-servers.json";

const readJson = (path) => JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, ""));

export const bootstrapAgentServersConfig = (rootDir, resourceDir) => {
    const packagedPath = join(resourceDir, CONFIG_FILE);
    const userPath = join(rootDir, CONFIG_FILE);
    const packaged = readJson(packagedPath);

    if (!existsSync(userPath)) {
        writeFileSync(userPath, `${JSON.stringify(packaged, null, 2)}\n`, "utf8");
        return packaged;
    }

    const user = readJson(userPath);
    const userServers = Array.isArray(user.agentServers) ? user.agentServers : [];
    const names = new Set(userServers.map((server) => String(server?.name ?? "").trim()));
    const missingBuiltIns = (Array.isArray(packaged.agentServers) ? packaged.agentServers : [])
        .filter((server) => !names.has(String(server?.name ?? "").trim()));
    if (!missingBuiltIns.length) return user;

    const merged = { ...user, agentServers: [...userServers, ...missingBuiltIns] };
    writeFileSync(userPath, `${JSON.stringify(merged, null, 2)}\n`, "utf8");
    return merged;
};

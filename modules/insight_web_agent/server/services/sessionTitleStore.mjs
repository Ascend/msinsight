/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2.
 */
import { randomUUID } from "node:crypto";
import { mkdir, readFile, rename, unlink, writeFile } from "node:fs/promises";
import { dirname } from "node:path";

const keyOf = (agentId, sessionId) => JSON.stringify([agentId, sessionId]);
const validId = (value) => typeof value === "string" && value.trim().length > 0 && value.length <= 2048;
export const validSessionTitle = (value) => typeof value === "string" && value.trim().length > 0
    && [...value.trim()].length <= 200 && !/[\u0000-\u001f\u007f]/u.test(value);

/** App-owned titles: ACP has no universally available rename operation. */
export const createSessionTitleStore = async ({ filePath }) => {
    let titles = new Map();
    let loadError;
    let queue = Promise.resolve();
    try {
        const document = JSON.parse(await readFile(filePath, "utf8"));
        if (document?.version !== 1 || !Array.isArray(document.titles) || document.titles.length > 10_000) throw new Error("Invalid title store");
        for (const entry of document.titles) {
            if (!validId(entry?.agentId) || !validId(entry?.sessionId) || !validSessionTitle(entry?.title)) throw new Error("Invalid title entry");
            const key = keyOf(entry.agentId, entry.sessionId);
            if (titles.has(key)) throw new Error("Duplicate title entry");
            titles.set(key, { agentId: entry.agentId, sessionId: entry.sessionId, title: entry.title.trim() });
        }
    } catch (error) {
        if (error.code !== "ENOENT") {
            titles = new Map();
            loadError = error;
            // Preserve unreadable/corrupt files; never replace them with an empty map.
            console.warn("Session title store could not be read; renaming is unavailable");
        }
    }
    return {
        resolve: (agentId, sessionId, fallback) => titles.get(keyOf(agentId, sessionId))?.title ?? fallback,
        set(agentId, sessionId, value) {
            const operation = queue.then(async () => {
                if (loadError) throw new Error("Session title store is unavailable");
                if (!validId(agentId) || !validId(sessionId) || !validSessionTitle(value)) throw new Error("Invalid session title");
                const title = value.trim();
                const next = new Map(titles);
                next.set(keyOf(agentId, sessionId), { agentId, sessionId, title });
                if (next.size > 10_000) throw new Error("Session title capacity reached");
                await mkdir(dirname(filePath), { recursive: true, mode: 0o700 });
                const temporary = `${filePath}.${randomUUID()}.tmp`;
                try {
                    await writeFile(temporary, JSON.stringify({ version: 1, titles: [...next.values()] }), { encoding: "utf8", mode: 0o600, flag: "wx" });
                    await rename(temporary, filePath);
                } finally {
                    await unlink(temporary).catch((error) => { if (error.code !== "ENOENT") console.warn("Session title temporary file cleanup failed"); });
                }
                titles = next;
                return title;
            });
            queue = operation.catch(() => {});
            return operation;
        },
    };
};

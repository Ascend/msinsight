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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { randomUUID } from "node:crypto";
import { mkdir, readFile, readdir, rename, unlink, writeFile } from "node:fs/promises";
import { basename, join } from "node:path";
import { toStoredSession } from "../shared/utils.mjs";

const SESSION_FILE_EXTENSION = ".jsonl";

/** 功能：创建每会话一个 JSONL 文件的 Native Session Store。 */
export const createSessionStore = ({ sessions, storeDir = join(process.cwd(), ".msinsight_native_agent"), createFilesystemRoots, canonicalizeFilesystemRoots, canonicalizeProjectRoot }) => {
    const sessionStoreDir = join(storeDir, "sessions");
    const persistedSessionIds = new Set();
    let saveQueue = Promise.resolve();
    let loadPromise;

    const load = () => {
        if (!loadPromise) loadPromise = loadOnce();
        return loadPromise;
    };

    const loadOnce = async () => {
        await mkdir(sessionStoreDir, { recursive: true });
        const entries = await readdir(sessionStoreDir, { withFileTypes: true });
        for (const entry of entries) {
            if (!entry.isFile() || !entry.name.endsWith(SESSION_FILE_EXTENSION)) continue;
            try {
                await restoreSessionFile(join(sessionStoreDir, entry.name));
            } catch (error) {
                console.warn(`Failed to load native session ${entry.name}: ${error.message}`);
            }
        }
    };

    const restoreSessionFile = async (filePath) => {
        const records = (await readFile(filePath, "utf8")).split(/\r?\n/).filter(Boolean).map(JSON.parse);
        const header = records[0];
        if (records.length !== 1 || header?.type !== "session_metadata" || header.version !== 3 || !header.session) throw new Error("unsupported session format");
        const item = header.session;
        const sessionId = String(item.sessionId ?? "").trim();
        if (!sessionId || basename(filePath, SESSION_FILE_EXTENSION) !== sessionId) throw new Error("sessionId does not match file name");
        sessions.set(sessionId, {
            sessionId,
            title: String(item.title ?? "New session"),
            messages: [],
            runtimeSession: undefined,
            hostSystemPrompt: String(item.hostSystemPrompt ?? ""),
            primaryAgentId: String(item.primaryAgentId ?? "general"),
            primaryAgentFingerprint: item.primaryAgentFingerprint ? String(item.primaryAgentFingerprint) : undefined,
            promptStarted: Boolean(item.promptStarted),
            primaryAgentBody: undefined,
            primaryAgentBashRules: [],
            primaryAgentError: undefined,
            projectRoot: item.projectRoot ? String(item.projectRoot) : undefined,
            lastPageObservationFingerprint: item.lastPageObservationFingerprint ? String(item.lastPageObservationFingerprint) : undefined,
            filesystemRoots: [],
            canonicalFilesystemRoots: [],
            createdAt: Number(item.createdAt ?? Date.now()),
            updatedAt: Number(item.updatedAt ?? item.createdAt ?? Date.now()),
        });
        const session = sessions.get(sessionId);
        session.projectRoot = await canonicalizeProjectRoot(session.projectRoot);
        session.filesystemRoots = createFilesystemRoots(session.projectRoot);
        session.canonicalFilesystemRoots = await canonicalizeFilesystemRoots(session.filesystemRoots);
        persistedSessionIds.add(sessionId);
    };

    const save = async () => {
        const previousSave = saveQueue;
        let releaseSave;
        saveQueue = new Promise((resolve) => { releaseSave = resolve; });
        try {
            try {
                await previousSave;
            } catch {
                // A failed session write must not block later saves.
            }
            await writeSessions();
        } finally {
            releaseSave();
        }
    };

    const writeSessions = async () => {
        await mkdir(sessionStoreDir, { recursive: true });
        for (const session of sessions.values()) await writeSession(session);
        const activeIds = new Set(sessions.keys());
        for (const sessionId of [...persistedSessionIds]) {
            if (activeIds.has(sessionId)) continue;
            await unlink(join(sessionStoreDir, `${sessionId}${SESSION_FILE_EXTENSION}`)).catch((error) => {
                if (error.code !== "ENOENT") throw error;
            });
            persistedSessionIds.delete(sessionId);
        }
    };

    const writeSession = async (session) => {
        const filePath = join(sessionStoreDir, `${session.sessionId}${SESSION_FILE_EXTENSION}`);
        const content = `${JSON.stringify({ type: "session_metadata", version: 3, session: toStoredSession(session) })}\n`;
        try {
            if (await readFile(filePath, "utf8") === content) {
                persistedSessionIds.add(session.sessionId);
                return;
            }
        } catch (error) {
            if (error.code !== "ENOENT") throw error;
        }
        const tempPath = `${filePath}.${process.pid}.${randomUUID()}.tmp`;
        await writeFile(tempPath, content, "utf8");
        await rename(tempPath, filePath);
        persistedSessionIds.add(session.sessionId);
    };

    return { load, save, storeDir, sessionStoreDir };
};

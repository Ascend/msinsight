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

const BLADE_SESSION_FILE_EXTENSION = ".jsonl";

/** 功能：创建 ACP 会话文件存储，负责加载、串行保存和原子替换 sessions.json。 */
export const createSessionStore = ({ sessions, storeDir = join(process.cwd(), ".msinsight_native_agent"), bladeStoragePath = join(storeDir, "blade"), createFilesystemRoots, canonicalizeFilesystemRoots, canonicalizeProjectRoot }) => {
    const sessionStorePath = join(storeDir, "sessions.json");
    let saveQueue = Promise.resolve();
    let loadPromise;

    /** 功能：返回首次加载 Promise，确保 ACP 每个请求等待初始化但不会重复覆盖运行时会话。 */
    const load = () => {
        if (!loadPromise) loadPromise = loadOnce();
        return loadPromise;
    };

    /** 功能：从 sessions.json 加载 ACP 会话，并重建运行时文件系统根目录。 */
    const loadOnce = async () => {
        await mkdir(bladeStoragePath, { recursive: true });
        let payload;
        try {
            payload = JSON.parse(await readFile(sessionStorePath, "utf8"));
        } catch (error) {
            if (error.code === "ENOENT") await cleanupUnreferencedBladeSessions();
            else console.warn(`Failed to load native session store: ${error.message}`);
            return;
        }
        if (!Array.isArray(payload?.sessions)) {
            console.warn("Failed to load native session store: sessions must be an array");
            return;
        }
        for (const item of payload.sessions) {
            const sessionId = String(item?.sessionId ?? "").trim();
            if (!sessionId) continue;
            await restoreStoredSession(item, sessionId);
        }
        await cleanupUnreferencedBladeSessions();
    };

    /** 功能：删除 Blade runtime 中未被 ACP 会话引用的持久化上下文。 */
    const cleanupUnreferencedBladeSessions = async () => {
        const referencedIds = new Set([...sessions.values()].map(readStoredRuntimeSessionId).filter(Boolean));
        const sessionsDir = join(bladeStoragePath, "sessions");
        let entries;
        try {
            entries = await readdir(sessionsDir, { withFileTypes: true });
        } catch (error) {
            if (error.code !== "ENOENT") console.warn(`Failed to inspect Blade session storage: ${error.message}`);
            return;
        }
        for (const entry of entries) {
            if (!entry.isFile() || !entry.name.endsWith(BLADE_SESSION_FILE_EXTENSION)) continue;
            const runtimeSessionId = basename(entry.name, BLADE_SESSION_FILE_EXTENSION);
            if (!runtimeSessionId || referencedIds.has(runtimeSessionId)) continue;
            await deleteUnreferencedBladeSession(join(sessionsDir, entry.name), runtimeSessionId);
        }
    };

    /** 功能：删除单个无引用 Blade 会话文件，失败时只记录告警不阻断 native-agent 启动。 */
    const deleteUnreferencedBladeSession = async (filePath, runtimeSessionId) => {
        try {
            await unlink(filePath);
            console.info(`Deleted unreferenced Blade session: ${runtimeSessionId}`);
        } catch (error) {
            if (error.code !== "ENOENT") console.warn(`Failed to delete unreferenced Blade session ${runtimeSessionId}: ${error.message}`);
        }
    };

    /** 功能：读取 ACP 会话持久化的 Blade runtime 标识。 */
    const readStoredRuntimeSessionId = (session) => String(session.runtimeSessionId ?? session.bladeSessionId ?? "").trim();

    /** 功能：把一条持久化记录恢复为运行时会话，并补齐文件系统白名单缓存。 */
    const restoreStoredSession = async (item, sessionId) => {
        sessions.set(sessionId, {
            sessionId,
            title: String(item.title ?? "New session"),
            messages: Array.isArray(item.messages) ? item.messages : [],
            bladeSession: undefined,
            bladeSessionId: item.bladeSessionId ? String(item.bladeSessionId) : undefined,
            hostSystemPrompt: String(item.hostSystemPrompt ?? ""),
            bladeSystemPrompt: String(item.bladeSystemPrompt ?? ""),
            bladeContextNeedsRestore: Boolean(item.bladeContextNeedsRestore),
            projectRoot: item.projectRoot ? String(item.projectRoot) : undefined,
            filesystemRoots: [],
            canonicalFilesystemRoots: [],
            createdAt: Number(item.createdAt ?? Date.now()),
            updatedAt: Number(item.updatedAt ?? item.createdAt ?? Date.now()),
        });
        const session = sessions.get(sessionId);
        session.projectRoot = await canonicalizeProjectRoot(session.projectRoot);
        session.filesystemRoots = createFilesystemRoots(session.projectRoot);
        session.canonicalFilesystemRoots = await canonicalizeFilesystemRoots(session.filesystemRoots);
    };

    /** 功能：串行调度 ACP 会话快照写入，避免并发请求互相覆盖。 */
    const save = () => {
        // 多个 ACP 请求可并发触发保存；用 Promise 链串行执行原子替换，避免后一次写入抢先移动临时文件或旧快照覆盖新快照。
        const pendingSave = saveQueue.then(writeSnapshot, writeSnapshot);
        saveQueue = pendingSave.catch(ignoreSaveQueueFailure);
        return pendingSave;
    };

    /** 功能：吞掉仅用于串行调度的 Promise 链失败，实际调用者仍通过 pendingSave 收到原始错误。 */
    const ignoreSaveQueueFailure = () => {};

    /** 功能：生成当前 ACP 会话快照，并通过唯一临时文件原子替换 sessions.json。 */
    const writeSnapshot = async () => {
        await mkdir(storeDir, { recursive: true });
        const payload = {
            version: 1,
            sessions: [...sessions.values()].map(toStoredSession),
        };
        const tempPath = `${sessionStorePath}.${process.pid}.${randomUUID()}.tmp`;
        await writeFile(tempPath, `${JSON.stringify(payload, null, 2)}\n`, "utf8");
        await rename(tempPath, sessionStorePath);
        await cleanupUnreferencedBladeSessions();
    };

    return {
        load,
        save,
        storeDir,
        sessionStorePath,
        bladeStoragePath,
    };
};

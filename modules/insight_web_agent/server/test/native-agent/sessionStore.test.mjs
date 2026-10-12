/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { createSessionStore } from "../../native-agent/data/sessionStore.mjs";

const createStore = (sessions, storeDir) => createSessionStore({
    sessions,
    storeDir,
    createFilesystemRoots: () => [storeDir],
    canonicalizeFilesystemRoots: async (roots) => roots,
    canonicalizeProjectRoot: async (root) => root,
});

test("session store persists only Insight metadata", async (t) => {
    const storeDir = await mkdtemp(join(tmpdir(), "msinsight-session-store-"));
    t.after(() => rm(storeDir, { recursive: true, force: true }));
    const sessions = new Map([["session-1", {
        sessionId: "session-1",
        title: "Metadata",
        messages: [{ id: "assistant-1", role: "assistant", content: [{ id: "text-1", type: "text", text: "not persisted" }] }],
        primaryAgentId: "general",
        projectRoot: "D:/project",
        createdAt: 1,
        updatedAt: 2,
    }]]);

    const store = createStore(sessions, storeDir);
    await store.save();
    const records = (await readFile(join(storeDir, "sessions", "session-1.jsonl"), "utf8")).trim().split(/\r?\n/).map(JSON.parse);
    assert.equal(records.length, 1);
    assert.equal(records[0].type, "session_metadata");
    assert.equal(records[0].version, 3);
    assert.equal(records[0].session.messages, undefined);

    const restored = new Map();
    const restoredStore = createStore(restored, storeDir);
    await restoredStore.load();
    assert.deepEqual(restored.get("session-1").messages, []);

    restored.delete("session-1");
    await restoredStore.save();
    await assert.rejects(readFile(join(storeDir, "sessions", "session-1.jsonl"), "utf8"), /ENOENT/);
});

test("session store ignores legacy snapshots and preserves unknown JSONL", async (t) => {
    const storeDir = await mkdtemp(join(tmpdir(), "msinsight-session-store-legacy-"));
    t.after(() => rm(storeDir, { recursive: true, force: true }));
    await mkdir(join(storeDir, "sessions"), { recursive: true });
    await writeFile(join(storeDir, "sessions.json"), JSON.stringify({ version: 1, sessions: [{ sessionId: "legacy" }] }), "utf8");
    const unknownPath = join(storeDir, "sessions", "unknown.jsonl");
    await writeFile(unknownPath, `${JSON.stringify({ type: "session", version: 2 })}\n`, "utf8");

    const sessions = new Map();
    const store = createStore(sessions, storeDir);
    await store.load();
    await store.save();

    assert.equal(sessions.size, 0);
    assert.match(await readFile(join(storeDir, "sessions.json"), "utf8"), /"legacy"/);
    assert.match(await readFile(unknownPath, "utf8"), /"version":2/);
});

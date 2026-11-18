/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2.
 */
import assert from "node:assert/strict";
import { mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { Readable } from "node:stream";
import test from "node:test";
import { createSessionTitleStore } from "../../services/sessionTitleStore.mjs";
import { createSessionService } from "../../services/sessionService.mjs";
import { setLocalTitle } from "../../services/messageService.mjs";
import { createRuntimeState, resetRuntimeForAgent, restoreRuntimeState, snapshotRuntimeState } from "../../state/runtimeState.mjs";
import { createRouter } from "../../http/router.mjs";
import { createSessionController } from "../../controllers/sessionController.mjs";
import { createApp } from "../../app.mjs";

const fixture = async (t) => {
    const directory = await mkdtemp(join(tmpdir(), "insight-rename-"));
    t.after(async () => {
        assert.equal(dirname(resolve(directory)), resolve(tmpdir()));
        assert.ok(basename(directory).startsWith("insight-rename-"));
        await rm(directory, { recursive: true, force: true });
    });
    const filePath = join(directory, "session-titles.json");
    const store = await createSessionTitleStore({ filePath });
    const state = createRuntimeState();
    state.activeAgentName = "agent-a";
    state.agentCapabilities = { session: { list: true } };
    state.sessions = [{ sessionId: "same", title: "Original", updatedAt: "2026-10-27T00:00:00Z" }];
    const messages = [{ id: "m", role: "user", content: [{ id: "c", type: "text", text: "Keep conversation" }] }];
    state.sessionContexts.set("same", { sessionId: "same", agentId: "agent-a", messages, pendingPrompt: true });
    const calls = [];
    const acpClient = { request: async (method) => {
        calls.push(method);
        if (method === "session/list") return { sessions: [{ sessionId: "same", title: "ACP automatic title" }] };
        throw new Error("Renaming must not call ACP");
    } };
    const service = createSessionService({ state, titleStore: store, config: {}, eventBus: { broadcast() {} }, acpClient });
    return { directory, filePath, store, state, service, messages, calls };
};

test("persists names across restart, serialized saves and identical session IDs in different owners", async (t) => {
    const { store, filePath } = await fixture(t);
    await Promise.all([store.set("agent-a", "same", "  DeepSeek memory  "), store.set("agent-b", "same", "Other owner"), store.set("agent-a", "other", "Third")]);
    const restarted = await createSessionTitleStore({ filePath });
    assert.equal(restarted.resolve("agent-a", "same"), "DeepSeek memory");
    assert.equal(restarted.resolve("agent-b", "same"), "Other owner");
    assert.equal(restarted.resolve("agent-a", "other"), "Third");
    assert.equal(restarted.resolve("agent-c", "same", "Fallback"), "Fallback");
    if (process.platform !== "win32") assert.equal((await stat(filePath)).mode & 0o777, 0o600);
});

test("metadata-only rename while generating preserves conversation and wins over subsequent ACP titles", async (t) => {
    const { state, service, messages, calls } = await fixture(t);
    assert.deepEqual(await service.renameSessionById("same", " DeepSeek renamed ", "agent-a"), { ok: true, agentId: "agent-a", sessionId: "same", title: "DeepSeek renamed" });
    assert.equal(state.sessionContexts.get("same").messages, messages);
    assert.equal(state.sessionContexts.get("same").pendingPrompt, true);
    assert.deepEqual(calls, []);
    setLocalTitle(state, "same", "Automatic overwrite");
    assert.equal(state.sessions[0].title, "DeepSeek renamed");
    const exported = await service.exportSessionById("same");
    assert.equal(exported.session.title, "DeepSeek renamed");
    assert.ok(exported.markdown.startsWith("# DeepSeek renamed"));
    assert.equal((await service.listSessions())[0].title, "DeepSeek renamed");
    const snapshot = snapshotRuntimeState(state);
    resetRuntimeForAgent(state, { activeAgentName: "agent-b", agentServers: [] });
    assert.equal(state.customTitles.size, 0);
    restoreRuntimeState(state, snapshot);
    assert.equal(state.customTitles.get("same"), "DeepSeek renamed");
});

test("unknown session, owner change, missing inputs, control characters and oversized titles do not write", async (t) => {
    const { state, service, store } = await fixture(t);
    for (const title of ["", "  ", "x".repeat(201), "bad\nline", "bad\u0000title", { title: "object" }]) {
        assert.equal((await service.renameSessionById("same", title, "agent-a")).status, 400);
    }
    assert.equal((await service.renameSessionById({}, "Valid", "agent-a")).status, 400);
    assert.equal((await service.renameSessionById("same", "Valid")).status, 400);
    assert.equal((await service.renameSessionById("missing", "Valid", "agent-a")).status, 404);
    assert.equal((await service.renameSessionById("same", "Valid", "agent-b")).status, 409);
    assert.equal(store.resolve("agent-a", "same"), undefined);
    assert.equal(state.sessions[0].title, "Original");
    assert.equal((await service.renameSessionById("same", "😀".repeat(200), "agent-a")).ok, true);
});

test("save failure leaves public title unchanged; corrupt stores are preserved", async (t) => {
    const { state, service, directory } = await fixture(t);
    const brokenPath = join(directory, "broken.json");
    await writeFile(brokenPath, '{"version":1,"titles":[', "utf8");
    const broken = await createSessionTitleStore({ filePath: brokenPath });
    await assert.rejects(broken.set("agent-a", "same", "New title"));
    assert.equal(await readFile(brokenPath, "utf8"), '{"version":1,"titles":[');
    const failureService = createSessionService({ state, titleStore: broken, config: {}, eventBus: { broadcast() {} } });
    assert.equal((await failureService.renameSessionById("same", "New title", "agent-a")).status, 500);
    assert.equal(state.sessions[0].title, "Original");
    assert.equal((await service.renameSessionById("same", "Saved", "agent-a")).ok, true);
    const missingParent = join(directory, "not-a-directory");
    await writeFile(missingParent, "preserve", "utf8");
    const unavailable = await createSessionTitleStore({ filePath: join(missingParent, "titles.json") });
    await assert.rejects(unavailable.set("agent-a", "same", "Never applied"));
    assert.equal(unavailable.resolve("agent-a", "same"), undefined);
});

test("a late save cannot overwrite a newly switched owner's identical session ID", async (t) => {
    const { state } = await fixture(t);
    let release;
    const saved = new Promise((resolve) => { release = resolve; });
    const service = createSessionService({ state, titleStore: { set: () => saved }, config: {}, eventBus: { broadcast() {} } });
    const pending = service.renameSessionById("same", "Old owner title", "agent-a");
    await new Promise((resolve) => setImmediate(resolve));
    resetRuntimeForAgent(state, { activeAgentName: "agent-b", agentServers: [] });
    state.sessions = [{ sessionId: "same", title: "B title" }];
    release("Old owner title");
    assert.equal((await pending).agentId, "agent-a");
    assert.equal(state.sessions[0].title, "B title");
    assert.equal(state.customTitles.size, 0);
});

test("late lists cannot authorize old-runtime sessions under a new owner, including A/B/A switches", async (t) => {
    const { state, store } = await fixture(t);
    let release;
    const response = new Promise((resolve) => { release = resolve; });
    const service = createSessionService({ state, titleStore: store, config: {}, eventBus: { broadcast() {} }, acpClient: { request: () => response } });
    const pending = service.listSessions();
    resetRuntimeForAgent(state, { activeAgentName: "agent-b", agentServers: [] });
    state.sessions = [{ sessionId: "b-only", title: "B" }];
    release({ sessions: [{ sessionId: "a-only", title: "A" }] });
    assert.equal((await pending)[0].sessionId, "b-only");
    assert.equal((await service.renameSessionById("a-only", "Wrong owner", "agent-b")).status, 404);
    assert.equal(store.resolve("agent-b", "a-only"), undefined);
    resetRuntimeForAgent(state, { activeAgentName: "agent-a", agentServers: [] });
    state.agentCapabilities = { session: { list: true } };
    let releaseSecond;
    const second = createSessionService({ state, titleStore: store, config: {}, eventBus: { broadcast() {} }, acpClient: { request: () => new Promise((done) => { releaseSecond = done; }) } });
    const staleA = second.listSessions();
    resetRuntimeForAgent(state, { activeAgentName: "agent-b", agentServers: [] });
    resetRuntimeForAgent(state, { activeAgentName: "agent-a", agentServers: [] });
    releaseSecond({ sessions: [{ sessionId: "old-runtime-only", title: "Old A" }] });
    assert.deepEqual(await staleA, []);
    assert.equal((await second.renameSessionById("old-runtime-only", "Stale title", "agent-a")).status, 404);
});

test("rename route forwards owner/title and preserves service status", async (t) => {
    const { service } = await fixture(t);
    const router = createRouter({ sessionController: createSessionController({ sessionService: service }) });
    for (const [agentId, status] of [["agent-a", 200], ["agent-b", 409]]) {
        const req = Object.assign(Readable.from([JSON.stringify({ sessionId: "same", title: "Renamed", agentId })]), { method: "POST", url: "/api/sessions/rename", headers: { host: "localhost" } });
        const res = { writeHead(code) { this.status = code; }, end(value) { this.body = JSON.parse(value); } };
        await router(req, res);
        assert.equal(res.status, status);
        assert.equal(res.body.status, undefined);
        if (status === 200) assert.equal(res.body.title, "Renamed");
        else assert.equal(res.body.error, "session_owner_changed");
    }
});

test("HTTP rename requires Host authentication and persists the returned title", async (t) => {
    const { service } = await fixture(t);
    const app = createApp({ sessionService: service, capabilityToken: "rename-test-token", allowedOrigins: [] });
    await new Promise((done) => app.listen(0, "127.0.0.1", done));
    t.after(() => new Promise((done) => { app.close(done); app.closeAllConnections(); }));
    const url = `http://127.0.0.1:${app.address().port}/api/sessions/rename`;
    const body = JSON.stringify({ sessionId: "same", title: "HTTP renamed", agentId: "agent-a" });
    assert.equal((await fetch(url, { method: "POST", headers: { "content-type": "application/json" }, body })).status, 401);
    const response = await fetch(url, { method: "POST", headers: { "content-type": "application/json", authorization: "Bearer rename-test-token" }, body });
    assert.equal(response.status, 200);
    assert.equal((await response.json()).title, "HTTP renamed");
});

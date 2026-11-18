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
import assert from "node:assert/strict";
import { Readable } from "node:stream";
import test from "node:test";
import { createRouter } from "../../http/router.mjs";
import { createApp } from "../../app.mjs";
import { createSessionBatchExportController } from "../../controllers/sessionBatchExportController.mjs";
import { createSessionBatchExportService } from "../../services/sessionBatchExport.mjs";
import { setImmediate as yieldTurn } from "node:timers/promises";

const request = (method, path, body) => Object.assign(
    Readable.from(body === undefined ? [] : [JSON.stringify(body)]),
    { method, url: path, headers: { host: "127.0.0.1" } },
);
const response = () => ({
    status: undefined, body: undefined,
    writeHead(status) { this.status = status; },
    end(body) { this.body = JSON.parse(body); },
});
const routerFor = (service) => createRouter({
    sessionBatchExportController: createSessionBatchExportController({ sessionBatchExportService: service }),
    sessionController: { export: (_req, res, body) => { res.writeHead(200); res.end(JSON.stringify({ single: body.sessionId })); } },
});

test("batch routes preserve filters, opaque owner-qualified selections and all-snapshot export", async () => {
    const calls = [];
    const service = {
        startQuery: (filters) => { calls.push(["query", filters]); return { jobId: "a/b" }; },
        getStatus: (id) => { calls.push(["status", id]); return { jobId: id, phase: "ready", items: [] }; },
        startExport: (body) => { calls.push(["export", body]); return { jobId: body.jobId }; },
    };
    const router = routerFor(service);
    const filters = { agentIds: ["owner-A", "owner-B"], from: "2026-10-01T00:00:00+08:00", to: "2026-10-28T00:00:00Z", keyword: "ds memory" };
    const query = response();
    await router(request("POST", "/api/sessions/export/query", filters), query);
    assert.equal(query.status, 202);
    assert.deepEqual(query.body, { jobId: "a/b" });
    const status = response();
    await router(request("GET", "/api/sessions/export/status?jobId=a%2Fb"), status);
    assert.equal(status.status, 200);
    const all = { jobId: "a/b", persist: false };
    await router(request("POST", "/api/sessions/export/batch", all), response());
    const selected = { jobId: "a/b", keys: ['["owner-B","same-id"]'], persist: true };
    await router(request("POST", "/api/sessions/export/batch", selected), response());
    assert.deepEqual(calls, [["query", filters], ["status", "a/b"], ["export", all], ["export", selected]]);
    const single = response();
    await router(request("POST", "/api/sessions/export", { sessionId: "old" }), single);
    assert.deepEqual(single.body, { single: "old" });
});

test("expired jobs preserve the HTTP status and machine code with localized recovery guidance", async (t) => {
    const service = createSessionBatchExportService({ listAgents: () => [], openSource: () => assert.fail("No sources") });
    t.after(() => service.dispose());
    const req = request("GET", "/api/sessions/export/status?jobId=missing");
    req.headers["accept-language"] = "zh-CN";
    const res = { ...response(), req };
    await routerFor(service)(req, res);
    assert.equal(res.status, 404);
    assert.equal(res.body.error, "session_export_job_expired");
    assert.equal(res.body.code, "session_export_job_expired");
    assert.equal(res.body.message, "导出任务不存在或已过期，请重新查询。");
});

test("real query/status/export routes deliver every cursor page and owner-qualified selection", async (t) => {
    const updatedAt = "2026-10-27T00:00:00.000Z";
    const pageCalls = [];
    const service = createSessionBatchExportService({
        now: () => new Date("2026-10-28T00:00:00.000Z"),
        listAgents: () => [{ name: "A" }, { name: "B" }],
        openSource: async ({ name }) => ({
            agentInfo: { name, version: "test" },
            list: async ({ cursor }) => {
                pageCalls.push([name, cursor]);
                const ids = name === "B" ? ["same-id"] : cursor ? ["page-2"] : ["same-id", "page-1"];
                return { sessions: ids.map((sessionId) => ({ sessionId, title: "matching memory", updatedAt })), ...(name === "A" && !cursor ? { nextCursor: "second-page" } : {}) };
            },
            load: async (sessionId) => [{ role: "user", content: [{ type: "text", text: `${name}/${sessionId}` }] }],
            close: async () => {},
        }),
    });
    t.after(() => service.dispose());
    const router = routerFor(service);
    const run = async (method, path, body) => {
        const res = response();
        await router(request(method, path, body), res);
        assert.ok(res.status < 400, JSON.stringify(res.body));
        return res.body;
    };
    const { jobId } = await run("POST", "/api/sessions/export/query", { days: 7, keyword: "memory" });
    const waitForPhase = async (phase) => {
        for (let count = 0; count < 1000; count++) {
            const result = await run("GET", `/api/sessions/export/status?jobId=${jobId}`);
            if (result.phase === phase) return result;
            assert.notEqual(result.phase, "failed", JSON.stringify(result));
            await yieldTurn();
        }
        assert.fail(`Export job did not reach ${phase}`);
    };
    const preview = await waitForPhase("ready");
    assert.equal(preview.items.length, 4);
    assert.deepEqual(pageCalls, [["A", undefined], ["A", "second-page"], ["B", undefined]]);
    const selected = preview.items.find((item) => item.agentId === "B");
    await run("POST", "/api/sessions/export/batch", { jobId, keys: [selected.key], persist: false });
    const partial = await waitForPhase("completed");
    assert.equal(partial.document.sessions.length, 1);
    assert.deepEqual(partial.document.sessions[0].owner, { agentId: "B" });
    await run("POST", "/api/sessions/export/batch", { jobId, persist: false });
    const all = await waitForPhase("completed");
    assert.equal(all.document.format, "msinsight.agent-sessions.v1");
    assert.equal(all.document.sessions.length, 4);
    assert.equal(all.document.sessions.filter((item) => item.session.sessionId === "same-id").length, 2);
});

test("real routes validate custom time ranges and reject message-only matches before loading", async (t) => {
    const loads = [];
    const updatedAt = "2024-02-29T16:00:00.000Z";
    const service = createSessionBatchExportService({
        now: () => new Date("2026-10-28T00:00:00.000Z"),
        listAgents: () => [{ name: "A" }, { name: "B" }],
        resolveTitle: (owner, _id, fallback) => owner === "A" ? "Local DeepSeek" : fallback,
        openSource: async ({ name }) => ({
            list: async () => ({ sessions: [{ sessionId: "same", title: "Unrelated", updatedAt }] }),
            load: async (id) => { loads.push([name, id]); return [{ role: "user", content: [{ type: "text", text: "DeepSeek message-only" }] }]; },
            close: async () => {},
        }),
    });
    t.after(() => service.dispose());
    const router = routerFor(service);
    for (const filters of [
        {}, { from: updatedAt }, { days: 7, from: updatedAt, to: updatedAt },
        { from: "2024-02-30T00:00:00Z", to: "2024-03-01T00:00:00Z" },
        { from: "2024-02-29T00:00:00+24:00", to: updatedAt },
        { from: "2024-02-29T00:00:00", to: updatedAt },
        { from: "2024-02-29T00:00:00.1234Z", to: updatedAt },
        { from: "2024-03-01T00:00:00Z", to: updatedAt },
    ]) {
        const res = response();
        await router(request("POST", "/api/sessions/export/query", filters), res);
        assert.equal(res.status, 400, JSON.stringify(filters));
    }
    assert.deepEqual(loads, []);
    const query = response();
    await router(request("POST", "/api/sessions/export/query", { from: "2024-03-01T00:00:00+08:00", to: updatedAt, keyword: "ds" }), query);
    assert.equal(query.status, 202);
    let ready;
    for (let count = 0; count < 1000; count++) {
        const res = response();
        await router(request("GET", `/api/sessions/export/status?jobId=${query.body.jobId}`), res);
        assert.equal(res.status, 200);
        assert.notEqual(res.body.phase, "failed", JSON.stringify(res.body));
        if (res.body.phase === "ready") { ready = res.body; break; }
        await yieldTurn();
    }
    assert.ok(ready, "Query did not reach ready");
    assert.deepEqual(ready.items.map(({ agentId, title }) => ({ agentId, title })), [{ agentId: "A", title: "Local DeepSeek" }]);
    assert.deepEqual(ready.filters, { agentIds: ["A", "B"], keyword: "ds", from: updatedAt, to: updatedAt, timestamp: "updatedAt" });
    assert.deepEqual(loads, [["A", "same"]]);
});

test("batch endpoints inherit capability authentication and origin checks", async (t) => {
    let started = 0;
    const server = createApp({
        capabilityToken: "batch-test-token",
        allowedOrigins: ["http://localhost:3011"],
        sessionBatchExportService: { startQuery: () => { started += 1; return { jobId: "owned" }; } },
    });
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    t.after(async () => {
        server.closeAllConnections();
        await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
    });
    const url = `http://127.0.0.1:${server.address().port}/api/sessions/export/query`;
    const send = (headers = {}) => fetch(url, { method: "POST", headers: { "content-type": "application/json", ...headers }, body: JSON.stringify({ days: 7 }) });
    assert.equal((await send()).status, 401);
    assert.equal((await send({ authorization: "Bearer batch-test-token", origin: "https://evil.invalid" })).status, 403);
    assert.equal(started, 0);
    const accepted = await send({ authorization: "Bearer batch-test-token", origin: "http://localhost:3011" });
    assert.equal(accepted.status, 202);
    assert.deepEqual(await accepted.json(), { jobId: "owned" });
    assert.equal(started, 1);
});

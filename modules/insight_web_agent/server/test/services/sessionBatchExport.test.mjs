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
import assert from "node:assert/strict";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import test from "node:test";
import { setImmediate as yieldTurn, setTimeout as delay } from "node:timers/promises";
import { createSessionBatchExportService } from "../../services/sessionBatchExport.mjs";

const NOW = "2026-09-15T12:34:56.789Z";
const session = (sessionId, extra = {}) => ({ sessionId, title: sessionId, updatedAt: NOW, ...extra });
const messages = (text = "hello") => [{ id: "m1", role: "user", content: [{ id: "b1", type: "text", text }] }];
const deferred = () => { let resolve; let reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; };
const waitFor = async (service, id, phase) => {
    const deadline = Date.now() + 3000;
    while (Date.now() < deadline) {
        const status = service.getStatus(id);
        if (status.phase === phase) return status;
        if (status.phase === "failed" && phase !== "failed") assert.fail(JSON.stringify(status));
        await yieldTurn();
    }
    assert.fail(`Job did not reach ${phase}`);
};
const fixture = (t, owners, options = {}) => {
    const calls = [];
    const closes = [];
    const service = createSessionBatchExportService({
        now: () => NOW,
        listAgents: () => Object.keys(owners).map((name) => ({ name, env: { TOP_SECRET: "must-not-leak" } })),
        openSource: async ({ name }) => {
            const owner = owners[name];
            if (owner.openError) throw owner.openError;
            return {
                agentInfo: { name: `runtime-${name}`, version: "1" },
                list: async ({ cursor }) => {
                    calls.push([name, "list", cursor]);
                    if (owner.list) return owner.list({ cursor });
                    const page = owner.pages?.[cursor ?? "first"] ?? { sessions: owner.sessions ?? [] };
                    if (page instanceof Error) throw page;
                    return page;
                },
                load: async (id) => {
                    calls.push([name, "load", id]);
                    if (owner.load) return owner.load(id);
                    const result = owner.histories?.[id] ?? messages(`${name}:${id}`);
                    if (result instanceof Error) throw result;
                    return result;
                },
                close: async () => { closes.push(name); },
            };
        },
        ...options,
    });
    t.after(() => service.dispose());
    return { service, calls, closes };
};

test("scans every page and snapshots owner-qualified IDs for selected and all export", async (t) => {
    const original = messages("original");
    const { service, calls, closes } = fixture(t, {
        a: { pages: { first: { sessions: [session("same", { title: "first", primaryAgentId: "general", primaryAgentName: "General" })], nextCursor: "page2" }, page2: { sessions: [session("second")] } }, histories: { same: original } },
        b: { sessions: [session("same", { title: "other owner" })] },
    });
    const { jobId } = await service.startQuery({ agentIds: [], days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.equal(ready.processed, 3);
    assert.equal(ready.total, 3);
    assert.equal(ready.items.length, 3);
    assert.notEqual(ready.items[0].key, ready.items[2].key);
    assert.deepEqual(calls.filter(([, method]) => method === "list"), [["a", "list", undefined], ["a", "list", "page2"], ["b", "list", undefined]]);
    assert.deepEqual(closes, ["a", "b"]);
    original[0].content[0].text = "later live mutation";
    const selected = ready.items[0].key;
    assert.deepEqual(service.startExport({ jobId, keys: [selected, selected] }), { jobId });
    const first = await waitFor(service, jobId, "completed");
    assert.equal(first.document.format, "msinsight.agent-sessions.v1");
    assert.equal(first.document.sessions.length, 1);
    assert.deepEqual(first.document.sessions[0].owner, { agentId: "a" });
    assert.equal(first.document.sessions[0].agent.name, "runtime-a");
    assert.equal(first.document.sessions[0].session.primaryAgentName, "General");
    assert.equal(first.document.sessions[0].messages[0].content[0].text, "original");
    assert.equal(first.document.sessions[0].format, "msinsight.agent-session.v1");
    first.document.sessions[0].messages[0].content[0].text = "client mutation";
    service.startExport({ jobId });
    const all = await waitFor(service, jobId, "completed");
    assert.equal(all.document.sessions.length, 3);
    assert.notEqual(all.document.filename, first.document.filename);
    assert.equal(all.document.sessions[0].messages[0].content[0].text, "original");
    assert.equal(JSON.stringify(all).includes("must-not-leak"), false);
    assert.equal(calls.filter(([, method]) => method === "load").length, 3);
});

test("ANDs owner, inclusive rolling UTC endpoints, and title-only literal case-insensitive keyword before loading", async (t) => {
    const from = "2026-09-14T12:34:56.789Z";
    const { service, calls } = fixture(t, {
        chosen: {
            sessions: [session("from", { updatedAt: from, title: "A [NeEdLe] title" }), session("to", { title: "Boundary [needle]" }), session("old", { updatedAt: "2026-09-14T12:34:56.788Z" }), session("future", { updatedAt: "2026-09-15T12:34:56.790Z" }), session("unknown", { updatedAt: undefined }), session("local-time", { updatedAt: "2026-09-15T12:34:56" }), session("body-only"), session("tool-only"), session("system-only")],
            histories: {
                "body-only": [{ role: "user", content: [{ type: "text", text: "Contains [NEEDLE]" }] }, { role: "assistant", content: [{ type: "text", text: "[needle]" }] }],
                "tool-only": [{ role: "assistant", content: [{ type: "thinking", text: "[needle]" }, { type: "tool", toolCall: { output: "[needle]" } }] }],
                "system-only": [{ role: "system", content: [{ type: "text", text: "[needle]" }] }],
            },
        },
        ignored: { sessions: [session("ignore", { title: "[needle]" })] },
    });
    const { jobId } = await service.startQuery({ agentIds: ["chosen"], days: 1, keyword: "[needle]" });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["from", "to"]);
    assert.deepEqual(ready.filters, { agentIds: ["chosen"], days: 1, keyword: "[needle]", from, to: NOW, timestamp: "updatedAt" });
    assert.equal(ready.issues.filter(({ code }) => code === "unknown_updated_at").length, 2);
    assert.equal(calls.some(([agent]) => agent === "ignored"), false);
    assert.deepEqual(calls.filter(([, method]) => method === "load"), [["chosen", "load", "from"], ["chosen", "load", "to"]]);
    assert.equal(ready.total, 2);
    assert.equal(ready.processed, 2);
});

test("ANDs creation time, update time, agent, and keyword filters strictly", async (t) => {
    const createdInRange = "2026-10-20T12:00:00.000Z";
    const createdOutOfRange = "2026-10-01T12:00:00.000Z";
    const updatedInRange = "2026-10-28T00:00:00.000Z";
    const updatedOutOfRange = "2026-10-10T00:00:00.000Z";

    const { service } = fixture(t, {
        agent1: {
            sessions: [
                session("both-match", { createdAt: createdInRange, updatedAt: updatedInRange, title: "DeepSeek Target" }),
                session("created-only", { createdAt: createdInRange, updatedAt: updatedOutOfRange, title: "DeepSeek Bad Update" }),
                session("updated-only", { createdAt: createdOutOfRange, updatedAt: updatedInRange, title: "DeepSeek Bad Create" }),
                session("keyword-mismatch", { createdAt: createdInRange, updatedAt: updatedInRange, title: "Other Agent Target" }),
                session("neither-time", { createdAt: createdOutOfRange, updatedAt: updatedOutOfRange, title: "DeepSeek Neither" }),
            ],
        },
        agent2: {
            sessions: [
                session("wrong-agent", { createdAt: createdInRange, updatedAt: updatedInRange, title: "DeepSeek Target" }),
            ],
        },
    }, { now: () => new Date("2026-10-28T12:00:00.000Z") });

    // Filter requires agent1 AND created in last 10 days AND updated in last 2 days AND keyword "ds"
    const { jobId } = await service.startQuery({
        agentIds: ["agent1"],
        createdDays: 10,
        days: 2,
        keyword: "ds",
    });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["both-match"]);
    assert.equal(ready.items[0].createdAt, createdInRange);
    assert.equal(ready.items[0].updatedAt, updatedInRange);

    // Custom creation range + custom update range
    const { jobId: customJob } = await service.startQuery({
        agentIds: ["agent1"],
        createdFrom: "2026-10-19T00:00:00.000Z",
        createdTo: "2026-10-21T00:00:00.000Z",
        from: "2026-10-27T00:00:00.000Z",
        to: "2026-10-28T12:00:00.000Z",
        keyword: "deepseek",
    });
    const customReady = await waitFor(service, customJob, "ready");
    assert.deepEqual(customReady.items.map(({ sessionId }) => sessionId), ["both-match"]);
});

test("does not use updatedAt as a fallback for missing or invalid creation times", async (t) => {
    const { service, calls } = fixture(t, { a: { sessions: [
        session("missing"), session("invalid", { createdAt: "2026-09-31T12:00:00Z" }),
        session("known", { createdAt: NOW }),
    ] } });
    const filtered = await service.startQuery({ days: 1, createdDays: 1 });
    const ready = await waitFor(service, filtered.jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["known"]);
    assert.deepEqual(ready.issues.map(({ sessionId, code }) => [sessionId, code]), [
        ["missing", "unknown_created_at"], ["invalid", "unknown_created_at"],
    ]);
    assert.deepEqual(calls.filter(([, method]) => method === "load"), [["a", "load", "known"]]);

    const unfiltered = await service.startQuery({ days: 1 });
    const matches = await waitFor(service, unfiltered.jobId, "ready");
    assert.deepEqual(matches.items.map(({ createdAt }) => createdAt), [undefined, undefined, NOW]);
    assert.equal(matches.issues.length, 0);
    service.startExport(unfiltered);
    const { document } = await waitFor(service, unfiltered.jobId, "completed");
    assert.deepEqual(document.sessions.map(({ session: meta }) => meta.createdAt), [undefined, undefined, NOW]);
});

test("matches title substrings and ordered Unicode character subsequences with ANDed whitespace tokens", async (t) => {
    const { service, calls } = fixture(t, { a: { sessions: [
        session("deep", { title: "DeepSeek" }),
        session("reversed", { title: "SeekDeep" }),
        session("punctuation", { title: "issue .* [ok]" }),
        session("plain", { title: "issue any ok" }),
        session("unicode", { title: "CAFÉ 北京 𐐀" }),
        session("single-a", { title: "abc" }),
    ] } }, { config: { sessionBatchExport: { maxJobs: 20 } } });
    for (const [keyword, expected] of [
        ["DS", ["deep"]],
        ["sd", ["reversed"]],
        ["deep", ["deep", "reversed"]],
        ["  ds\tdeep  ", ["deep"]],
        ["deep ds", ["deep"]],
        ["ds missing", []],
        [".*", ["punctuation"]],
        ["[ok]", ["punctuation"]],
        ["é 北 𐐨", ["unicode"]],
        ["aa", []],
        [" \t ", ["deep", "reversed", "punctuation", "plain", "unicode", "single-a"]],
    ]) {
        const before = calls.length;
        const { jobId } = await service.startQuery({ days: 30, keyword });
        const ready = await waitFor(service, jobId, "ready");
        assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), expected, keyword);
        assert.deepEqual(calls.slice(before).filter(([, method]) => method === "load").map(([, , id]) => id), expected, keyword);
    }
});

test("resolves host-owned titles by owner before matching and keeps resolved titles in immutable snapshots", async (t) => {
    const titles = new Map([['["a","same"]', "Local DeepSeek"], ['["a","hidden"]', "Unrelated"], ['["b","same"]', "Another unrelated title"]]);
    const lookups = [];
    const { service, calls } = fixture(t, {
        a: { sessions: [session("same", { title: "Remote original" }), session("hidden", { title: "DeepSeek" })] },
        b: { sessions: [session("same", { title: "DeepSeek" })] },
    }, { resolveTitle: (agentId, sessionId, fallback) => {
        lookups.push([agentId, sessionId, fallback]);
        return titles.get(JSON.stringify([agentId, sessionId])) ?? fallback;
    } });
    const { jobId } = await service.startQuery({ days: 1, keyword: "ds" });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ agentId, sessionId, title }) => ({ agentId, sessionId, title })), [{ agentId: "a", sessionId: "same", title: "Local DeepSeek" }]);
    assert.deepEqual(lookups, [["a", "same", "Remote original"], ["a", "hidden", "DeepSeek"], ["b", "same", "DeepSeek"]]);
    assert.deepEqual(calls.filter(([, method]) => method === "load"), [["a", "load", "same"]]);
    titles.set('["a","same"]', "Changed after query");
    service.startExport({ jobId });
    const completed = await waitFor(service, jobId, "completed");
    assert.equal(completed.document.sessions[0].session.title, "Local DeepSeek");
    assert.deepEqual(completed.document.sessions[0].owner, { agentId: "a" });
});

test("title resolution failures preserve other snapshots and sanitized partial diagnostics", async (t) => {
    const { service, calls } = fixture(t, { a: { sessions: [session("bad"), session("good")] } }, {
        resolveTitle: (_owner, id, fallback) => { if (id === "bad") throw new Error("private title state"); return fallback; },
    });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["good"]);
    assert.equal(ready.issues[0].code, "title_resolution_failed");
    assert.equal(JSON.stringify(ready).includes("private"), false);
    assert.deepEqual(calls.filter(([, method]) => method === "load"), [["a", "load", "good"]]);
});

test("custom ranges normalize explicit offsets and include both endpoints without changing export time", async (t) => {
    const from = "2024-02-29T23:30:00+08:00";
    const to = "2024-03-01T00:30:00+08:00";
    const { service, calls } = fixture(t, { a: { sessions: [
        session("before", { updatedAt: "2024-02-29T15:29:59.999Z" }),
        session("from", { updatedAt: "2024-02-29T10:30:00-05:00" }),
        session("middle", { updatedAt: Date.parse("2024-02-29T16:00:00Z") }),
        session("sub-ms-history", { updatedAt: "2024-02-29T16:00:00.123456Z" }),
        session("to", { updatedAt: "2024-02-29T16:30:00Z" }),
        session("after", { updatedAt: "2024-02-29T16:30:00.001Z" }),
        session("invalid-calendar", { updatedAt: "2024-02-30T15:30:00Z" }),
        session("invalid-time", { updatedAt: "2024-02-29T24:00:00Z" }),
        session("invalid-offset", { updatedAt: "2024-02-29T15:30:00+24:00" }),
    ] } });
    const { jobId } = await service.startQuery({ from, to });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["from", "middle", "sub-ms-history", "to"]);
    assert.deepEqual(ready.filters, { agentIds: ["a"], keyword: "", from: "2024-02-29T15:30:00.000Z", to: "2024-02-29T16:30:00.000Z", timestamp: "updatedAt" });
    assert.equal(ready.issues.filter(({ code }) => code === "unknown_updated_at").length, 3);
    assert.deepEqual(calls.filter(([, method]) => method === "load").map(([, , id]) => id), ["from", "middle", "sub-ms-history", "to"]);
    service.startExport({ jobId });
    const completed = await waitFor(service, jobId, "completed");
    assert.equal(completed.document.exportedAt, NOW);
    assert.equal(completed.document.sessions[0].exportedAt, NOW);
    assert.deepEqual(completed.document.filters, ready.filters);
});

test("equal instants with different offsets are a valid zero-width closed range", async (t) => {
    const { service } = fixture(t, { a: { sessions: [
        session("before", { updatedAt: "2026-09-15T12:34:56.788Z" }),
        session("exact"),
        session("after", { updatedAt: "2026-09-15T12:34:56.790Z" }),
    ] } });
    const { jobId } = await service.startQuery({ from: "2026-09-15T20:34:56.789+08:00", to: NOW });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["exact"]);
    assert.equal(ready.filters.from, NOW);
    assert.equal(ready.filters.to, NOW);
});

test("accepts leap-year dates, RFC3339 offset limits and future custom ranges", async (t) => {
    const { service } = fixture(t, { a: { sessions: [session("future", { updatedAt: "2028-02-29T12:00:00Z" })] } });
    for (const value of ["2000-02-29T12:00:00Z", "2024-02-29t12:00:00z", "2024-02-29T12:00:00+23:59", "2024-02-29T12:00:00-23:59", "2024-02-29T12:00:00.1Z", "2024-02-29T12:00:00.12Z", "2024-02-29T12:00:00.123Z"]) {
        const { jobId } = await service.startQuery({ from: value, to: value });
        const ready = await waitFor(service, jobId, "ready");
        assert.equal(ready.filters.from, new Date(value).toISOString());
        assert.equal(ready.filters.to, ready.filters.from);
    }
    const { jobId } = await service.startQuery({ from: "2028-02-29T00:00:00Z", to: "2028-03-01T00:00:00Z" });
    assert.deepEqual((await waitFor(service, jobId, "ready")).items.map(({ sessionId }) => sessionId), ["future"]);
});

test("validates real calendar dates, times and explicit offsets at both custom range endpoints", async (t) => {
    const { service, calls } = fixture(t, { a: {} });
    for (const value of [
        "2026-02-29T12:00:00Z", "1900-02-29T12:00:00Z", "2100-02-29T12:00:00Z", "2024-02-30T12:00:00Z", "2024-04-31T12:00:00Z",
        "2024-00-01T12:00:00Z", "2024-13-01T12:00:00Z", "2024-01-00T12:00:00Z", "2024-01-32T12:00:00Z",
        "2024-01-01T24:00:00Z", "2024-01-01T12:60:00Z", "2024-01-01T12:00:60Z",
        "2024-01-01T12:00:00+24:00", "2024-01-01T12:00:00-24:00", "2024-01-01T12:00:00+01:60", "2024-01-01T12:00:00+0800",
        "2024-01-01T12:00:00.1234Z", "2024-01-01T12:00:00.0000Z", "2024-01-01T12:00:00.123456+08:00",
        "2024-01-01T12:00:00", "2024-01-01", "2024-01-01 12:00:00Z", "2024-01-01T12:00Z", " 2024-01-01T12:00:00Z", "2024-01-01T12:00:00Z ",
        "invalid", "", null, 0, {}, [],
    ]) {
        for (const input of [{ from: value, to: "9999-12-31T23:59:59Z" }, { from: "0001-01-01T00:00:00Z", to: value }]) {
            await assert.rejects(service.startQuery(input), { status: 400, message: /valid timestamps with an explicit timezone/ }, JSON.stringify(input));
        }
    }
    assert.deepEqual(calls, []);
});

test("rejects missing, incomplete, mixed and reversed time modes before opening sources", async (t) => {
    const { service, calls } = fixture(t, { a: {} });
    for (const input of [
        {}, { keyword: "ds" }, { from: NOW }, { to: NOW },
        { days: 1, from: NOW, to: NOW }, { days: 1, from: NOW }, { days: 1, to: NOW },
        { from: "2026-09-16T12:00:00Z", to: NOW },
        { from: "2026-09-15T12:00:00-01:00", to: "2026-09-15T12:00:00Z" },
    ]) await assert.rejects(service.startQuery(input), { status: 400 }, JSON.stringify(input));
    assert.deepEqual(calls, []);
});

test("keeps 1/7/30-day presets and legacy upper bound as fixed UTC rolling ranges", async (t) => {
    const { service } = fixture(t, { a: { sessions: [session("now")] } });
    for (const days of [1, 7, 30, 36500]) {
        const { jobId } = await service.startQuery({ days });
        const ready = await waitFor(service, jobId, "ready");
        assert.equal(ready.filters.days, days);
        assert.equal(ready.filters.from, new Date(Date.parse(NOW) - days * 86_400_000).toISOString());
        assert.equal(ready.filters.to, NOW);
        assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["now"]);
    }
});

for (const kind of ["failure", "repeat", "malformed"]) {
    test(`retains earlier page successes after ${kind} pagination`, async (t) => {
        const pages = { first: { sessions: [session("first")], nextCursor: "next" }, next: { sessions: [session("second")], nextCursor: "next" } };
        if (kind === "failure") pages.next = new Error("ENV_API_KEY=secret");
        if (kind === "malformed") pages.next.nextCursor = { unsafe: "cursor" };
        const { service, closes } = fixture(t, { a: { pages } });
        const { jobId } = await service.startQuery({ days: 1 });
        const ready = await waitFor(service, jobId, "ready");
        assert.equal(ready.items.length, kind === "failure" ? 1 : 2);
        assert.equal(ready.issues[0].code, { failure: "list_page_failed", repeat: "repeated_cursor", malformed: "malformed_cursor" }[kind]);
        assert.equal(JSON.stringify(ready).includes("ENV_API_KEY"), false);
        assert.deepEqual(closes, ["a"]);
    });
}

test("owner unsupported/list/load failures preserve other owners and explicit diagnostics", async (t) => {
    const { service, closes } = fixture(t, {
        unsupported: { openError: Object.assign(new Error("private config"), { code: "session_export_load_unsupported" }) },
        dead: { openError: new Error("secret env") },
        listFailed: { pages: { first: new Error("secret") } },
        partial: { sessions: [session("bad"), session("good"), session("busy")], histories: { bad: new Error("secret"), busy: Object.assign(new Error("private"), { code: "session_export_busy" }) } },
    });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["good"]);
    assert.equal(ready.processed, 3);
    assert.deepEqual(ready.issues.map(({ code }) => code), ["session_export_load_unsupported", "owner_unavailable", "list_page_failed", "load_failed", "session_export_busy"]);
    assert.deepEqual(closes, ["listFailed", "partial"]);
    assert.equal(/secret|private/.test(JSON.stringify(ready)), false);
    service.startExport({ jobId });
    const exported = await waitFor(service, jobId, "completed");
    assert.deepEqual(exported.document.issues, ready.issues);
});

test("reports real discovery, load and export progress without using chat events", async (t) => {
    const page = deferred();
    const first = deferred();
    const second = deferred();
    const { service } = fixture(t, { a: { list: () => page.promise, load: (id) => id === "one" ? first.promise : second.promise } });
    const { jobId } = await service.startQuery({ days: 1 });
    assert.equal(service.getStatus(jobId).total, null);
    assert.equal(service.getStatus(jobId).processed, 0);
    assert.throws(() => service.startExport({ jobId }), { status: 409 });
    page.resolve({ sessions: [session("one"), session("two")] });
    await yieldTurn();
    assert.equal(service.getStatus(jobId).total, 2);
    assert.equal(service.getStatus(jobId).processed, 0);
    first.resolve(messages());
    await yieldTurn();
    assert.equal(service.getStatus(jobId).processed, 1);
    second.resolve(messages());
    await waitFor(service, jobId, "ready");
    service.startExport({ jobId });
    assert.equal(service.getStatus(jobId).phase, "exporting");
    assert.equal(service.getStatus(jobId).processed, 0);
    assert.throws(() => service.startExport({ jobId }), { status: 409 });
    const completed = await waitFor(service, jobId, "completed");
    assert.equal(completed.processed, completed.total);
});

test("validates requests and no-match/empty selection without creating an empty artifact", async (t) => {
    const { service } = fixture(t, { a: { sessions: [session("one")] } });
    for (const days of [undefined, 0, 36501, 1.5, "1"]) await assert.rejects(service.startQuery({ days }), { status: 400 });
    for (const input of [null, [], { days: 1, agentIds: "a" }, { days: 1, keyword: 2 }, { days: 1, agentIds: ["unknown"] }]) await assert.rejects(service.startQuery(input), { status: 400 });
    assert.throws(() => service.getStatus("missing"), { status: 404 });
    assert.throws(() => service.startExport(null), { status: 400 });
    const { jobId } = await service.startQuery({ days: 1 });
    await waitFor(service, jobId, "ready");
    for (const input of [{ jobId, keys: [] }, { jobId, keys: ["one"] }, { jobId, persist: "yes" }]) assert.throws(() => service.startExport(input), { status: 400 });
    assert.equal(service.getStatus(jobId).phase, "ready");
    const empty = await service.startQuery({ days: 1, keyword: "not-found" });
    assert.deepEqual((await waitFor(service, empty.jobId, "ready")).items, []);
    assert.throws(() => service.startExport(empty), { status: 400 });
});

test("explicitly reports candidate, page, history and snapshot limits", async (t) => {
    const a = fixture(t, { a: { sessions: [session("one"), session("two")] } }, { config: { sessionBatchExport: { maxCandidates: 1 } } }).service;
    const first = await a.startQuery({ days: 1 });
    const result = await waitFor(a, first.jobId, "ready");
    assert.equal(result.items.length, 1);
    assert.equal(result.issues[0].code, "candidate_limit");
    const b = fixture(t, { a: { pages: { first: { sessions: [session("one")], nextCursor: "next" }, next: { sessions: [session("two")] } } } }, { config: { sessionBatchExport: { maxPagesPerAgent: 1 } } }).service;
    const pageJob = await b.startQuery({ days: 1 });
    assert.equal((await waitFor(b, pageJob.jobId, "ready")).issues[0].code, "page_limit");
    const c = fixture(t, { a: { sessions: [session("large"), session("small")], histories: { large: messages("x".repeat(2000)) } } }, { config: { sessionBatchExport: { maxHistoryBytes: 1024 } } }).service;
    const historyJob = await c.startQuery({ days: 1 });
    const history = await waitFor(c, historyJob.jobId, "ready");
    assert.equal(history.issues[0].code, "session_export_history_limit");
    assert.equal(history.items[0].sessionId, "small");
    const d = fixture(t, { a: { sessions: [session("large")], histories: { large: messages("x".repeat(2000)) } } }, { config: { sessionBatchExport: { maxSnapshotBytes: 1024 } } }).service;
    const snapshotJob = await d.startQuery({ days: 1 });
    assert.equal((await waitFor(d, snapshotJob.jobId, "ready")).issues[0].code, "snapshot_limit");
});

test("caps retained jobs, expires snapshots, and does not leak diagnostics beyond their cap", async (t) => {
    let time = Date.parse(NOW);
    const { service } = fixture(t, { a: { sessions: [session("one"), session("bad1", { updatedAt: null }), session("bad2", { updatedAt: null }), session("bad3", { updatedAt: null })] } }, { now: () => time, config: { sessionBatchExport: { maxJobs: 1, jobTtlMs: 100, maxIssues: 2 } } });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(ready.issues.map(({ code }) => code), ["unknown_updated_at", "issue_limit"]);
    await assert.rejects(service.startQuery({ days: 1 }), { status: 409 });
    time += 101;
    assert.throws(() => service.getStatus(jobId), { status: 404 });
    await service.startQuery({ days: 1 });
});

test("save failures retain the JSON document and permit retry; successful persistence is one unique batch", async (t) => {
    const directory = await mkdtemp(join(tmpdir(), "insight-batch-export-test-"));
    t.after(async () => {
        assert.equal(dirname(directory), resolve(tmpdir()));
        assert.ok(directory.startsWith(join(resolve(tmpdir()), "insight-batch-export-test-")));
        await rm(directory, { recursive: true, force: true });
    });
    const blocked = join(directory, "not-a-directory");
    await writeFile(blocked, "leave unchanged");
    const config = { sessionExportDir: blocked };
    const { service } = fixture(t, { a: { sessions: [session("one")] } }, { config });
    const { jobId } = await service.startQuery({ days: 1 });
    await waitFor(service, jobId, "ready");
    service.startExport({ jobId, persist: true });
    const failed = await waitFor(service, jobId, "failed");
    assert.equal(failed.document.sessions.length, 1);
    assert.ok(failed.error);
    assert.equal(await readFile(blocked, "utf8"), "leave unchanged");
    config.sessionExportDir = directory;
    service.startExport({ jobId, persist: true });
    const completed = await waitFor(service, jobId, "completed");
    const stored = JSON.parse(await readFile(completed.document.savedPath, "utf8"));
    assert.equal(stored.format, "msinsight.agent-sessions.v1");
    assert.equal(stored.sessions.length, 1);
    service.startExport({ jobId, persist: true });
    const another = await waitFor(service, jobId, "completed");
    assert.notEqual(another.document.savedPath, completed.document.savedPath);
    assert.deepEqual(JSON.parse(await readFile(completed.document.savedPath, "utf8")), stored);
});

test("times out a load explicitly, closes its source, and disposes in-flight work", async (t) => {
    const pending = deferred();
    const { service, closes } = fixture(t, { a: { sessions: [session("one")], load: () => pending.promise } }, { config: { sessionBatchExport: { operationTimeoutMs: 10 } } });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.equal(ready.issues[0].code, "session_export_timeout");
    assert.deepEqual(closes, ["a"]);
    const next = await service.startQuery({ days: 1 });
    await yieldTurn();
    await service.dispose();
    assert.throws(() => service.getStatus(next.jobId), { status: 409 });
    await assert.rejects(service.startQuery({ days: 1 }), { status: 409 });
    assert.throws(() => service.startExport({ jobId }), { status: 409 });
    pending.resolve(messages());
    await yieldTurn();
    assert.equal(closes.length, 2);
});

test("reports the remaining sessions skipped for a timed-out owner and still loads other owners", async (t) => {
    const pending = deferred();
    const { service, calls, closes } = fixture(t, {
        a: { sessions: [session("kept"), session("slow"), session("skip-1"), session("skip-2")], load: (id) => id === "slow" ? pending.promise : messages(id) },
        b: { sessions: [session("other")] },
    }, { config: { sessionBatchExport: { operationTimeoutMs: 25 } } });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.deepEqual(calls.filter(([, method]) => method === "load"), [["a", "load", "kept"], ["a", "load", "slow"], ["b", "load", "other"]]);
    assert.deepEqual(ready.items.map(({ sessionId }) => sessionId), ["kept", "other"]);
    assert.deepEqual(ready.issues.map(({ agentId, sessionId, code }) => [agentId, sessionId, code]), [
        ["a", "slow", "session_export_timeout"], ["a", undefined, "agent_load_aborted"],
    ]);
    assert.match(ready.issues[1].message, /Skipped 2 remaining sessions/);
    assert.equal(ready.processed, 5);
    assert.equal(ready.total, 5);
    assert.deepEqual(closes, ["a", "b"]);
    pending.resolve(messages());
});

test("late source opens after timeout or dispose are always closed", async () => {
    const opened = deferred();
    let closed = 0;
    const service = createSessionBatchExportService({ now: () => NOW, listAgents: () => [{ name: "a" }], openSource: () => opened.promise, config: { sessionBatchExport: { operationTimeoutMs: 10 } } });
    const { jobId } = await service.startQuery({ days: 1 });
    const ready = await waitFor(service, jobId, "ready");
    assert.equal(ready.issues[0].code, "session_export_timeout");
    await service.dispose();
    opened.resolve({ close: async () => { closed++; } });
    await delay(0);
    assert.equal(closed, 1);
});

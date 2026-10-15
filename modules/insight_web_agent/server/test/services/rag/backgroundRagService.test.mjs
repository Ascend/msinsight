/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import assert from "node:assert/strict";
import { once } from "node:events";
import { createServer } from "node:http";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { Worker } from "node:worker_threads";
import test from "node:test";
import { createBackgroundRagService } from "../../../services/rag/backgroundRagService.mjs";
import { createRagCapability } from "../../../capability-center/ragCapability.mjs";

const source = `
import { parentPort, workerData } from 'node:worker_threads';
const config = workerData.config;
parentPort.postMessage({ type: 'loading' });
const until = Date.now() + (config.delayMs ?? 0);
while (Date.now() < until) {}
if (config.mode === 'crash') throw new Error('private diagnostic text');
if (config.mode === 'exit') process.exit(7);
if (config.mode === 'fail') {
    parentPort.postMessage({ type: 'failed', reason: 'checksum_mismatch' });
    parentPort.close();
} else if (config.mode === 'disabled') {
    parentPort.postMessage({ type: 'status', status: { enabled: false, reason: 'pack_unreadable' } });
    parentPort.close();
} else {
    parentPort.postMessage({ type: 'status', status: { enabled: true, chunks: 2557 } });
    parentPort.on('message', (message) => {
        if (message.query === 'crash') process.exit(7);
        if (config.mode === 'hang') return;
        setTimeout(() => parentPort.postMessage({ type: 'result', id: message.id,
            ...(message.query === 'busy' ? { error: 'rag_busy' } : { result: { query: message.query } })
        }), config.replyDelayMs ?? 0);
    });
}
`;
const silent = { info() {}, warn() {}, debug() {} };

const fixture = async (t, config = {}, extra = {}) => {
    const root = await mkdtemp(join(tmpdir(), "insight-background-rag-"));
    const entry = join(root, "worker.mjs");
    await writeFile(entry, source);
    const service = createBackgroundRagService({ config, workerUrl: pathToFileURL(entry), logger: silent, ...extra });
    t.after(async () => { await service.close(); await rm(root, { recursive: true, force: true }); });
    return service;
};

test("CPU-heavy initialization does not block HTTP or expose an unvalidated service", async (t) => {
    let worker;
    const service = await fixture(t, { delayMs: 1500 }, {
        createWorker: (url, options) => { worker = new Worker(url, options); return worker; },
    });
    const ready = service.start();
    await once(worker, "message"); // Worker is actively loading, not merely scheduled.
    const server = createServer((_request, response) => response.end(JSON.stringify(service.getStatus())));
    server.listen(0, "127.0.0.1");
    await once(server, "listening");
    t.after(() => new Promise((resolve) => { server.closeAllConnections(); server.close(resolve); }));
    const began = performance.now();
    const response = await fetch(`http://127.0.0.1:${server.address().port}`, { signal: AbortSignal.timeout(1000) });
    assert.equal((await response.json()).reason, "initializing");
    assert.ok(performance.now() - began < 1000);
    const capability = createRagCapability({ ragService: service });
    assert.equal((await capability.execute({ query: "test" }, {})).reason, "initializing");
    assert.equal((await ready).phase, "ready");
    assert.deepEqual(await service.retrieve("validated"), { query: "validated" });
});

test("start is idempotent and disabled RAG does not create a worker", async (t) => {
    let count = 0;
    const service = await fixture(t, {}, { createWorker: (url, options) => { count++; return new Worker(url, options); } });
    assert.strictEqual(service.start(), service.start());
    await service.start();
    assert.equal(count, 1);
    const disabled = createBackgroundRagService({ config: { enabled: false }, createWorker: () => { throw new Error("must not start"); } });
    assert.deepEqual(await disabled.start(), { enabled: false, phase: "disabled", reason: "disabled" });
    await disabled.close();
});

test("load failures preserve fail-open and fail-closed policy", async (t) => {
    for (const failOpen of [true, false]) {
        let fatal = 0;
        const service = await fixture(t, { mode: "fail", failOpen }, { onFatal: () => fatal++ });
        assert.equal((await service.start()).reason, "checksum_mismatch");
        assert.equal(service.isEnabled(), false);
        assert.equal(fatal, failOpen ? 0 : 1);
        assert.equal(await service.retrieve("test"), undefined);
    }
});

test("a normally disabled worker may exit without being reported as a crash", async (t) => {
    const service = await fixture(t, { mode: "disabled" });
    assert.equal((await service.start()).phase, "disabled");
    await new Promise((resolve) => setTimeout(resolve, 30));
    assert.equal(service.getStatus().reason, "pack_unreadable");
});

for (const mode of ["crash", "exit"]) {
    test(`unexpected worker ${mode} reports a stable error without leaking diagnostics`, async (t) => {
        const service = await fixture(t, { mode });
        const status = await service.start();
        assert.equal(status.phase, "failed");
        assert.match(status.reason, /^rag_worker_(failed|exited)$/);
        assert.doesNotMatch(JSON.stringify(status), /private diagnostic/);
    });
}

test("worker construction failures are handled without unhandled rejections", async () => {
    const service = createBackgroundRagService({ logger: silent, createWorker() { throw new Error("failed to spawn"); } });
    assert.equal((await service.start()).reason, "rag_worker_start_failed");
    await service.close();
});

test("closing during initialization settles readiness and terminates the worker", async (t) => {
    let worker;
    const service = await fixture(t, { delayMs: 10000 }, { createWorker: (url, options) => { worker = new Worker(url, options); return worker; } });
    const ready = service.start();
    const exited = once(worker, "exit");
    await once(worker, "message");
    const closing = service.close();
    assert.strictEqual(closing, service.close());
    assert.equal((await ready).reason, "closed");
    await closing;
    await exited;
    assert.equal(service.getStatus().enabled, false);
});

test("closing rejects pending retrievals and busy errors retain their code", async (t) => {
    const busy = await fixture(t);
    await busy.start();
    await assert.rejects(busy.retrieve("busy"), { code: "rag_busy" });
    const service = await fixture(t, { mode: "hang" });
    await service.start();
    const request = assert.rejects(service.retrieve("pending"), { code: "rag_worker_closed" });
    await service.close();
    await request;
    await assert.rejects(service.retrieve("closed"), { code: "rag_worker_closed" });
});

test("AbortSignal is handled locally instead of being cloned into the worker", async (t) => {
    const service = await fixture(t, { replyDelayMs: 100 });
    await service.start();
    const controller = new AbortController();
    const rejected = assert.rejects(service.retrieve("cancel", { signal: controller.signal }), { name: "AbortError" });
    controller.abort();
    await rejected;
    await assert.rejects(service.retrieve("already cancelled", { signal: controller.signal }), { name: "AbortError" });
    assert.deepEqual(await service.retrieve("next"), { query: "next" });
});

test("a worker exiting during retrieval rejects the request and disables further use", async (t) => {
    const service = await fixture(t);
    await service.start();
    await assert.rejects(service.retrieve("crash"), { code: "rag_worker_exited" });
    assert.equal(service.getStatus().phase, "failed");
    assert.equal(await service.retrieve("after failure"), undefined);
});

test("the real worker reports invalid model configuration through the normal fail-open path", async (t) => {
    const service = createBackgroundRagService({ config: { enabled: true, failOpen: true }, logger: silent });
    t.after(() => service.close());
    const status = await service.start();
    assert.equal(status.phase, "disabled");
    assert.equal(status.reason, "model_directory_missing");
});

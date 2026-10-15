/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { Worker } from "node:worker_threads";

const stableReason = (value, fallback) => /^[a-z0-9_]{1,64}$/.test(String(value ?? "")) ? value : fallback;
const workerError = (code) => Object.assign(new Error(code), { code });

// The Host keeps a synchronous status facade; CPU-heavy loading and inference
// live in the worker, so listening and /api/state never wait for them.
export const createBackgroundRagService = ({
    config = {},
    workerUrl = new URL("../../rag-worker.mjs", import.meta.url),
    createWorker = (url, options) => new Worker(url, options),
    onStatus = () => {},
    onFatal = () => {},
    logger = console,
} = {}) => {
    let status = { enabled: false, phase: config.enabled === false ? "disabled" : "initializing", reason: config.enabled === false ? "disabled" : "initializing" };
    let worker;
    let closed = false;
    let startPromise;
    let resolveStart;
    let closePromise;
    let sequence = 0;
    const pending = new Map();

    const publish = (next) => {
        status = next;
        onStatus({ ...status });
        resolveStart?.({ ...status });
    };
    const rejectPending = (error) => {
        for (const request of pending.values()) request.reject(error);
        pending.clear();
    };
    const fail = (reason) => {
        if (closed || status.phase === "failed") return;
        const code = stableReason(reason, "rag_worker_failed");
        publish({ enabled: false, phase: "failed", reason: code });
        rejectPending(workerError(code));
        logger.warn?.(`RAG background service failed: ${code}`);
        if (config.failOpen === false) onFatal();
    };

    const start = () => {
        if (startPromise) return startPromise;
        if (closed || config.enabled === false) return Promise.resolve({ ...status });
        startPromise = new Promise((resolve) => { resolveStart = resolve; });
        const began = performance.now();
        logger.info?.("RAG background initialization started");
        try {
            worker = createWorker(workerUrl, { workerData: { config } });
            worker.on("message", (message) => {
                if (closed || status.phase === "failed") return;
                if (message.type === "log") {
                    if (["info", "warn", "debug"].includes(message.level)) logger[message.level]?.(message.message);
                } else if (message.type === "status") {
                    const next = message.status;
                    publish({ ...next, phase: next.enabled ? "ready" : "disabled" });
                    logger.info?.(`RAG background initialization completed: enabled=${Boolean(next.enabled)} elapsedMs=${Math.round(performance.now() - began)}`);
                } else if (message.type === "failed") {
                    fail(message.reason);
                } else if (message.type === "result") {
                    const request = pending.get(message.id);
                    if (!request) return;
                    if (message.error) request.reject(workerError(stableReason(message.error, "rag_retrieval_failed")));
                    else request.resolve(message.result);
                }
            });
            worker.on("error", () => fail("rag_worker_failed"));
            worker.on("exit", () => {
                worker = undefined;
                if (!closed && ["initializing", "ready"].includes(status.phase)) fail("rag_worker_exited");
            });
        } catch {
            fail("rag_worker_start_failed");
        }
        return startPromise;
    };

    const retrieve = (query, options = {}) => {
        if (closed) return Promise.reject(workerError("rag_worker_closed"));
        if (options.signal?.aborted) return Promise.reject(Object.assign(new Error("RAG request aborted"), { name: "AbortError" }));
        if (!status.enabled || !worker) return Promise.resolve(undefined);
        return new Promise((resolve, reject) => {
            const id = ++sequence;
            const finish = (callback, value) => {
                pending.delete(id);
                options.signal?.removeEventListener("abort", abort);
                callback(value);
            };
            const abort = () => finish(reject, Object.assign(new Error("RAG request aborted"), { name: "AbortError" }));
            pending.set(id, { resolve: (value) => finish(resolve, value), reject: (error) => finish(reject, error) });
            options.signal?.addEventListener("abort", abort, { once: true });
            try {
                worker.postMessage({ type: "retrieve", id, query, options: {
                    previousUserText: options.previousUserText,
                    propagateBusy: options.propagateBusy,
                } });
            } catch {
                finish(reject, workerError("rag_worker_send_failed"));
            }
        });
    };

    const close = () => {
        if (closePromise) return closePromise;
        closed = true;
        status = { enabled: false, phase: "disabled", reason: "closed" };
        resolveStart?.({ ...status });
        rejectPending(workerError("rag_worker_closed"));
        closePromise = worker ? worker.terminate().then(() => {}) : Promise.resolve();
        return closePromise;
    };

    return {
        start,
        isEnabled: () => status.enabled,
        getStatus: () => ({ ...status }),
        retrieve,
        close,
    };
};

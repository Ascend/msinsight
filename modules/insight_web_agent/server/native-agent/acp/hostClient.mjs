/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

const DEFAULT_TIMEOUT_MS = 5 * 60 * 1000;

/** 功能：创建 Native ACP 到宿主的 JSON-RPC 请求客户端，供权限等双向方法使用。 */
export const createAcpHostClient = ({ writeJson, timeoutMs = DEFAULT_TIMEOUT_MS }) => {
    const pending = new Map();
    let nextId = 1;

    const request = (method, params, { signal } = {}) => {
        const id = `native-host-${nextId++}`;
        const promise = new Promise((resolve, reject) => {
            const finish = (callback, value) => {
                const waiter = pending.get(id);
                if (!waiter) return;
                pending.delete(id);
                clearTimeout(waiter.timeout);
                signal?.removeEventListener("abort", waiter.abort);
                callback(value);
            };
            const abort = () => finish(reject, signal?.reason ?? new Error("ACP host request cancelled"));
            const timeout = setTimeout(() => finish(reject, new Error(`ACP host request timed out: ${method}`)), timeoutMs);
            pending.set(id, { resolve, reject, timeout, abort, signal });
            signal?.addEventListener("abort", abort, { once: true });
            if (signal?.aborted) abort();
        });
        if (pending.has(id)) writeJson({ jsonrpc: "2.0", id, method, params });
        return promise;
    };

    const handleResponse = (message) => {
        const waiter = pending.get(message?.id);
        if (!waiter) return false;
        pending.delete(message.id);
        clearTimeout(waiter.timeout);
        waiter.signal?.removeEventListener("abort", waiter.abort);
        if (message.error) waiter.reject(new Error(message.error.data?.details ?? message.error.message ?? "ACP host request failed"));
        else waiter.resolve(message.result ?? null);
        return true;
    };

    const close = (reason = "ACP host client closed") => {
        for (const waiter of pending.values()) {
            clearTimeout(waiter.timeout);
            waiter.signal?.removeEventListener("abort", waiter.abort);
            waiter.reject(new Error(reason));
        }
        pending.clear();
    };

    return { request, handleResponse, close };
};

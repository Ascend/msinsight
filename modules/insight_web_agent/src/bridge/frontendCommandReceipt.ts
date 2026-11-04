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
export interface FrontendCommandAck { ok?: boolean; settled?: boolean }
export interface ReceiptBody {
    requestId: string;
    claimToken: string;
    status: 'completed' | 'failed' | 'cancelled';
    result?: unknown;
    error?: unknown;
}
export interface PreparedReceipt { readonly requestId: string; readonly json: string }
const preparedReceipts = new WeakSet<object>();
export const isPreparedReceipt = (value: object): value is PreparedReceipt => preparedReceipts.has(value);

export const prepareReceipt = (body: ReceiptBody): PreparedReceipt => {
    let json: string;
    try {
        json = JSON.stringify(body);
    } catch (_error) {
        json = JSON.stringify({
            requestId: body.requestId,
            claimToken: body.claimToken,
            status: 'failed',
            error: {
                code: 'COMMAND_RESULT_INVALID',
                message: 'The command returned, but its receipt could not be serialized.',
                retryable: false,
                details: { stage: 'respond', outcome: body.status === 'completed' ? 'completed' : 'failed', reason: 'result_not_serializable' },
            },
        });
    }
    const receipt = Object.freeze({ requestId: body.requestId, json });
    preparedReceipts.add(receipt);
    return receipt;
};

const wait = (ms: number, signal: AbortSignal): Promise<void> => new Promise((resolve) => {
    const done = (): void => { clearTimeout(timer); signal.removeEventListener('abort', done); resolve(); };
    const timer = setTimeout(done, ms);
    signal.addEventListener('abort', done, { once: true });
    if (signal.aborted) done();
});

export const deliverReceipt = async (receipt: PreparedReceipt, {
    send, signal, deadline, now = Date.now, sleep = wait,
}: {
    send: (receipt: PreparedReceipt, options: { signal: AbortSignal }) => Promise<FrontendCommandAck>;
    signal: AbortSignal;
    deadline: number;
    now?: () => number;
    sleep?: (ms: number, signal: AbortSignal) => Promise<void>;
}): Promise<boolean> => {
    if (signal.aborted || now() >= deadline) return false;
    const controller = new AbortController();
    const abort = (): void => controller.abort();
    signal.addEventListener('abort', abort, { once: true });
    const timer = setTimeout(abort, Math.max(0, deadline - now()));
    try {
        for (let attempt = 0; attempt < 2; attempt += 1) {
            if (controller.signal.aborted || now() >= deadline) return false;
            try {
                const ack = await send(receipt, { signal: controller.signal });
                return !controller.signal.aborted && now() < deadline && ack?.ok === true && ack.settled !== false;
            } catch (error) {
                if (controller.signal.aborted || attempt === 1) return false;
                const status = error && typeof error === 'object' && 'status' in error ? error.status : undefined;
                const name = error instanceof Error ? error.name : undefined;
                if (name === 'AbortError' || (status !== undefined && ![500, 502, 503, 504].includes(Number(status)))) return false;
                const remaining = deadline - now();
                if (remaining <= 100) return false;
                await sleep(100, controller.signal);
            }
        }
        return false;
    } finally {
        clearTimeout(timer);
        signal.removeEventListener('abort', abort);
    }
};

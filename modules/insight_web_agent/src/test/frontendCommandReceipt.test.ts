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
import { prepareReceipt, deliverReceipt } from '../bridge/frontendCommandReceipt';
import { respondFrontendCommand } from '../api';

jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({}),
    ACP_STATUS: 'ready',
    ACP_NODE_VERSION: '',
}));
const body = { requestId: 'one', claimToken: 'owner', status: 'completed' as const, result: { changed: true } };
const run = (signal = new AbortController().signal, deadline = Date.now() + 5000): Promise<boolean> => deliverReceipt(
    prepareReceipt(body), { signal, deadline, send: respondFrontendCommand },
);
const response = (status: number, value: unknown): unknown => ({
    ok: status === 200, status, text: async () => JSON.stringify(value),
});
afterEach(() => jest.restoreAllMocks());

test('two actual fetches only, identical frozen body, no lower-layer retries', async () => {
    const fetchMock = jest.fn().mockRejectedValue(new TypeError('connection lost'));
    global.fetch = fetchMock;
    expect(await run()).toBe(false);
    expect(fetchMock).toHaveBeenCalledTimes(2);
    expect(fetchMock.mock.calls[0][1].body).toBe(fetchMock.mock.calls[1][1].body);
});

test.each([400, 401, 403, 409])('HTTP %s does not resend', async (status) => {
    global.fetch = jest.fn().mockResolvedValue(response(status, { error: 'rejected' }));
    expect(await run()).toBe(false);
    expect(fetch).toHaveBeenCalledTimes(1);
});

test.each([{ ok: true, settled: true }, { ok: true }])('accepts current and legacy acknowledgements %j', async (ack) => {
    global.fetch = jest.fn().mockResolvedValue(response(200, ack));
    expect(await run()).toBe(true);
    expect(fetch).toHaveBeenCalledTimes(1);
});

test('invalid ownership ack is not retried', async () => {
    global.fetch = jest.fn().mockResolvedValue(response(200, { ok: true, settled: false }));
    expect(await run()).toBe(false);
    expect(fetch).toHaveBeenCalledTimes(1);
});

test('transient HTTP failure resends the immutable result, never executes a command', async () => {
    const result = { ...body, result: { changed: true } };
    const prepared = prepareReceipt(result);
    result.result.changed = false;
    global.fetch = jest.fn().mockResolvedValueOnce(response(503, {})).mockResolvedValue(response(200, { ok: true, settled: true }));
    expect(await deliverReceipt(prepared, { signal: new AbortController().signal, deadline: Date.now() + 1000, send: respondFrontendCommand })).toBe(true);
    expect(fetch).toHaveBeenCalledTimes(2);
    expect(JSON.parse((fetch as jest.Mock).mock.calls[1][1].body).result.changed).toBe(true);
});

test('serialization failure reports respond/completed, not execution failure', () => {
    const circular: Record<string, unknown> = {};
    circular.self = circular;
    const prepared = prepareReceipt({ ...body, result: circular });
    expect(JSON.parse(prepared.json)).toEqual(expect.objectContaining({
        status: 'failed',
        error: expect.objectContaining({
            code: 'COMMAND_RESULT_INVALID', details: expect.objectContaining({ stage: 'respond', outcome: 'completed' }),
        }),
    }));
});

test('pre-cancel and expired deadline never send; in-flight cancellation aborts fetch without retry', async () => {
    const controller = new AbortController();
    controller.abort();
    global.fetch = jest.fn();
    expect(await run(controller.signal)).toBe(false);
    expect(await run(undefined, Date.now() - 1)).toBe(false);
    expect(fetch).not.toHaveBeenCalled();
    const active = new AbortController();
    global.fetch = jest.fn((_url, init) => new Promise((_resolve, reject) => {
        init?.signal?.addEventListener('abort', () => reject(new DOMException('Cancelled', 'AbortError')));
    }));
    const delivery = run(active.signal);
    await Promise.resolve();
    active.abort();
    expect(await delivery).toBe(false);
    expect(fetch).toHaveBeenCalledTimes(1);
});

test('the receipt deadline aborts an in-flight fetch and prevents resend', async () => {
    global.fetch = jest.fn((_url, init) => new Promise((_resolve, reject) => {
        init?.signal?.addEventListener('abort', () => reject(new DOMException('Timeout', 'AbortError')));
    }));
    expect(await run(undefined, Date.now() + 30)).toBe(false);
    expect(fetch).toHaveBeenCalledTimes(1);
});

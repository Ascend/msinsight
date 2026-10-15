/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({}),
    ACP_STATUS: 'starting',
    ACP_NODE_VERSION: '22.14.0',
    ACP_STARTUP_STATUS_URL: 'http://localhost/acp-startup-status',
}));

beforeEach(() => {
    jest.resetModules();
    jest.useFakeTimers('modern');
});

afterEach(() => {
    jest.useRealTimers();
    jest.restoreAllMocks();
});

const advanceSeconds = async (seconds: number): Promise<void> => {
    for (let second = 0; second < seconds; second += 1) {
        for (let task = 0; task < 10; task += 1) {
            await Promise.resolve();
        }
        jest.advanceTimersByTime(1000);
    }
};

test('waits for a slow startup without reporting a failure, then returns the initial data', async () => {
    const { fetchState } = await import('../api');
    const { subscribeBackendUnavailable } = await import('../backendConnection');
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    const readyAt = Date.now() + 20_000;
    global.fetch = jest.fn(async (url: string) => {
        if (url.endsWith('/acp-startup-status')) {
            return { ok: true, json: async () => ({ status: Date.now() < readyAt ? 'starting' : 'ready' }) };
        }
        return { ok: true, status: 200, text: async () => '{"activeAgentName":"test"}' };
    }) as unknown as typeof fetch;

    const request = fetchState();
    await advanceSeconds(21);
    await expect(request).resolves.toEqual({ activeAgentName: 'test' });
    expect(listener.mock.calls.every(([failure]) => failure === undefined)).toBe(true);
    unsubscribe();
});

test('concurrent requests keep loading beyond 60 seconds until the launcher reports readiness', async () => {
    const { fetchState } = await import('../api');
    const { subscribeBackendUnavailable } = await import('../backendConnection');
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    let status = 'starting';
    const fetchMock = jest.fn(async (url: string) => {
        if (url.endsWith('/acp-startup-status')) {
            return { ok: true, json: async () => ({ status }) };
        }
        return { ok: true, status: 200, text: async () => '{}' };
    });
    global.fetch = fetchMock as unknown as typeof fetch;

    const first = fetchState();
    await advanceSeconds(30);
    expect(listener).toHaveBeenCalledTimes(1);
    const callsBeforeSecond = fetchMock.mock.calls.length;
    const second = fetchState();
    await advanceSeconds(1);
    expect(fetchMock.mock.calls.length - callsBeforeSecond).toBe(1);
    await advanceSeconds(60);
    expect(listener).toHaveBeenCalledTimes(1);
    expect(fetchMock.mock.calls.every(([url]) => url.endsWith('/acp-startup-status'))).toBe(true);
    status = 'ready';
    await advanceSeconds(2);
    await expect(first).resolves.toEqual({});
    await expect(second).resolves.toEqual({});
    expect(listener).toHaveBeenCalledTimes(1);
    unsubscribe();
});

test('reports startup failure when the launcher reports that the child process exited', async () => {
    const { fetchState } = await import('../api');
    const { subscribeBackendUnavailable } = await import('../backendConnection');
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    const fetchMock = jest.fn().mockResolvedValue({ ok: true, json: async () => ({ status: 'start-failed' }) });
    global.fetch = fetchMock;

    await expect(fetchState()).rejects.toThrow(/start-failed/);
    expect(fetchMock).toHaveBeenCalledTimes(1);
    expect(listener).toHaveBeenLastCalledWith(expect.objectContaining({ status: 'start-failed' }));
    unsubscribe();
});

test('does not interpret a failed launcher status request as a Node startup failure', async () => {
    const { fetchState } = await import('../api');
    const { subscribeBackendUnavailable } = await import('../backendConnection');
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    global.fetch = jest.fn()
        .mockRejectedValueOnce(new TypeError('Failed to fetch'))
        .mockResolvedValueOnce({ ok: true, json: async () => ({ status: 'ready' }) })
        .mockResolvedValueOnce({ ok: true, status: 200, text: async () => '{}' });

    const request = fetchState();
    await advanceSeconds(3);
    await expect(request).resolves.toEqual({});
    expect(listener).toHaveBeenCalledTimes(1);
    unsubscribe();
});

test('uses ordinary connection error handling after the launcher reports readiness', async () => {
    const { fetchState } = await import('../api');
    const { subscribeBackendUnavailable } = await import('../backendConnection');
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    const fetchMock = jest.fn()
        .mockResolvedValueOnce({ ok: true, json: async () => ({ status: 'ready' }) })
        .mockResolvedValue({ ok: true, status: 200, text: async () => '{}' });
    global.fetch = fetchMock;
    await fetchState();

    fetchMock.mockRejectedValue(new TypeError('Failed to fetch'));
    const request = expect(fetchState()).rejects.toThrow(/Unable to connect/);
    await advanceSeconds(5);
    await request;
    expect(listener).toHaveBeenLastCalledWith(expect.objectContaining({ status: 'unreachable' }));
    unsubscribe();
});

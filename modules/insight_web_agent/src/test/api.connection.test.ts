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
    ACP_STATUS: 'ready',
    ACP_NODE_VERSION: '',
}));

import { fetchState } from '../api';
import { reportBackendAvailable, subscribeBackendUnavailable } from '../backendConnection';

afterEach(() => {
    reportBackendAvailable();
    jest.restoreAllMocks();
});

test('retries a connection failure before asking the UI to treat the backend as down', async () => {
    const fetchMock = jest.fn()
        .mockRejectedValueOnce(new TypeError('Failed to fetch'))
        .mockResolvedValueOnce({
            ok: true,
            status: 200,
            text: async () => '{}',
        });
    global.fetch = fetchMock as unknown as typeof fetch;

    await fetchState();

    expect(fetchMock).toHaveBeenCalledTimes(2);
});

test('closes the unavailable state after a later request succeeds', async () => {
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    const fetchMock = jest.fn()
        .mockRejectedValueOnce(new TypeError('Failed to fetch'))
        .mockRejectedValueOnce(new TypeError('Failed to fetch'))
        .mockRejectedValueOnce(new TypeError('Failed to fetch'))
        .mockRejectedValueOnce(new TypeError('Failed to fetch'));
    global.fetch = fetchMock as unknown as typeof fetch;

    await expect(fetchState()).rejects.toThrow(/Unable to connect to the Agent backend/);
    expect(listener).toHaveBeenCalledWith(expect.objectContaining({ status: 'unreachable' }));

    fetchMock.mockReset();
    fetchMock.mockResolvedValue({
        ok: true,
        status: 200,
        text: async () => '{}',
    });
    await fetchState();
    expect(listener).toHaveBeenLastCalledWith(undefined);
    unsubscribe();
});

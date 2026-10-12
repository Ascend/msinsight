/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

jest.mock('../env', () => ({
    apiUrl: (path: string) => path,
    capabilityAuthHeaders: () => ({}),
    eventsUrl: () => '/api/events',
    ACP_STATUS: 'missing-node',
    ACP_NODE_VERSION: '',
}));

import { fetchState } from '../api';
import { clearBackendConnectionFailure, subscribeBackendUnavailable } from '../backendConnection';

afterEach(() => {
    clearBackendConnectionFailure();
    jest.restoreAllMocks();
});

test('does not probe wry when Node.js is missing, and keeps the unavailable state', async () => {
    const listener = jest.fn();
    const unsubscribe = subscribeBackendUnavailable(listener);
    const fetchMock = jest.fn();
    global.fetch = fetchMock as unknown as typeof fetch;

    await expect(fetchState()).rejects.toThrow(/missing-node/);
    expect(fetchMock).not.toHaveBeenCalled();
    expect(listener).toHaveBeenCalledWith(expect.objectContaining({ status: 'missing-node' }));

    unsubscribe();
});

/*
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See http://license.coscl.org.cn/MulanPSL2.
 */
jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({ Authorization: 'Bearer rename-test' }),
    ACP_STATUS: 'ready', ACP_NODE_VERSION: '',
}));
import { renameSession } from '../api';

const originalFetch = global.fetch;
const fetchMock = jest.fn();
beforeEach(() => { global.fetch = fetchMock as unknown as typeof fetch; fetchMock.mockReset(); });
afterAll(() => { global.fetch = originalFetch; });

test('renames only the explicitly identified owner and session with authentication', async () => {
    const response = { ok: true, agentId: 'Owner A', sessionId: 'same', title: 'DeepSeek renamed' };
    fetchMock.mockResolvedValue({ ok: true, status: 200, text: async () => JSON.stringify(response) });
    await expect(renameSession('same', 'DeepSeek renamed', 'Owner A')).resolves.toEqual(response);
    expect(fetchMock).toHaveBeenCalledWith('http://127.0.0.1:9090/api/sessions/rename', {
        method: 'POST', body: JSON.stringify({ sessionId: 'same', title: 'DeepSeek renamed', agentId: 'Owner A' }),
        headers: { Authorization: 'Bearer rename-test', 'content-type': 'application/json', 'Accept-Language': 'en-US' },
    });
});

test('save errors remain errors instead of reporting a successful local rename', async () => {
    fetchMock.mockResolvedValue({ ok: false, status: 500, text: async () => JSON.stringify({ error: 'session_rename_failed', message: 'Could not save title' }) });
    await expect(renameSession('same', 'New title', 'Owner A')).rejects.toMatchObject({ status: 500, message: 'Could not save the session title.' });
});

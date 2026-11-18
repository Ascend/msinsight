/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * -------------------------------------------------------------------------
 */
jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({ Authorization: 'Bearer export-test' }),
    ACP_STATUS: 'ready',
    ACP_NODE_VERSION: '',
}));

import { exportSession, exportSessionBatch, fetchSessionExportStatus, querySessionExports } from '../api';

const originalFetch = global.fetch;
const fetchMock = jest.fn();
beforeEach(() => {
    fetchMock.mockReset().mockResolvedValue({ ok: true, status: 200, text: async () => '{"jobId":"job-1"}' });
    global.fetch = fetchMock as unknown as typeof fetch;
});
afterAll(() => { global.fetch = originalFetch; });

test('authenticates query and sends owner names, literal keyword, integer days and cancellation', async () => {
    const signal = new AbortController().signal;
    const filters = { agentIds: ['Owner A', 'Owner B'], days: 7, keyword: 'LITERAL .*' };
    await expect(querySessionExports(filters, signal)).resolves.toEqual({ jobId: 'job-1' });
    expect(fetchMock).toHaveBeenCalledWith('http://127.0.0.1:9090/api/sessions/export/query', {
        method: 'POST', body: JSON.stringify(filters), signal,
        headers: { Authorization: 'Bearer export-test', 'content-type': 'application/json', 'Accept-Language': 'en-US' },
    });
});

test('sends explicit timezone-qualified custom ranges unchanged without adding days', async () => {
    const filters = { agentIds: [], from: '2026-10-01T00:00:00+08:00', to: '2026-10-28T18:00:00+08:00', keyword: 'ds memory' };
    await querySessionExports(filters);
    expect(JSON.parse(fetchMock.mock.calls[0][1].body)).toEqual(filters);
    expect(JSON.parse(fetchMock.mock.calls[0][1].body)).not.toHaveProperty('days');
});

test('encodes job IDs and passes cancellation to authenticated status reads', async () => {
    const signal = new AbortController().signal;
    await fetchSessionExportStatus('job? &/中文', signal);
    expect(fetchMock).toHaveBeenCalledWith(`http://127.0.0.1:9090/api/sessions/export/status?${new URLSearchParams({ jobId: 'job? &/中文' })}`, {
        signal, headers: { Authorization: 'Bearer export-test', 'Accept-Language': 'en-US' },
    });
});

test('omits keys for all snapshot matches and preserves selected opaque keys', async () => {
    const signal = new AbortController().signal;
    await exportSessionBatch({ jobId: 'job-1', persist: false }, signal);
    await exportSessionBatch({ jobId: 'job-1', keys: ['owner/session-key'], persist: true }, signal);
    expect(fetchMock.mock.calls[0][0]).toBe('http://127.0.0.1:9090/api/sessions/export/batch');
    expect(JSON.parse(fetchMock.mock.calls[0][1].body)).toEqual({ jobId: 'job-1', persist: false });
    expect(JSON.parse(fetchMock.mock.calls[1][1].body)).toEqual({ jobId: 'job-1', keys: ['owner/session-key'], persist: true });
    expect(fetchMock.mock.calls[1][1].headers.Authorization).toBe('Bearer export-test');
    expect(fetchMock.mock.calls[1][1].signal).toBe(signal);
});

test('preserves the existing single export endpoint and persistence contract', async () => {
    await exportSession('current-session', true);
    expect(fetchMock.mock.calls[0][0]).toBe('http://127.0.0.1:9090/api/sessions/export');
    expect(JSON.parse(fetchMock.mock.calls[0][1].body)).toEqual({ sessionId: 'current-session', persist: true });
});

test('reports an expired export snapshot via the localized HTTP error contract', async () => {
    fetchMock.mockResolvedValueOnce({ ok: false, status: 404, text: async () => '{"code":"session_export_job_expired"}' });
    await expect(exportSessionBatch({ jobId: 'job-1', persist: true })).rejects.toMatchObject({
        status: 404,
        body: { code: 'session_export_job_expired' },
        message: 'The export job was not found or has expired. Search again to continue.',
    });
});

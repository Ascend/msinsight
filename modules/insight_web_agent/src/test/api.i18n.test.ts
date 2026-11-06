/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import i18n from '@insight/lib/i18n';
import { ApiRequestError, fetchState } from '../api';
import { apiErrorMessage } from '../apiErrorMessage';

jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({ authorization: 'Bearer test-token' }),
    ACP_STATUS: 'ready',
    ACP_NODE_VERSION: '',
}));

afterEach(async () => {
    await i18n.changeLanguage('enUS');
    jest.restoreAllMocks();
});

test('requests send the current language while preserving authentication and headers', async () => {
    global.fetch = jest.fn().mockResolvedValue({ ok: true, status: 200, text: async () => '{}' });
    await i18n.changeLanguage('zhCN');
    await fetchState();
    expect(fetch).toHaveBeenLastCalledWith(expect.any(String), expect.objectContaining({ headers: expect.objectContaining({
        'Accept-Language': 'zh-CN', authorization: 'Bearer test-token',
    }) }));
    await i18n.changeLanguage('enUS');
    await fetchState();
    expect(fetch).toHaveBeenLastCalledWith(expect.any(String), expect.objectContaining({ headers: expect.objectContaining({
        'Accept-Language': 'en-US',
    }) }));
});

test('display formatting follows language changes while Error message and stack remain stable', async () => {
    const body = { code: 'duplicate_agent_launch', message: 'Raw server message', conflictingAgent: 'OpenCode',
        details: { cause: 'Raw provider error' }, saved: true };
    const error = new ApiRequestError(409, '/api/agent-config/servers', body);
    expect(error.message).toBe('ACP launch command duplicates configured agent: OpenCode');
    const originalStack = error.stack;
    await i18n.changeLanguage('zhCN');
    expect(apiErrorMessage(error.body)).toBe('ACP 启动命令与已配置的 Agent 重复：OpenCode');
    expect(error.message).toBe('ACP launch command duplicates configured agent: OpenCode');
    expect(error.stack).toBe(originalStack);
    expect(Object.getOwnPropertyDescriptor(error, 'message')?.writable).toBe(true);
    error.message = 'replacement';
    expect(error.message).toBe('replacement');
    expect(apiErrorMessage(error.body)).toBe('ACP 启动命令与已配置的 Agent 重复：OpenCode');
    expect(error.body).toEqual(body);
    expect(error.status).toBe(409);
});

test('validation subcodes and unknown error fallback are translated', async () => {
    await i18n.changeLanguage('zhCN');
    const error = new ApiRequestError(400, '/api/agent-config/builtin', {
        code: 'validation_failed', details: [{ code: 'model_required', field: 'builtinAgent.model' }],
    });
    expect(error.message).toBe('请输入模型名称。');
    const unknown = new ApiRequestError(500, '/api/future', { code: 'future_error', message: 'Raw English error' });
    expect(unknown.message).toBe('请求失败，请重试。');
});

test('legacy HTTP status errors are localized again when the display language changes', async () => {
    const error = new ApiRequestError(401, '/mcp', { code: 'http_401', error: 'Unauthorized' });
    expect(apiErrorMessage(error.body)).toBe('Authentication failed. Reopen the Agent page or check the access token.');
    await i18n.changeLanguage('zhCN');
    expect(apiErrorMessage(error.body)).toBe('身份验证失败，请重新打开 Agent 页面或检查访问凭证。');
});

test('HTTP failures retain a localizable descriptor and malformed responses use a localized message', async () => {
    await i18n.changeLanguage('zhCN');
    global.fetch = jest.fn().mockResolvedValue({ ok: false, status: 502,
        text: async () => JSON.stringify({ code: 'model_list_failed', message: 'Raw English', details: 'Raw cause' }) });
    await expect(fetchState()).rejects.toThrow('无法获取模型列表，请检查配置，或通过 JSON 手动填写模型。');
    global.fetch = jest.fn().mockResolvedValue({ ok: false, status: 404, text: async () => '<html>Not found</html>' });
    await expect(fetchState()).rejects.toThrow('后台返回的数据不是有效的 JSON（HTTP 404）。');
});

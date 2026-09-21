/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { fetchAgentConfig, saveAgentServersConfig, saveBuiltinAgentConfig } from '../api';
import { clearBackendConnectionFailure } from '../backendConnection';
import { sealEnvSecrets, sealSecretInput } from '../secretSeal';

jest.mock('../env', () => ({
    apiUrl: (path: string) => `http://127.0.0.1:9090${path}`,
    capabilityAuthHeaders: () => ({ Authorization: 'Bearer test-token' }),
    eventsUrl: () => 'http://127.0.0.1:9090/api/events',
    ACP_STATUS: 'ready',
    ACP_NODE_VERSION: '',
}));

jest.mock('../secretSeal');

const sealed = { v: 1 as const, alg: 'rsa-oaep-aes-gcm-v1', ek: 'e', n: 'n', ct: 'c' };
const cryptoConfig = { alg: 'rsa-oaep-aes-gcm-v1', publicKey: 'public' };

afterEach(() => {
    clearBackendConnectionFailure();
    jest.restoreAllMocks();
});

test('sends capability tokens as Bearer headers and seals secrets before save', async () => {
    (sealSecretInput as jest.Mock).mockImplementation(async (value) => (
        value === 'plain-key' ? sealed : value
    ));
    (sealEnvSecrets as jest.Mock).mockImplementation(async (env) => ({
        ...env,
        TOKEN: env.TOKEN === 'plain-token' ? sealed : env.TOKEN,
    }));

    const fetchMock = jest.fn(async (url: string, init?: RequestInit) => {
        if (String(url).endsWith('/api/agent-config') && init?.method !== 'PUT') {
            return {
                ok: true,
                status: 200,
                text: async () => JSON.stringify({
                    snapshot: {
                        builtinAgent: { apiKey: '******' },
                        crypto: cryptoConfig,
                    },
                }),
            };
        }
        return {
            ok: true,
            status: 200,
            text: async () => JSON.stringify({ ok: true }),
        };
    });
    global.fetch = fetchMock as unknown as typeof fetch;

    await fetchAgentConfig();
    await saveBuiltinAgentConfig({
        schemaVersion: 1,
        name: 'msinsight-native',
        provider: 'openai',
        model: 'gpt',
        baseUrl: 'http://127.0.0.1',
        apiKey: 'plain-key',
    });
    await saveAgentServersConfig({
        activeAgentName: 'Claude',
        agentServers: [{ name: 'Claude', command: 'claude', args: [], env: { TOKEN: 'plain-token' } }],
    });

    const putBodies = fetchMock.mock.calls
        .filter(([, init]) => init?.method === 'PUT')
        .map(([, init]) => JSON.parse(String(init?.body)));
    expect(fetchMock.mock.calls[0][1]?.headers).toEqual(expect.objectContaining({
        Authorization: 'Bearer test-token',
    }));
    expect(putBodies).toEqual(expect.arrayContaining([
        expect.objectContaining({ apiKey: sealed }),
        expect.objectContaining({
            agentServers: [expect.objectContaining({ env: expect.objectContaining({ TOKEN: sealed }) })],
        }),
    ]));
});

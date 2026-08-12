/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

beforeAll(() => {
    window.__ACP_API_BASE__ = 'http://127.0.0.1:9090';
    window.history.replaceState({}, '', '/?capabilityToken=test');
});

afterAll(() => {
    delete window.__ACP_API_BASE__;
});

test('packaged Wry frontend connects ACP over IPv4 loopback', () => {
    const { resolveAcpPortBase } = require('../env');

    const macAndLinuxBase = resolveAcpPortBase('9090', {
        protocol: 'wry:',
        host: 'localhost',
        hostname: 'localhost',
        pathname: '/resources/profiler/frontend/index.html',
    }, false);
    const windowsBase = resolveAcpPortBase('9090', {
        protocol: 'http:',
        host: 'wry.localhost',
        hostname: 'wry.localhost',
        pathname: '/resources/profiler/frontend/index.html',
    }, false);

    expect(macAndLinuxBase).toBe('http://127.0.0.1:9090');
    expect(windowsBase).toBe('http://127.0.0.1:9090');
});

test('Jupyter proxy resolution keeps its host and base path', () => {
    const { resolveAcpPortBase } = require('../env');

    expect(resolveAcpPortBase('9090', {
        protocol: 'https:',
        host: 'jupyter.example.com',
        hostname: 'jupyter.example.com',
        pathname: '/user/demo/proxy/9000/resources/profiler/frontend/index.html',
    }, true)).toBe('https://jupyter.example.com/user/demo/proxy/9090');
});

test('local development uses its configured capability token', () => {
    const { resolveCapabilityToken } = require('../env');

    expect(resolveCapabilityToken('', 'development', 'local-token')).toBe('local-token');
    expect(resolveCapabilityToken('?capabilityToken=', 'development', 'local-token')).toBe('local-token');
    expect(resolveCapabilityToken('?capabilityToken=runtime-token', 'development', 'local-token')).toBe('runtime-token');
    expect(resolveCapabilityToken('', 'production', 'local-token')).toBe('');
});

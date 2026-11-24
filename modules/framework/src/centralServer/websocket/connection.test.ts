/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import { Connection } from './connection';
import { store } from '../../store';

jest.mock('@/connection', () => ({ send: jest.fn() }), { virtual: true });
jest.mock('@/utils/enum', () => ({ ProjectType: {} }), { virtual: true });
jest.mock('antd', () => ({ Modal: { error: jest.fn() } }));
jest.mock('@insight/lib/utils', () => ({ errorCenter: { handleError: jest.fn() } }), { virtual: true });
jest.mock('@insight/lib/i18n', () => ({ t: (key: string): string => key }), { virtual: true });
jest.mock('../server', () => ({ connectRemote: jest.fn() }));
jest.mock('../../store', () => ({
    store: { sessionStore: { activeSession: { activeDataSource: { projectName: 'project' } } } },
}));
jest.mock('@/vscode-adapter/WebviewSocket', () => ({}), { virtual: true });
jest.mock('@/vscode-adapter', () => ({ isVscodeEnv: (): boolean => false }), { virtual: true });

describe('websocket response dispatch', () => {
    let connection: Connection;
    let warnSpy: jest.SpyInstance;
    let send: jest.Mock;

    beforeEach(() => {
        send = jest.fn();
        jest.spyOn(window, 'WebSocket').mockImplementation(() => ({
            readyState: WebSocket.OPEN,
            send,
            onmessage: null,
        }) as unknown as WebSocket);
        jest.spyOn(console, 'info').mockImplementation(() => {});
        warnSpy = jest.spyOn(console, 'warn').mockImplementation(() => {});
        connection = new Connection({ remote: 'localhost', port: 9000 });
    });

    afterEach(() => {
        jest.restoreAllMocks();
    });

    const respond = (command: string, requestId: number, result = true, body = {}): MessageEvent<string> =>
        new MessageEvent('message', { data: JSON.stringify({ type: 'response', command, requestId, result, body }) });

    it('accepts successful heartbeat replies without a business callback', () => {
        connection.fetchDataOnMessage(respond('heartCheck', 7));
        connection.fetchDataOnMessage(respond('heartCheck', 8));
        expect(warnSpy).not.toHaveBeenCalled();
    });

    it('still reports unexpected responses and unsuccessful heartbeats', () => {
        connection.fetchDataOnMessage(respond('other/command', 7));
        connection.fetchDataOnMessage(respond('heartCheck', 8, false));
        expect(warnSpy).toHaveBeenNthCalledWith(1, 'handler for msg #7 not found');
        expect(warnSpy).toHaveBeenNthCalledWith(2, 'handler for msg #8 not found');
    });

    it('keeps pending business requests intact when a heartbeat arrives', async () => {
        const pending = connection.fetch('global', { command: 'test/query' });
        const request = JSON.parse(send.mock.calls[0][0]) as { id: number };
        connection.fetchDataOnMessage(respond('heartCheck', request.id + 1));
        connection.fetchDataOnMessage(respond('test/query', request.id, true, { value: 42 }));
        await expect(pending).resolves.toEqual({ value: 42 });
        expect(warnSpy).not.toHaveBeenCalled();
    });

    it('dispatches explicitly requested heartbeats to their registered callback', async () => {
        const pending = connection.fetch('global', { command: 'heartCheck' });
        const request = JSON.parse(send.mock.calls[0][0]) as { id: number };
        connection.fetchDataOnMessage(respond('heartCheck', request.id));
        await expect(pending).resolves.toEqual({});
        expect(warnSpy).not.toHaveBeenCalled();
    });
});

describe('development backend address', () => {
    const environment = { ...process.env };
    let socket: jest.SpyInstance;

    beforeEach(() => {
        process.env = { ...environment, NODE_ENV: 'development', REACT_APP_BACKEND_HOST: '192.0.2.44' };
        delete process.env.REACT_APP_IS_VSCODE;
        window.history.replaceState({}, '', '/?port=9100');
        socket = jest.spyOn(window, 'WebSocket').mockImplementation(() => ({}) as WebSocket);
        jest.spyOn(console, 'info').mockImplementation(() => {});
    });

    afterEach(() => {
        process.env = { ...environment };
        window.history.replaceState({}, '', '/');
        jest.restoreAllMocks();
    });

    it('routes the connection and iframe backend address to the configured development host', () => {
        expect(new Connection({ remote: '192.0.2.44', port: 9100 })).toBeInstanceOf(Connection);
        expect(socket).toHaveBeenCalledWith('ws://192.0.2.44:9100?port=9100');
        expect(store.sessionStore.activeSession.toIframeUrl).toBe('ws://192.0.2.44:9100');
        jest.isolateModules(() => {
            const { GLOBAL_HOST } = jest.requireActual<typeof import('./defs')>('./defs');
            expect(GLOBAL_HOST).toEqual({ remote: '192.0.2.44', port: 9100 });
        });
    });

    it.each(['', '   '])('preserves page-host routing when the override is empty (%s)', host => {
        process.env.REACT_APP_BACKEND_HOST = host;
        expect(new Connection({ remote: 'localhost', port: 9100 })).toBeInstanceOf(Connection);
        expect(socket).toHaveBeenCalledWith(`ws://${window.location.hostname}:9100?port=9100`);
    });

    it('ignores a development host in production builds', () => {
        process.env = { ...process.env, NODE_ENV: 'production' };
        expect(new Connection({ remote: 'localhost', port: 9100 })).toBeInstanceOf(Connection);
        expect(socket).toHaveBeenCalledWith(`ws://${window.location.hostname}:9100?port=9100`);
        jest.isolateModules(() => {
            const { GLOBAL_HOST } = jest.requireActual<typeof import('./defs')>('./defs');
            expect(GLOBAL_HOST.remote).toBe('localhost');
        });
    });

    it('does not apply the host override to VSCode configuration', () => {
        process.env.REACT_APP_IS_VSCODE = 'true';
        jest.isolateModules(() => {
            const { GLOBAL_HOST, getDevelopmentBackendHost } = jest.requireActual<typeof import('./defs')>('./defs');
            expect(getDevelopmentBackendHost()).toBe('');
            expect(GLOBAL_HOST.remote).toBe('localhost');
        });
    });

    it('preserves proxy and JupyterLab URLs with a development override configured', () => {
        window.history.replaceState({}, '', '/proxy/9000/index.html?port=9100');
        expect(new Connection({ remote: 'localhost', port: 9100 })).toBeInstanceOf(Connection);
        expect(socket).toHaveBeenLastCalledWith(`ws://${window.location.host}/proxy/9100/index.html?port=9100`);
        window.history.replaceState({}, '', '/resources/profiler/frontend/index.html?port=9100&jupyterlabProxy=true');
        expect(new Connection({ remote: 'localhost', port: 9100, jupyterlabProxy: true })).toBeInstanceOf(Connection);
        expect(socket).toHaveBeenLastCalledWith(`ws://${window.location.host}/proxy/9100/resources/profiler/frontend/index.html?port=9100&jupyterlabProxy=true`);
        jest.isolateModules(() => {
            const { GLOBAL_HOST } = jest.requireActual<typeof import('./defs')>('./defs');
            expect(GLOBAL_HOST.remote).toBe('localhost');
        });
    });
});

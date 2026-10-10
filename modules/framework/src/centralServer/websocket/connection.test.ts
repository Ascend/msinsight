/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import { Connection } from './connection';

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

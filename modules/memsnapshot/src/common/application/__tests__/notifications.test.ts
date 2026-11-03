/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { createMemorySession } from '../../entity/createMemorySession';
import { createNotificationHandlers, subscribeNotifications } from '../notifications';
import connector from '../connector';

jest.mock('../MemoryConnector.ts', () => ({
    MemoryConnector: jest.fn().mockImplementation(() => ({
        send: jest.fn(), addListener: jest.fn(event => ({ event, sequence: 0 })), removeListener: jest.fn(),
    })),
}));
jest.mock('@insight/lib/i18n', () => ({ __esModule: true, default: { changeLanguage: jest.fn() } }), { virtual: true });
jest.mock('@insight/lib', () => ({ errorCenter: { handleError: jest.fn() }, ErrorCode: { PARSE_FAIL: 'PARSE_FAIL' }, WsError: class extends Error {} }), { virtual: true });

describe('application notification lifecycle', () => {
    beforeEach(() => {
        (connector.addListener as jest.Mock).mockImplementation(event => ({ event, sequence: 0 }));
    });
    it('updates only owned fields and rejects another feature module', () => {
        const session = createMemorySession({ threadIds: [] as number[] }, 'leaks');
        const reconcile = jest.fn();
        const handlers = createNotificationHandlers({ session, requests: [], reset: jest.fn(), dispose: jest.fn(), reconcile, events: {} });
        handlers.updateSession({ threadIds: [42], snapshotSlices: {}, __proto__: { injected: true } });
        expect(session.threadIds).toEqual([42]);
        expect('snapshotSlices' in session).toBe(false);
        expect('injected' in session).toBe(false);
        handlers.updateSession({ module: 'memsnapshot', threadIds: [99] });
        expect(session.module).toBe('leaks');
        expect(session.threadIds).toEqual([42]);
        expect(reconcile).toHaveBeenCalledTimes(1);
    });
    it('unsubscribes exactly the registered listeners', () => {
        const dispose = subscribeNotifications({ first: jest.fn(), second: jest.fn() });
        dispose();
        expect(connector.removeListener).toHaveBeenCalledWith({ event: 'first', sequence: 0 });
        expect(connector.removeListener).toHaveBeenCalledWith({ event: 'second', sequence: 0 });
    });
    it('responds with the feature parse state and resets on removal', () => {
        const reset = jest.fn();
        const session = createMemorySession({}, 'memsnapshot');
        const handlers = createNotificationHandlers({ session, requests: [], reset, dispose: jest.fn(), isParsing: () => true, events: {} });
        handlers.getLeaksParseStatus({});
        expect(connector.send).toHaveBeenCalledWith({ event: 'leaksParseStatus', body: { complete: false } });
        handlers['remote/remove']({});
        expect(reset).toHaveBeenCalledTimes(1);
    });
});

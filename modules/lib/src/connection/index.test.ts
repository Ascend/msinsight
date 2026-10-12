/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { ACP_MESSAGE_CHANNEL } from '../FrontendAgentCommand';
import { withWindowMessageChannel } from '../WindowMessageRouter';
import { ClientConnector } from './index';

describe('ClientConnector message routing', () => {
    test('receives legacy messages without receiving explicit Agent channels', () => {
        const connector = new ClientConnector({ module: 'test' });
        const listener = jest.fn();
        connector.addListener('setTheme', listener);

        window.dispatchEvent(new MessageEvent('message', {
            data: JSON.stringify({ event: 'setTheme', body: { isDark: true } }),
        }));
        window.dispatchEvent(new MessageEvent('message', {
            data: withWindowMessageChannel(ACP_MESSAGE_CHANNEL, {
                event: 'setTheme',
                body: { isDark: false },
            }),
        }));

        expect(listener).toHaveBeenCalledTimes(1);
        expect(listener.mock.calls[0][0].data).toEqual({
            event: 'setTheme',
            body: { isDark: true },
        });
        connector.dispose();
    });

    test('rejects multiple Connectors in the same Window', () => {
        const connector = new ClientConnector({ module: 'first' });
        try {
            expect(() => new ClientConnector({ module: 'second' })).toThrow('only one Connector');
        } finally {
            connector.dispose();
        }
    });
});

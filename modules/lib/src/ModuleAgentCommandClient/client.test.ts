/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    MODULE_AGENT_COMMAND_RESPONSE,
    MODULE_AGENT_COMMANDS_CHANGED,
    MODULE_AGENT_EXECUTE_COMMAND,
    MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL,
    MODULE_AGENT_READY,
} from '../FrontendAgentCommand';
import { withWindowMessageChannel } from '../WindowMessageRouter';
import { ModuleAgentCommandClient } from './client';

const PARENT_ORIGIN = 'https://framework.test';

describe('ModuleAgentCommandClient', () => {
    test('publishes a full snapshot and ignores stale connection tokens', async () => {
        const parentDescriptor = Object.getOwnPropertyDescriptor(window, 'parent');
        const parent = { postMessage: jest.fn() } as unknown as Window;
        Object.defineProperty(window, 'parent', { configurable: true, value: parent });
        const handler = jest.fn(() => ({ ok: true }));
        const client = new ModuleAgentCommandClient({
            moduleId: 'MemScope',
            parentOrigin: PARENT_ORIGIN,
            observe: () => ({}),
        });
        client.registerCommand({
            name: 'MemScope.table.refresh',
            title: 'Refresh table',
            description: 'Refresh the current table.',
            inputSchema: { type: 'object', properties: {}, additionalProperties: false },
        }, handler);
        const stop = client.start();

        try {
            dispatchFromParent({
                event: MODULE_AGENT_HELLO,
                moduleId: 'MemScope',
                connectionToken: 'token-1',
            }, parent);
            await Promise.resolve();

            expect(parent.postMessage).toHaveBeenCalledWith(expect.objectContaining({
                event: MODULE_AGENT_READY,
                connectionToken: 'token-1',
            }), PARENT_ORIGIN);
            expect(parent.postMessage).toHaveBeenCalledWith(expect.objectContaining({
                event: MODULE_AGENT_COMMANDS_CHANGED,
                connectionToken: 'token-1',
                commands: [expect.objectContaining({ name: 'MemScope.table.refresh' })],
            }), PARENT_ORIGIN);

            dispatchFromParent({
                event: MODULE_AGENT_HELLO,
                moduleId: 'MemScope',
                connectionToken: 'token-2',
            }, parent);
            await Promise.resolve();
            dispatchFromParent(executeMessage('token-1', 'stale-request'), parent);
            dispatchFromParent(executeMessage('token-2', 'current-request'), parent);
            await Promise.resolve();
            await Promise.resolve();

            expect(handler).toHaveBeenCalledTimes(1);
            expect(parent.postMessage).toHaveBeenCalledWith(expect.objectContaining({
                event: MODULE_AGENT_COMMAND_RESPONSE,
                connectionToken: 'token-2',
                requestId: 'current-request',
                result: { ok: true },
            }), PARENT_ORIGIN);
            expect(parent.postMessage).not.toHaveBeenCalledWith(expect.objectContaining({
                event: MODULE_AGENT_COMMAND_RESPONSE,
                requestId: 'stale-request',
            }), PARENT_ORIGIN);
        } finally {
            stop();
            if (parentDescriptor) Object.defineProperty(window, 'parent', parentDescriptor);
            else delete (window as unknown as { parent?: Window }).parent;
        }
    });
});

const executeMessage = (connectionToken: string, requestId: string): object => ({
    event: MODULE_AGENT_EXECUTE_COMMAND,
    moduleId: 'MemScope',
    connectionToken,
    requestId,
    command: 'MemScope.table.refresh',
    args: {},
    deadline: Date.now() + 5000,
});

const dispatchFromParent = (message: object, parent: Window): void => {
    window.dispatchEvent(new MessageEvent('message', {
        data: withWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL, message),
        source: parent,
        origin: PARENT_ORIGIN,
    }));
};

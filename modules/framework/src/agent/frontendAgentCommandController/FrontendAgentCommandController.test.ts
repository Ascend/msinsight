/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    COMMAND_ERROR_CODES,
    FRONTEND_AGENT_EXECUTE_COMMAND,
    MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL,
    MODULE_AGENT_READY,
    type CommandDefinition,
    type FrontendAgentExecuteCommandMessage,
} from '@insight/lib/FrontendAgentCommand';
import { withWindowMessageChannel } from '@insight/lib/WindowMessageRouter';
import { FrontendAgentCommandController } from './FrontendAgentCommandController';
import { ModuleFrameTransport } from './ModuleFrameTransport';

const execute = (
    controller: FrontendAgentCommandController,
    command: string,
    args: FrontendAgentExecuteCommandMessage['args'] = {},
): Promise<unknown> => (controller as unknown as {
    execute: (message: FrontendAgentExecuteCommandMessage) => Promise<unknown>;
}).execute({
    event: FRONTEND_AGENT_EXECUTE_COMMAND,
    requestId: `request-${command}`,
    command,
    args,
    deadline: Date.now() + 1000,
});

// Only the command attribution logic is under test, so inject a transport stub instead of a real iframe and postMessage.
const attachFakeModule = (controller: FrontendAgentCommandController, moduleId: string): void => {
    (controller as unknown as { moduleTransports: Map<string, { dispose: () => void }> })
        .moduleTransports.set(moduleId, { dispose: () => undefined });
};

const registerCommands = (controller: FrontendAgentCommandController, moduleId: string, names: string[]): void => {
    const definitions: CommandDefinition[] = names.map(name => ({
        name,
        title: name,
        description: name,
        inputSchema: { type: 'object', properties: {}, additionalProperties: false },
    }));
    (controller as unknown as { catalog: { replaceModule: (id: string, definitions: CommandDefinition[]) => void } })
        .catalog.replaceModule(moduleId, definitions);
};

describe('ModuleFrameTransport HELLO retry', () => {
    let frame: HTMLIFrameElement;
    let transport: ModuleFrameTransport;
    let postMessage: jest.Mock;
    let onDisconnect: jest.Mock;
    let onCommandsChanged: jest.Mock;
    let randomUUIDDescriptor: PropertyDescriptor | undefined;

    const helloAt = (index: number): { event: string; connectionToken: string } => postMessage.mock.calls[index][0];
    const dispatchReady = (connectionToken: string, source = frame.contentWindow, origin = 'https://module.test'): void => {
        window.dispatchEvent(new MessageEvent('message', {
            data: withWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL, {
                event: MODULE_AGENT_READY,
                moduleId: 'Compute',
                connectionToken,
            }),
            source,
            origin,
        }));
    };

    beforeEach(() => {
        jest.useFakeTimers();
        randomUUIDDescriptor = Object.getOwnPropertyDescriptor(crypto, 'randomUUID');
        let nextToken = 0;
        Object.defineProperty(crypto, 'randomUUID', {
            configurable: true,
            value: jest.fn(() => `token-${++nextToken}`),
        });
        frame = document.createElement('iframe');
        frame.src = 'https://module.test/';
        postMessage = jest.fn();
        Object.defineProperty(frame, 'contentWindow', { value: { postMessage } });
        onDisconnect = jest.fn();
        onCommandsChanged = jest.fn();
        transport = new ModuleFrameTransport({ moduleId: 'Compute', frame, onDisconnect, onCommandsChanged });
    });

    afterEach(() => {
        transport.dispose();
        jest.useRealTimers();
        if (randomUUIDDescriptor) Object.defineProperty(crypto, 'randomUUID', randomUUIDDescriptor);
        else delete (crypto as unknown as { randomUUID?: unknown }).randomUUID;
    });

    test('retries with capped backoff and the same token without reconnecting until READY', () => {
        expect(helloAt(0)).toMatchObject({ event: MODULE_AGENT_HELLO, connectionToken: 'token-1' });
        let count = 1;
        for (const delay of [250, 500, 1000, 2000, 4000, 4000]) {
            jest.advanceTimersByTime(delay - 1);
            expect(postMessage).toHaveBeenCalledTimes(count);
            jest.advanceTimersByTime(1);
            expect(postMessage).toHaveBeenCalledTimes(++count);
            expect(postMessage.mock.calls[count - 1]).toEqual(postMessage.mock.calls[0]);
            expect(jest.getTimerCount()).toBe(1);
        }
        expect(onDisconnect).toHaveBeenCalledTimes(1);
        expect(onCommandsChanged).not.toHaveBeenCalled();
        expect(crypto.randomUUID).toHaveBeenCalledTimes(1);
        dispatchReady(helloAt(0).connectionToken);
        expect(jest.getTimerCount()).toBe(0);
        jest.advanceTimersByTime(10000);
        expect(postMessage).toHaveBeenCalledTimes(count);
    });

    test('matching READY clears the retry before its deadline', () => {
        jest.advanceTimersByTime(100);
        dispatchReady(helloAt(0).connectionToken);
        expect(jest.getTimerCount()).toBe(0);
        jest.advanceTimersByTime(2000);
        expect(postMessage).toHaveBeenCalledTimes(1);
    });

    test('load clears the old timer and restarts backoff for the new token', () => {
        jest.advanceTimersByTime(100);
        frame.dispatchEvent(new Event('load'));
        expect(postMessage).toHaveBeenCalledTimes(2);
        expect(helloAt(1).connectionToken).not.toBe(helloAt(0).connectionToken);
        expect(jest.getTimerCount()).toBe(1);
        jest.advanceTimersByTime(249);
        expect(postMessage).toHaveBeenCalledTimes(2);
        jest.advanceTimersByTime(1);
        expect(postMessage).toHaveBeenCalledTimes(3);
        expect(postMessage.mock.calls[2]).toEqual(postMessage.mock.calls[1]);
        expect(onDisconnect).toHaveBeenCalledTimes(2);
        expect(jest.getTimerCount()).toBe(1);
    });

    test('dispose clears the timer and ignores subsequent loads', () => {
        transport.dispose();
        transport.dispose();
        expect(jest.getTimerCount()).toBe(0);
        frame.dispatchEvent(new Event('load'));
        jest.advanceTimersByTime(2000);
        expect(postMessage).toHaveBeenCalledTimes(1);
        expect(onDisconnect).toHaveBeenCalledTimes(2);
    });

    test('stale READY cannot stop the current token retry', () => {
        const staleToken = helloAt(0).connectionToken;
        frame.dispatchEvent(new Event('load'));
        dispatchReady(staleToken);
        expect(jest.getTimerCount()).toBe(1);
        jest.advanceTimersByTime(250);
        expect(postMessage).toHaveBeenCalledTimes(3);
        expect(postMessage.mock.calls[2]).toEqual(postMessage.mock.calls[1]);
    });

    test('READY from the wrong source or origin cannot stop the retry', () => {
        const token = helloAt(0).connectionToken;
        dispatchReady(token, window);
        dispatchReady(token, frame.contentWindow, 'https://untrusted.test');
        jest.advanceTimersByTime(250);
        expect(postMessage).toHaveBeenCalledTimes(2);
    });
});

describe('FrontendAgentCommandController command diagnostics', () => {
    test('help preserves the inactive module diagnostic without switching or executing its command', async () => {
        const controller = new FrontendAgentCommandController();
        const transportExecute = jest.fn();
        registerCommands(controller, 'Timeline', ['Timeline.zoom']);
        (controller as unknown as { moduleTransports: Map<string, unknown> }).moduleTransports.set('Timeline', {
            execute: transportExecute,
            dispose: () => undefined,
        });
        controller.setActiveModule('Compute');

        try {
            await expect(execute(controller, 'help', { command: 'Timeline.zoom' })).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                retryable: true,
                details: {
                    reason: 'module_not_active', command: 'Timeline.zoom', targetModule: 'Timeline', activeModule: 'Compute',
                },
            });
            expect(transportExecute).not.toHaveBeenCalled();
            expect((controller as unknown as { activeModule: string }).activeModule).toBe('Compute');
        } finally {
            controller.dispose();
        }
    });

    test('help keeps the generic unavailable error for an unknown command', async () => {
        const controller = new FrontendAgentCommandController();
        controller.setActiveModule('Compute');

        try {
            const error = await execute(controller, 'help', { command: 'Unknown.query' }).catch(value => value);
            expect(error).toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                message: "Command 'Unknown.query' is unavailable. Use help with empty args to discover the current fully qualified command names.",
                retryable: false,
            });
            expect(error).toHaveProperty('details', undefined);
        } finally {
            controller.dispose();
        }
    });

    test('explains when the active Module has not registered with the Agent framework', async () => {
        const controller = new FrontendAgentCommandController();
        attachFakeModule(controller, 'Compute');
        controller.setActiveModule('Compute');

        try {
            await expect(execute(controller, 'Compute.query')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                details: { reason: 'module_not_registered', moduleId: 'Compute' },
            });
            await expect(execute(controller, 'Compute.query')).rejects.toThrow(
                "module 'Compute' has not registered any commands with the Agent framework",
            );
        } finally {
            controller.dispose();
        }
    });

    test('distinguishes an inactive Module command from an unknown command', async () => {
        const controller = new FrontendAgentCommandController();
        registerCommands(controller, 'Timeline', ['Timeline.zoom']);
        controller.setActiveModule('Compute');

        try {
            await expect(execute(controller, 'Timeline.zoom')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.UNAVAILABLE,
                details: { reason: 'module_not_active', targetModule: 'Timeline', activeModule: 'Compute' },
            });
            await expect(execute(controller, 'Unknown.query')).rejects.toMatchObject({
                code: COMMAND_ERROR_CODES.NOT_FOUND,
                details: { reason: 'command_not_found' },
            });
        } finally {
            controller.dispose();
        }
    });
});

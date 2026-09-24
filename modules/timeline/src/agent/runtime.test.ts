/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { MODULE_AGENT_READY } from '@insight/lib/FrontendAgentCommand';
import { startTimelineAgentRuntime, stopTimelineAgentRuntime } from './runtime';

describe('Timeline agent runtime', () => {
    test('starts the client and answers the framework hello handshake', () => {
        const parent = { postMessage: jest.fn() } as unknown as Window;
        Object.defineProperty(window, 'parent', { configurable: true, value: parent });

        startTimelineAgentRuntime();
        window.dispatchEvent(new MessageEvent('message', {
            source: parent as Window,
            origin: window.location.origin,
            data: {
                channel: 'moduleAgentMessage',
                event: 'moduleAgent/hello',
                moduleId: 'Timeline',
                connectionToken: 'token-1',
            },
        }));

        expect(parent.postMessage).toHaveBeenCalledWith(expect.objectContaining({
            event: MODULE_AGENT_READY,
            connectionToken: 'token-1',
        }), window.location.origin);
    });

    test('stop disconnects the client so the handshake is ignored', () => {
        const parent = { postMessage: jest.fn() } as unknown as Window;
        Object.defineProperty(window, 'parent', { configurable: true, value: parent });

        startTimelineAgentRuntime();
        stopTimelineAgentRuntime();
        window.dispatchEvent(new MessageEvent('message', {
            source: parent as Window,
            origin: window.location.origin,
            data: {
                channel: 'moduleAgentMessage',
                event: 'moduleAgent/hello',
                moduleId: 'Timeline',
                connectionToken: 'token-1',
            },
        }));

        expect(parent.postMessage).not.toHaveBeenCalled();
    });

    test('start is idempotent while active', () => {
        const parent = { postMessage: jest.fn() } as unknown as Window;
        Object.defineProperty(window, 'parent', { configurable: true, value: parent });

        startTimelineAgentRuntime();
        startTimelineAgentRuntime();
        startTimelineAgentRuntime();
        window.dispatchEvent(new MessageEvent('message', {
            source: parent as Window,
            origin: window.location.origin,
            data: {
                channel: 'moduleAgentMessage',
                event: 'moduleAgent/hello',
                moduleId: 'Timeline',
                connectionToken: 'token-1',
            },
        }));

        expect(parent.postMessage).toHaveBeenCalledTimes(1);
    });
});

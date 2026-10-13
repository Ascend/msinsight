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
    MODULE_AGENT_HELLO,
    MODULE_AGENT_MESSAGE_CHANNEL,
    MODULE_AGENT_READY,
} from '@insight/lib/FrontendAgentCommand';
import { withWindowMessageChannel } from '@insight/lib/WindowMessageRouter';
import { ModuleFrameTransport } from './ModuleFrameTransport';

const MODULE_ID = 'MemScope';
const READY_RETRY_WINDOW_MS = 60000;

interface PostedMessage {
    message: Record<string, unknown>;
    targetOrigin: string;
}

const createFrame = (): {
    frame: HTMLIFrameElement;
    contentWindow: Window;
    posted: PostedMessage[];
} => {
    const frame = document.createElement('iframe');
    frame.src = `/plugins/${MODULE_ID}/index.html`;
    const posted: PostedMessage[] = [];
    const contentWindow = {
        postMessage: (message: Record<string, unknown>, targetOrigin: string): void => {
            posted.push({ message, targetOrigin });
        },
    } as unknown as Window;
    // jsdom 不实现 iframe 的真实加载，这里只替身 contentWindow，消息收发由用例手工驱动。
    Object.defineProperty(frame, 'contentWindow', { value: contentWindow, configurable: true });
    return { frame, contentWindow, posted };
};

const attach = (): {
    transport: ModuleFrameTransport;
    frame: HTMLIFrameElement;
    contentWindow: Window;
    posted: PostedMessage[];
    onDisconnect: jest.Mock;
} => {
    const { frame, contentWindow, posted } = createFrame();
    const onDisconnect = jest.fn();
    const transport = new ModuleFrameTransport({
        moduleId: MODULE_ID,
        frame,
        onCommandsChanged: jest.fn(),
        onDisconnect,
    });
    return { transport, frame, contentWindow, posted, onDisconnect };
};

const helloMessages = (posted: PostedMessage[]): Array<Record<string, unknown>> => posted
    .map(({ message }) => message)
    .filter(message => message.event === MODULE_AGENT_HELLO);

const receive = (contentWindow: Window, message: Record<string, unknown>): void => {
    window.dispatchEvent(new MessageEvent('message', {
        data: withWindowMessageChannel(MODULE_AGENT_MESSAGE_CHANNEL, message),
        origin: window.location.origin,
        source: contentWindow,
    }));
};

const readyFor = (token: unknown): Record<string, unknown> => ({
    event: MODULE_AGENT_READY,
    moduleId: MODULE_ID,
    connectionToken: token,
});

describe('ModuleFrameTransport handshake retry', () => {
    beforeAll(() => {
        // jest 的 jsdom 环境没有 globalThis.crypto，浏览器与打包后的 WebView 都有，这里补确定性替身。
        if (typeof globalThis.crypto === 'undefined') {
            Object.defineProperty(globalThis, 'crypto', { value: {}, configurable: true });
        }
        if (typeof globalThis.crypto.randomUUID !== 'function') {
            let sequence = 0;
            Object.defineProperty(globalThis.crypto, 'randomUUID', {
                value: (): string => `generation-${++sequence}`,
                configurable: true,
            });
        }
    });

    beforeEach(() => {
        jest.useFakeTimers();
    });

    afterEach(() => {
        jest.useRealTimers();
    });

    test('keeps re-sending hello with the same token until the module answers ready', () => {
        const { transport, posted } = attach();
        try {
            // attach 时的第一次 hello 发生在 Module runtime 注册监听之前，会丢失。
            expect(helloMessages(posted)).toHaveLength(1);
            const token = helloMessages(posted)[0].connectionToken;

            jest.advanceTimersByTime(250);
            expect(helloMessages(posted)).toHaveLength(2);
            jest.advanceTimersByTime(500);
            expect(helloMessages(posted)).toHaveLength(3);
            jest.advanceTimersByTime(1000);
            expect(helloMessages(posted)).toHaveLength(4);
            // 重发必须沿用同一 connectionToken，否则 Module 会认为连接代次变化并中止在跑的命令。
            expect(helloMessages(posted).every(message => message.connectionToken === token)).toBe(true);
            expect(helloMessages(posted).every(message => message.moduleId === MODULE_ID)).toBe(true);
        } finally {
            transport.dispose();
        }
    });

    test('stops retrying after the module reports ready and keeps serving requests', async () => {
        const { transport, contentWindow, posted } = attach();
        try {
            const token = helloMessages(posted)[0].connectionToken;
            jest.advanceTimersByTime(250);
            expect(helloMessages(posted)).toHaveLength(2);

            receive(contentWindow, readyFor(token));
            const observation = transport.observe('request-1', Date.now() + 1000);
            const observeMessage = posted[posted.length - 1].message;
            expect(observeMessage.event).toBe('moduleAgent/observe');

            receive(contentWindow, {
                event: MODULE_AGENT_COMMAND_RESPONSE,
                moduleId: MODULE_ID,
                connectionToken: token,
                requestId: 'request-1',
                result: { moduleId: MODULE_ID },
            });
            await expect(observation).resolves.toEqual({ moduleId: MODULE_ID });

            // ready 之后握手重试必须停止。
            jest.advanceTimersByTime(READY_RETRY_WINDOW_MS);
            expect(helloMessages(posted)).toHaveLength(2);
        } finally {
            transport.dispose();
        }
    });

    test('restarts the handshake with a new token when the iframe reloads', () => {
        const { transport, frame, posted } = attach();
        try {
            const firstToken = helloMessages(posted)[0].connectionToken;
            jest.advanceTimersByTime(250);
            expect(helloMessages(posted)).toHaveLength(2);

            frame.dispatchEvent(new Event('load'));
            expect(helloMessages(posted)).toHaveLength(3);
            const reloadToken = helloMessages(posted)[2].connectionToken;
            expect(reloadToken).not.toBe(firstToken);

            jest.advanceTimersByTime(READY_RETRY_WINDOW_MS);
            const afterReload = helloMessages(posted).slice(2);
            expect(afterReload.length).toBeGreaterThan(3);
            expect(afterReload.every(message => message.connectionToken === reloadToken)).toBe(true);
        } finally {
            transport.dispose();
        }
    });

    test('caps the retry backoff and stops retrying once disposed', () => {
        const { transport, posted } = attach();
        jest.advanceTimersByTime(250 + 500 + 1000 + 2000);
        expect(helloMessages(posted)).toHaveLength(5);

        // 退避上限 4s：此后每 4s 一次，不再继续放大。
        jest.advanceTimersByTime(4000);
        expect(helloMessages(posted)).toHaveLength(6);
        jest.advanceTimersByTime(4000);
        expect(helloMessages(posted)).toHaveLength(7);

        transport.dispose();
        jest.advanceTimersByTime(READY_RETRY_WINDOW_MS);
        expect(helloMessages(posted)).toHaveLength(7);
    });

    test('reports an unavailable command provider before the module answers ready', async () => {
        const { transport } = attach();
        try {
            await expect(transport.observe('request-late', Date.now() + 1000)).rejects.toMatchObject({
                code: 'COMMAND_UNAVAILABLE',
                message: `Module '${MODULE_ID}' command provider is unavailable.`,
            });
        } finally {
            transport.dispose();
        }
    });
});

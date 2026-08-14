/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import {
    getWindowMessageRouter,
    isWindowMessageChannel,
    matchesWindowMessageOrigin,
    normalizeWindowMessageOrigin,
    withWindowMessageChannel,
} from './index';

describe('WindowMessageRouter', () => {
    test('uses one native message listener for all subscriptions', () => {
        const frame = document.createElement('iframe');
        document.body.appendChild(frame);
        const targetWindow = frame.contentWindow as Window;
        const addEventListener = jest.spyOn(targetWindow, 'addEventListener');
        const removeEventListener = jest.spyOn(targetWindow, 'removeEventListener');
        const router = getWindowMessageRouter(targetWindow);
        const unsubscribeFirst = router.subscribe(jest.fn());
        const unsubscribeSecond = router.subscribe(jest.fn());
        try {
            expect(addEventListener.mock.calls.filter(([type]) => type === 'message')).toHaveLength(1);
            unsubscribeFirst();
            expect(removeEventListener.mock.calls.filter(([type]) => type === 'message')).toHaveLength(0);
            unsubscribeSecond();
            expect(removeEventListener.mock.calls.filter(([type]) => type === 'message')).toHaveLength(1);
        } finally {
            unsubscribeFirst();
            unsubscribeSecond();
            addEventListener.mockRestore();
            removeEventListener.mockRestore();
            frame.remove();
        }
    });

    test('normalizes legacy JSON messages and preserves message metadata', () => {
        const listener = jest.fn();
        const unsubscribe = getWindowMessageRouter().subscribe(listener);
        try {
            window.dispatchEvent(messageEvent(JSON.stringify({ event: 'frame/loaded' })));
            expect(listener).toHaveBeenCalledTimes(1);
            const routedEvent = listener.mock.calls[0][0] as MessageEvent;
            expect(routedEvent.data).toEqual({ event: 'frame/loaded' });
            expect(routedEvent.origin).toBe('https://example.test');
            expect(routedEvent.source).toBe(window);
        } finally {
            unsubscribe();
        }
    });

    test('routes object messages by independent filters', () => {
        const legacyListener = jest.fn();
        const agentListener = jest.fn();
        const router = getWindowMessageRouter();
        const unsubscribeLegacy = router.subscribe(legacyListener, event => event.data?.channel === undefined);
        const unsubscribeAgent = router.subscribe(agentListener, isWindowMessageChannel('acpMessage'));
        try {
            window.dispatchEvent(messageEvent({ event: 'frame/loaded' }));
            window.dispatchEvent(messageEvent(withWindowMessageChannel('acpMessage', { event: 'insightWebAgent/ready' })));
            expect(legacyListener).toHaveBeenCalledTimes(1);
            expect(agentListener).toHaveBeenCalledTimes(1);
        } finally {
            unsubscribeLegacy();
            unsubscribeAgent();
        }
    });

    test('does not let the message body override its assigned channel', () => {
        expect(withWindowMessageChannel('acpMessage', {
            channel: 'moduleAgentMessage',
            event: 'insightWebAgent/ready',
        })).toEqual({
            channel: 'acpMessage',
            event: 'insightWebAgent/ready',
        });
    });

    test('normalizes opaque origins and supports wildcard matching', () => {
        expect(normalizeWindowMessageOrigin('null')).toBe('*');
        expect(normalizeWindowMessageOrigin('https://example.test')).toBe('https://example.test');
        expect(matchesWindowMessageOrigin('https://example.test', '*')).toBe(true);
        expect(matchesWindowMessageOrigin('https://example.test', 'https://other.test')).toBe(false);
    });

    test('stops notifying a subscriber after unsubscribe', () => {
        const listener = jest.fn();
        const unsubscribe = getWindowMessageRouter().subscribe(listener);
        unsubscribe();
        window.dispatchEvent(messageEvent({ event: 'frame/loaded' }));
        expect(listener).not.toHaveBeenCalled();
    });
});

const messageEvent = (data: unknown): MessageEvent => new MessageEvent('message', {
    data,
    origin: 'https://example.test',
    source: window,
});

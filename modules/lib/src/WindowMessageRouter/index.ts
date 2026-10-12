/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { recordWindowMessageDebug } from './debug';

export * from './debug';

export type WindowMessageFilter = (event: MessageEvent) => boolean;
export type WindowMessageListener = (event: MessageEvent) => void;
export type WindowMessageEnvelope<T extends object = Record<string, unknown>> = Omit<T, 'channel'> & { channel: string };

export const isWindowMessageChannel = (channel: string): WindowMessageFilter => (event: MessageEvent): boolean => (
    (event.data as { channel?: unknown } | null)?.channel === channel
);

export const withWindowMessageChannel = <T extends object>(channel: string, message: T): WindowMessageEnvelope<T> => ({
    ...message,
    channel,
});

export const normalizeWindowMessageOrigin = (origin: string): string => origin === 'null' ? '*' : origin;

export const currentWindowMessageOrigin = (): string => (
    window.location.protocol === 'file:' ? '*' : normalizeWindowMessageOrigin(window.location.origin)
);

export const parentWindowMessageOrigin = (): string => {
    try {
        return document.referrer ? normalizeWindowMessageOrigin(new URL(document.referrer).origin) : '*';
    } catch {
        return currentWindowMessageOrigin();
    }
};

export const frameWindowMessageOrigin = (src: string): string => {
    try {
        return normalizeWindowMessageOrigin(new URL(src, window.location.href).origin);
    } catch {
        return currentWindowMessageOrigin();
    }
};

export const matchesWindowMessageOrigin = (origin: string, expected: string): boolean => expected === '*' || origin === expected;

interface WindowMessageSubscriber {
    filter: WindowMessageFilter;
    listener: WindowMessageListener;
}

export interface WindowMessageRouter {
    subscribe: (listener: WindowMessageListener, filter?: WindowMessageFilter) => () => void;
}

class SharedWindowMessageRouter implements WindowMessageRouter {
    private readonly subscribers = new Set<WindowMessageSubscriber>();

    constructor(private readonly targetWindow: Window) {}

    subscribe(listener: WindowMessageListener, filter: WindowMessageFilter = () => true): () => void {
        const subscriber = { listener, filter };
        if (this.subscribers.size === 0) {
            this.targetWindow.addEventListener('message', this.routeMessage);
        }
        this.subscribers.add(subscriber);
        return () => {
            if (!this.subscribers.delete(subscriber) || this.subscribers.size > 0) return;
            this.targetWindow.removeEventListener('message', this.routeMessage);
        };
    }

    private readonly routeMessage = (event: MessageEvent): void => {
        const normalizedEvent = normalizeMessageEvent(event);
        recordWindowMessageDebug({
            direction: 'inbound',
            data: normalizedEvent.data,
            origin: normalizedEvent.origin,
            source: sourceWindowName(normalizedEvent.source),
        });
        [...this.subscribers].forEach(({ filter, listener }) => {
            try {
                if (filter(normalizedEvent)) listener(normalizedEvent);
            } catch (error) {
                globalThis.console.error(error);
            }
        });
    };
}

const routers = new WeakMap<Window, WindowMessageRouter>();

export const getWindowMessageRouter = (targetWindow: Window = window): WindowMessageRouter => {
    const existing = routers.get(targetWindow);
    if (existing) return existing;
    const router = new SharedWindowMessageRouter(targetWindow);
    routers.set(targetWindow, router);
    return router;
};

const normalizeMessageEvent = (event: MessageEvent): MessageEvent => {
    if (typeof event.data !== 'string') return event;
    try {
        return cloneMessageEvent(event, JSON.parse(event.data));
    } catch {
        return event;
    }
};

const cloneMessageEvent = (event: MessageEvent, data: unknown): MessageEvent => new MessageEvent('message', {
    data,
    origin: event.origin,
    lastEventId: event.lastEventId,
    source: event.source,
    ports: [...event.ports],
});

const sourceWindowName = (source: MessageEventSource | null): string | undefined => {
    if (!source || typeof window !== 'object') return undefined;
    if (source === window) return window.name || 'framework';
    if (window.parent !== window && source === window.parent) return 'parent';
    for (const frame of Array.from(document.querySelectorAll('iframe'))) {
        if (frame.contentWindow === source) return frame.name || frame.id || 'iframe';
    }
    return undefined;
};

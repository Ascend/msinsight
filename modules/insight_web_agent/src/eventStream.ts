/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { capabilityAuthHeaders, eventsUrl } from './env';

export interface EventSubscription {
    close(): void;
}

const RECONNECT_DELAY_MS = 1000;

export const subscribeEvents = (onData: (data: string) => void): EventSubscription => {
    const headers = capabilityAuthHeaders();
    if (!headers.Authorization) {
        const events = new EventSource(eventsUrl());
        events.onmessage = (event) => onData(event.data);
        return { close: () => events.close() };
    }
    const controller = new AbortController();
    void consumeWithReconnect(controller.signal, headers, onData);
    return { close: () => controller.abort() };
};

const consumeWithReconnect = async (
    signal: AbortSignal,
    headers: Record<string, string>,
    onData: (data: string) => void,
): Promise<void> => {
    while (!signal.aborted) {
        try {
            const response = await fetch(eventsUrl(), {
                headers,
                signal,
            });
            if (!response.ok || !response.body) throw new Error(`Event stream failed with HTTP ${response.status}`);
            await consumeBody(response.body, signal, onData);
        } catch (error) {
            if (signal.aborted || (error instanceof DOMException && error.name === 'AbortError')) return;
        }
        await reconnectDelay(signal);
    }
};

const consumeBody = async (
    body: ReadableStream<Uint8Array>,
    signal: AbortSignal,
    onData: (data: string) => void,
): Promise<void> => {
    const reader = body.getReader();
    const decoder = new TextDecoder();
    let buffer = '';
    try {
        while (!signal.aborted) {
            const { value, done } = await reader.read();
            buffer = `${buffer}${decoder.decode(value, { stream: !done })}`.replace(/\r\n/g, '\n');
            for (let boundary = buffer.indexOf('\n\n'); boundary !== -1; boundary = buffer.indexOf('\n\n')) {
                const frame = buffer.slice(0, boundary);
                buffer = buffer.slice(boundary + 2);
                const data = frame.split('\n')
                    .filter((line) => line.startsWith('data:'))
                    .map((line) => line.slice(5).trimStart())
                    .join('\n');
                if (data) onData(data);
            }
            if (done) return;
        }
    } finally {
        reader.releaseLock();
    }
};

const reconnectDelay = (signal: AbortSignal): Promise<void> => new Promise((resolve) => {
    if (signal.aborted) return resolve();
    const timer = window.setTimeout(done, RECONNECT_DELAY_MS);
    signal.addEventListener('abort', done, { once: true });
    function done(): void {
        window.clearTimeout(timer);
        signal.removeEventListener('abort', done);
        resolve();
    }
});

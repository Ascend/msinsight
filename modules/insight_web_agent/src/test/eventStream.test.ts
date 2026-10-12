/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { subscribeEvents, type EventSubscription } from '../eventStream';

jest.mock('../env', () => ({
    capabilityAuthHeaders: () => ({ Authorization: 'Bearer stream-token' }),
    eventsUrl: () => 'https://example.test/api/events',
}));

test('streams SSE with a Bearer header and closes without reconnecting', async () => {
    const encoded = new TextEncoder().encode('data: {"type":"state"}\n\n');
    const releaseLock = jest.fn();
    const read = jest.fn()
        .mockResolvedValueOnce({ value: encoded, done: false })
        .mockResolvedValue({ value: undefined, done: true });
    global.fetch = jest.fn(async () => ({
        ok: true,
        status: 200,
        body: { getReader: () => ({ read, releaseLock }) },
    })) as unknown as typeof fetch;

    let subscription: EventSubscription | undefined;
    const received = await new Promise<string>((resolve) => {
        subscription = subscribeEvents((data) => {
            subscription?.close();
            resolve(data);
        });
    });

    expect(received).toBe('{"type":"state"}');
    expect(global.fetch).toHaveBeenCalledWith('https://example.test/api/events', expect.objectContaining({
        headers: { Authorization: 'Bearer stream-token' },
    }));
    expect(releaseLock).toHaveBeenCalled();
});

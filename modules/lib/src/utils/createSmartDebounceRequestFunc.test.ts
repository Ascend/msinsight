/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import { createSmartDebounceRequestFunc } from './createSmartDebounceRequestFunc';

describe('createSmartDebounceRequestFunc', () => {
    beforeAll(() => {
        jest.useFakeTimers();
    });

    beforeEach(() => {
        jest.clearAllTimers();
    });

    afterAll(() => {
        jest.useRealTimers();
    });

    const createMockRequest = () => {
        const fn = jest.fn((...args: any[]) => {
            return Promise.resolve({ result: args[0] ?? 'default' });
        });
        return { fn };
    };

    const createDeferredRequest = () => {
        let resolveRef: (value?: any) => void = () => {};
        let rejectRef: (reason?: any) => void = () => {};
        const fn = jest.fn((...args: any[]) => {
            return new Promise((res, rej) => {
                resolveRef = () => res({ result: args[0] ?? 'default' });
                rejectRef = rej;
            });
        });
        return { fn, getResolve: () => resolveRef, getReject: () => rejectRef };
    };

    it('should delay execution and return result', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        const promise = debounced('a');
        expect(fn).not.toHaveBeenCalled();

        jest.advanceTimersByTime(300);
        const result = await promise;

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('a');
        expect(result).toEqual({ result: 'a' });
    });

    it('should replace parameters when called multiple times in pending', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        debounced('a');
        jest.advanceTimersByTime(100);
        debounced('b');
        jest.advanceTimersByTime(100);
        debounced('c');

        jest.advanceTimersByTime(300);

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('c');
    });

    it('should reset timer on each call in pending', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        debounced('a');
        jest.advanceTimersByTime(200);
        debounced('b');
        jest.advanceTimersByTime(200);

        expect(fn).not.toHaveBeenCalled();

        jest.advanceTimersByTime(100);

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('b');
    });

    it('should discard old result and re-enter pending when called during inflight', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(50);

        const p2 = debounced('b');
        expect(debounced.getPendingCount()).toBe(1);

        getResolve()();
        jest.advanceTimersByTime(50);

        expect(fn).toHaveBeenCalledTimes(2);
        expect(fn).toHaveBeenNthCalledWith(1, 'a');
        expect(fn).toHaveBeenNthCalledWith(2, 'b');

        getResolve()();
        const r2 = await p2;
        expect(r2).toEqual({ result: 'b' });
    });

    it('should only execute the latest request after inflight completes', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 30 });

        debounced('a');
        jest.advanceTimersByTime(30);

        debounced('b');
        debounced('c');
        debounced('d');

        getResolve()();
        jest.advanceTimersByTime(30);

        expect(fn).toHaveBeenCalledTimes(2);
        expect(fn).toHaveBeenNthCalledWith(1, 'a');
        expect(fn).toHaveBeenNthCalledWith(2, 'd');
    });

    it('should cancel pending request', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        debounced('a');
        debounced.cancel();

        jest.advanceTimersByTime(300);
        expect(fn).not.toHaveBeenCalled();
    });

    it('should cancel all pending requests when no key provided', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, {
            delay: 300,
            keyFn: (arg: string) => arg,
        });

        debounced('key1');
        debounced('key2');

        debounced.cancel();

        jest.advanceTimersByTime(300);
        expect(fn).not.toHaveBeenCalled();
    });

    it('should not affect inflight request when cancelled', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(50);

        debounced.cancel();
        expect(debounced.getPendingCount()).toBe(0);

        getResolve()();
        expect(fn).toHaveBeenCalledTimes(1);
    });

    it('should cancel next pending during inflight', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(50);

        debounced('b');
        debounced.cancel();

        getResolve()();
        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('a');
    });

    it('should immediately execute pending request on flush', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        const p = debounced('a');
        jest.advanceTimersByTime(100);

        debounced.flush();

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('a');

        const result = await p;
        expect(result).toEqual({ result: 'a' });
    });

    it('should flush all pending requests', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, {
            delay: 300,
            keyFn: (arg: string) => arg,
        });

        debounced('key1');
        debounced('key2');

        debounced.flush();

        expect(fn).toHaveBeenCalledTimes(2);
    });

    it('should not flush during inflight', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(50);

        debounced.flush();
        expect(fn).toHaveBeenCalledTimes(1);

        getResolve()();
    });

    it('should call onBeforeRequest and onAfterRequest', async () => {
        const { fn } = createMockRequest();
        const onBeforeRequest = jest.fn();
        const onAfterRequest = jest.fn();

        const debounced = createSmartDebounceRequestFunc(fn, {
            delay: 50,
            onBeforeRequest,
            onAfterRequest,
        });

        const p = debounced('a');
        jest.advanceTimersByTime(50);
        await p;

        expect(onBeforeRequest).toHaveBeenCalledWith('a');
        expect(onAfterRequest).toHaveBeenCalledWith({ result: 'a' }, 'a');
    });

    it('should reject when requestFn throws', async () => {
        const fn = jest.fn(() => Promise.reject(new Error('network error')));
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        const p = debounced();
        jest.advanceTimersByTime(50);

        await expect(p).rejects.toThrow('network error');
    });

    it('should allow retry after rejection', async () => {
        let shouldFail = true;
        const fn = jest.fn(async () => {
            if (shouldFail) {
                shouldFail = false;
                throw new Error('fail');
            }
            return 'success';
        });

        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        const p1 = debounced();
        jest.advanceTimersByTime(50);
        await expect(p1).rejects.toThrow('fail');

        const p2 = debounced();
        jest.advanceTimersByTime(50);
        const result = await p2;
        expect(result).toBe('success');
    });

    it('should isolate different keys', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, {
            delay: 50,
            keyFn: (arg: string) => arg,
        });

        debounced('a');
        debounced('b');

        jest.advanceTimersByTime(50);

        expect(fn).toHaveBeenCalledTimes(2);
        expect(fn).toHaveBeenCalledWith('a');
        expect(fn).toHaveBeenCalledWith('b');
    });

    it('getPendingCount should return correct count', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300 });

        expect(debounced.getPendingCount()).toBe(0);

        debounced('a');
        expect(debounced.getPendingCount()).toBe(1);

        debounced('a');
        expect(debounced.getPendingCount()).toBe(1);

        jest.advanceTimersByTime(300);
        await Promise.resolve();
        expect(debounced.getPendingCount()).toBe(0);
    });

    it('getPendingKeys should return correct keys', () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, {
            delay: 300,
            keyFn: (arg: string) => arg,
        });

        expect(debounced.getPendingKeys()).toEqual([]);

        debounced('key1');
        debounced('key2');
        expect(debounced.getPendingKeys().sort()).toEqual(['key1', 'key2']);
    });

    it('should handle cancel immediately followed by new call', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        debounced.cancel();
        const p = debounced('b');

        jest.advanceTimersByTime(50);
        const result = await p;

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('b');
    });

    it('should handle rapid cancel + new call during timer expiry', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(49);
        debounced.cancel();
        const p = debounced('b');

        jest.advanceTimersByTime(1);
        expect(fn).not.toHaveBeenCalled();

        jest.advanceTimersByTime(49);
        const result = await p;

        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('b');
    });

    it('should handle flush + new call during timer expiry', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 50 });

        debounced('a');
        jest.advanceTimersByTime(30);
        debounced.flush();
        getResolve()();
        await Promise.resolve();

        debounced('b');
        jest.advanceTimersByTime(50);
        getResolve()();

        expect(fn).toHaveBeenCalledTimes(2);
        expect(fn).toHaveBeenNthCalledWith(1, 'a');
        expect(fn).toHaveBeenNthCalledWith(2, 'b');
    });
});

describe('createSmartDebounceRequestFunc with leading=true', () => {
    beforeAll(() => {
        jest.useFakeTimers();
    });

    beforeEach(() => {
        jest.clearAllTimers();
    });

    afterAll(() => {
        jest.useRealTimers();
    });

    const createMockRequest = () => {
        const fn = jest.fn((...args: any[]) => {
            return Promise.resolve({ result: args[0] ?? 'default' });
        });
        return { fn };
    };

    const createDeferredRequest = () => {
        let resolveRef: (value?: any) => void = () => {};
        let rejectRef: (reason?: any) => void = () => {};
        const fn = jest.fn((...args: any[]) => {
            return new Promise((res, rej) => {
                resolveRef = () => res({ result: args[0] ?? 'default' });
                rejectRef = rej;
            });
        });
        return { fn, getResolve: () => resolveRef, getReject: () => rejectRef };
    };

    it('should execute first request immediately but resolve after delay', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300, leading: true });

        const promise = debounced('a');
        // 立即执行请求
        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('a');

        // 但 delay 内不 resolve
        jest.advanceTimersByTime(100);
        let resolved = false;
        promise.then(() => { resolved = true; });
        await Promise.resolve();
        expect(resolved).toBe(false);

        // delay 到期后 resolve
        jest.advanceTimersByTime(200);
        const result = await promise;
        expect(result).toEqual({ result: 'a' });
    });

    it('should batch all callers within delay window', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300, leading: true });

        const p1 = debounced('a');
        jest.advanceTimersByTime(50);
        const p2 = debounced('b');
        jest.advanceTimersByTime(50);
        const p3 = debounced('c');

        // delay 内只执行了 1 次请求
        expect(fn).toHaveBeenCalledTimes(1);
        expect(fn).toHaveBeenCalledWith('a');

        jest.advanceTimersByTime(200);
        const [r1, r2, r3] = await Promise.all([p1, p2, p3]);

        // 所有人都拿到第一次的结果
        expect(r1).toEqual({ result: 'a' });
        expect(r2).toEqual({ result: 'a' });
        expect(r3).toEqual({ result: 'a' });
    });

    it('should resolve immediately when request takes longer than delay', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 100, leading: true });

        const p1 = debounced('a');
        const p2 = debounced('b');
        expect(fn).toHaveBeenCalledTimes(1);

        // delay 到期但请求未完成
        jest.advanceTimersByTime(100);
        let resolved = false;
        p1.then(() => { resolved = true; });
        await Promise.resolve();
        expect(resolved).toBe(false);

        // 请求完成后立即 resolve
        getResolve()();
        const r1 = await p1;
        const r2 = await p2;
        expect(r1).toEqual({ result: 'a' });
        expect(r2).toEqual({ result: 'a' });
    });

    it('should start new cycle after past_delay', async () => {
        const { fn, getResolve } = createDeferredRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 100, leading: true });

        debounced('a');
        jest.advanceTimersByTime(100); // delay 到期，进入 past_delay

        // past_delay 期间的新调用开始新周期
        const p = debounced('b');
        expect(fn).toHaveBeenCalledTimes(2);
        expect(fn).toHaveBeenNthCalledWith(2, 'b');

        getResolve()();
        jest.advanceTimersByTime(100);
        const result = await p;
        expect(result).toEqual({ result: 'b' });
    });

    it('should cancel all waiting callers', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300, leading: true });

        const p1 = debounced('a');
        const p2 = debounced('b');
        debounced.cancel();

        await expect(p1).rejects.toThrow('Cancelled');
        await expect(p2).rejects.toThrow('Cancelled');
    });

    it('should flush to end delay early', async () => {
        const { fn } = createMockRequest();
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 300, leading: true });

        const p = debounced('a');
        debounced.flush();

        const result = await p;
        expect(result).toEqual({ result: 'a' });
        expect(fn).toHaveBeenCalledTimes(1);
    });

    it('should propagate error to all waiting callers', async () => {
        const fn = jest.fn(() => Promise.reject(new Error('network error')));
        const debounced = createSmartDebounceRequestFunc(fn, { delay: 100, leading: true });

        const p1 = debounced();
        const p2 = debounced();

        jest.advanceTimersByTime(100);
        await expect(p1).rejects.toThrow('network error');
        await expect(p2).rejects.toThrow('network error');
    });

    it('should allow retry after error', async () => {
        let shouldFail = true;
        const fn = jest.fn(async () => {
            if (shouldFail) {
                shouldFail = false;
                throw new Error('fail');
            }
            return 'success';
        });

        const debounced = createSmartDebounceRequestFunc(fn, { delay: 100, leading: true });

        const p1 = debounced();
        jest.advanceTimersByTime(100);
        await expect(p1).rejects.toThrow('fail');

        const p2 = debounced();
        jest.advanceTimersByTime(100);
        const result = await p2;
        expect(fn).toHaveBeenCalledTimes(2);
        expect(result).toBe('success');
    });
});

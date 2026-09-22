/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import { customDebounce } from '../customDebounce';

describe('customDebounce', () => {
    it('should call the callback function immediately if no task is waiting', async () => {
        const mockCallback = jest.fn(() => Promise.resolve('result'));
        const debouncedFunction = customDebounce(mockCallback);

        await debouncedFunction();
        expect(mockCallback).toHaveBeenCalledTimes(1);
    });

    it('should not call the callback function immediately if a task is waiting', async () => {
        const mockCallback = jest.fn(() => Promise.resolve('result'));
        const debouncedFunction = customDebounce(mockCallback);

        debouncedFunction();
        debouncedFunction();
        expect(mockCallback).toHaveBeenCalledTimes(1);
    });

    it('should resolve with the result of the first task if a new task is queued', async () => {
        const mockCallback = jest.fn(() => Promise.resolve('result'));
        const debouncedFunction = customDebounce(mockCallback);

        const result1 = debouncedFunction();
        const result2 = debouncedFunction();

        expect(await result1).toBe('result');
        expect(await result2).toBe('result');
    });

    it('should handle errors from the callback function', async () => {
        const mockCallback = jest.fn(() => Promise.reject(new Error('error')));
        const debouncedFunction = customDebounce(mockCallback);

        try {
            await debouncedFunction();
        } catch (e: any) {
            expect(e).toBeInstanceOf(Error);
            expect(e.message).toBe('error');
        }
    });

    it('should call the callback function again if the previous task is resolved', async () => {
        const mockCallback = jest.fn(() => Promise.resolve('result'));
        const debouncedFunction = customDebounce(mockCallback);

        await debouncedFunction();
        await debouncedFunction();

        expect(mockCallback).toHaveBeenCalledTimes(2);
    });

    it('should retry after the first request fails', async () => {
        const error = new Error('request failed');
        const mockCallback = jest.fn().mockRejectedValueOnce(error).mockResolvedValue('recovered');
        const debouncedFunction = customDebounce(mockCallback);

        await expect(debouncedFunction()).rejects.toBe(error);
        await expect(debouncedFunction()).resolves.toBe('recovered');
        expect(mockCallback).toHaveBeenCalledTimes(2);
    });

    it('should reject trailing and queued calls when the trailing request fails', async () => {
        let resolveFirst!: (value: string) => void;
        let rejectTrailing!: (error: Error) => void;
        const firstRequest = new Promise<string>(resolve => { resolveFirst = resolve; });
        const trailingRequest = new Promise<string>((resolve, reject) => { rejectTrailing = reject; });
        const mockCallback = jest.fn()
            .mockReturnValueOnce(firstRequest)
            .mockReturnValueOnce(trailingRequest)
            .mockResolvedValue('recovered');
        const debouncedFunction = customDebounce(mockCallback);
        const firstCall = debouncedFunction();
        const trailingCall = debouncedFunction();

        resolveFirst('first result');
        await firstCall;
        expect(mockCallback).toHaveBeenCalledTimes(2);

        const queuedCall = debouncedFunction();
        const results = Promise.allSettled([trailingCall, queuedCall]);
        const error = new Error('trailing request failed');
        rejectTrailing(error);

        await expect(results).resolves.toEqual([
            { status: 'rejected', reason: error },
            { status: 'rejected', reason: error },
        ]);
        await expect(debouncedFunction()).resolves.toBe('recovered');
        expect(mockCallback).toHaveBeenCalledTimes(3);
    });

    it('should settle all queued calls when the first request fails', async () => {
        let rejectFirst!: (error: Error) => void;
        const firstRequest = new Promise<string>((resolve, reject) => { rejectFirst = reject; });
        const mockCallback = jest.fn().mockReturnValueOnce(firstRequest).mockResolvedValue('recovered');
        const debouncedFunction = customDebounce(mockCallback);
        const results = Promise.allSettled([debouncedFunction(), debouncedFunction(), debouncedFunction()]);
        const error = new Error('first request failed');

        rejectFirst(error);

        await expect(results).resolves.toEqual([
            { status: 'rejected', reason: error },
            { status: 'rejected', reason: error },
            { status: 'rejected', reason: error },
        ]);
        await expect(debouncedFunction()).resolves.toBe('recovered');
        expect(mockCallback).toHaveBeenCalledTimes(2);
    });
});

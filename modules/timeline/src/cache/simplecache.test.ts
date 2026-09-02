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

import { SimpleCache } from './simplecache';

describe('SimpleCache raw counter data', () => {
    const originalRequest = window.request;

    afterEach(() => {
        window.request = originalRequest;
    });

    it('reuses one in-flight and completed request until the session cache is cleared', async () => {
        const request = jest.fn().mockResolvedValue({
            data: [{ timestamp: 10, value: { hits: 9, misses: 1 } }],
        });
        window.request = request;
        const cache = new SimpleCache();
        const params = { dataSource: {} as never, startTime: 0, endTime: 100 };

        const [first, second] = await Promise.all([
            cache.fetchRawCounterData('thread-1-llc', params),
            cache.fetchRawCounterData('thread-1-llc', params),
        ]);

        expect(request).toHaveBeenCalledTimes(1);
        expect(second).toBe(first);

        cache.clear();
        await cache.fetchRawCounterData('thread-1-llc', params);
        expect(request).toHaveBeenCalledTimes(2);
    });
});

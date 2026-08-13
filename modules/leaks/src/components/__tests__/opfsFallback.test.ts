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

import { createOpfsFallbackGuard } from '../opfsFallbackGuard';
import { createOpfsFallbackPrompt } from '../opfsFallbackPrompt';

describe('OPFS fallback guard', () => {
    it('continues without confirmation when OPFS is available', async () => {
        const probe = jest.fn().mockResolvedValue(true);
        const requestApproval = jest.fn().mockResolvedValue(undefined);
        const guard = createOpfsFallbackGuard(probe, requestApproval);

        await expect(Promise.all([guard(), guard()])).resolves.toEqual([undefined, undefined]);
        await expect(guard()).resolves.toBeUndefined();
        expect(probe).toHaveBeenCalledTimes(1);
        expect(requestApproval).not.toHaveBeenCalled();
    });

    it('shares one approval and remembers an approved fallback', async () => {
        let approveFallback: () => void = () => undefined;
        const probe = jest.fn().mockResolvedValue(false);
        const requestApproval = jest.fn(() => new Promise<void>(resolve => {
            approveFallback = resolve;
        }));
        const guard = createOpfsFallbackGuard(probe, requestApproval);

        const first = guard();
        const second = guard();
        await new Promise(resolve => setTimeout(resolve, 0));
        expect(probe).toHaveBeenCalledTimes(1);
        expect(requestApproval).toHaveBeenCalledTimes(1);

        approveFallback();
        await expect(Promise.all([first, second])).resolves.toEqual([undefined, undefined]);
        await expect(guard()).resolves.toBeUndefined();
        expect(requestApproval).toHaveBeenCalledTimes(1);
    });

    it('keeps the inline notice visible until the user approves', async () => {
        const prompt = createOpfsFallbackPrompt();
        const listener = jest.fn();
        const unsubscribe = prompt.subscribe(listener);
        const approval = prompt.requestApproval();

        expect(prompt.getSnapshot()).toBe(true);
        expect(listener).toHaveBeenCalledTimes(1);

        prompt.approve();
        await expect(approval).resolves.toBeUndefined();
        expect(prompt.getSnapshot()).toBe(false);
        expect(listener).toHaveBeenCalledTimes(2);
        unsubscribe();
    });
});

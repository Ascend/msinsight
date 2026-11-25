/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { autorun, runInAction } from 'mobx';
import { createSession } from '../session';

describe('MemScope application session', () => {
    it('owns only Scope fields and isolates mutable defaults', () => {
        const first = createSession();
        const second = createSession();
        expect(first.module).toBe('leaks');
        expect('snapshotSlices' in first).toBe(false);
        expect('stateWorkerInfo' in first).toBe(false);
        expect(first.getUpdateKeys()).toContain('threadIds');
        runInAction(() => {
            first.threadIds.push(42);
            first.leaksWorkerInfo.renderOptions.viewport.width = 200;
            first.markLineInfo.block.x = 9;
        });
        expect(second.threadIds).toEqual([]);
        expect(second.leaksWorkerInfo.renderOptions.viewport.width).toBe(0);
        expect(second.markLineInfo.block.x).toBe(-1);
    });
    it('retains subscriptions through the feature-state view', () => {
        const session = createSession();
        const seen: string[] = [];
        const dispose = autorun(() => { seen.push(String(session.threadId)); });
        runInAction(() => { session.threadId = 42; });
        expect(seen).toEqual(['', '42']);
        dispose();
    });
});

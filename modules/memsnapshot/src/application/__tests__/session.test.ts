/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { autorun, runInAction } from 'mobx';
import { createSession } from '../session';

describe('MemSnapshot application session', () => {
    it('owns only Snapshot fields and isolates renderer defaults', () => {
        const first = createSession();
        const second = createSession();
        expect(first.module).toBe('memsnapshot');
        expect('threadIds' in first).toBe(false);
        expect('funcData' in first).toBe(false);
        runInAction(() => { first.stateWorkerInfo.renderOptions.viewport.width = 200; });
        expect(second.stateWorkerInfo.renderOptions.viewport.width).toBe(0);
    });
    it('retains immutable overview array identity and notifications', () => {
        const session = createSession();
        const payload = { allocations: [], maxTimestamp: 1, minTimestamp: 0 };
        const seen: unknown[] = [];
        const dispose = autorun(() => { seen.push(session.sliceOverviewData); });
        const overview = { 0: { 0: payload } };
        runInAction(() => { session.sliceOverviewData = overview; });
        expect(session.sliceOverviewData).toBe(overview);
        expect(session.sliceOverviewData[0][0].allocations).toBe(payload.allocations);
        expect(seen).toHaveLength(2);
        dispose();
    });
});

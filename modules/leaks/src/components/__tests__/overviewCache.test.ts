/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { autorun, isObservable, runInAction } from 'mobx';
import { Session } from '../../entity/session';

it('publishes overview replacements without converting coordinate objects into observables', () => {
    const session = new Session();
    const allocations = [{ timestamp: 0, totalSize: 100 }];
    const observed: number[] = [];
    const dispose = autorun(() => observed.push(Object.keys(session.sliceOverviewData[0] ?? {}).length));
    runInAction(() => { session.sliceOverviewData = { 0: { 0: { allocations, minTimestamp: 0, maxTimestamp: 0 } } }; });
    expect(session.sliceOverviewData[0][0].allocations).toBe(allocations);
    expect(isObservable(session.sliceOverviewData[0][0].allocations[0])).toBe(false);
    expect(observed).toEqual([0, 1]);
    runInAction(() => { session.sliceOverviewData = {}; });
    expect(observed).toEqual([0, 1, 0]);
    dispose();
});

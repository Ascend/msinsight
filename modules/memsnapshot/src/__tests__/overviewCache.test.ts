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
import { autorun, isObservable, runInAction } from 'mobx';
import { createSession as createFeatureSession } from '../application/session';

it('publishes overview replacements without converting coordinate objects into observables', () => {
    const session = createFeatureSession();
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

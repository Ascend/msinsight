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
import { autorun, computed, observable, runInAction } from 'mobx';
import { buildUnitOffsets, computeVisibleRange } from './unitLayout';

function referenceRange(heights: number[], height: number, top: number): [number, number] {
    let start = 0;
    let end = 0;
    let offset = 0;
    for (const laneHeight of heights) {
        if (offset + 30 > height + top) { break; }
        if (offset + laneHeight + 30 < top) { start++; }
        offset += laneHeight + 1;
        end++;
    }
    return [start, end + 1];
}

describe('virtual lane layout', () => {
    it('preserves visible lanes and placeholders at empty, collapsed, deep and overscan boundaries', () => {
        for (const heights of [[], [0], [20], [20, 0, 50, 50000, 20], Array.from({ length: 768 }, (_, i) => i % 7 * 10)]) {
            const offsets = buildUnitOffsets(heights.map(height => ({ height: () => height })));
            for (const height of [0, 1, 29, 30, 200, 800]) {
                const positions = [-1, 0, 1, 29, 30, 31, 50, 51, 52, 800, 50050, 100000];
                for (const boundary of offsets) { positions.push(boundary - 1, boundary, boundary + 29, boundary + 30); }
                for (const top of positions) {
                    const actual = computeVisibleRange(offsets, height, top);
                    expect(actual).toEqual(referenceRange(heights, height, top));
                    const [first, last] = actual;
                    expect(offsets[first]).toBe(heights.slice(0, first).reduce((sum, item) => sum + item + 1, 0));
                    expect(offsets[heights.length] - offsets[Math.min(last, heights.length)])
                        .toBe(heights.slice(last).reduce((sum, item) => sum + item + 1, 0));
                }
            }
        }
    });

    it('rebuilds observed offsets for height, visibility and order changes, not scroll positions', () => {
        const state = observable({ lanes: [{ id: 'a', height: 20, visible: true }, { id: 'b', height: 40, visible: true }], top: 0 });
        let builds = 0;
        const layout = computed(() => {
            builds++;
            const units = state.lanes.filter(lane => lane.visible);
            return { ids: units.map(lane => lane.id), offsets: buildUnitOffsets(units.map(lane => ({ height: () => lane.height }))) };
        });
        const snapshots: Array<{ ids: string[]; offsets: number[] }> = [];
        const stop = autorun(() => {
            const value = layout.get();
            computeVisibleRange(value.offsets, 200, state.top);
            snapshots.push(value);
        });
        for (let i = 1; i <= 100; i++) { runInAction(() => { state.top = i; }); }
        expect(builds).toBe(1);
        runInAction(() => { state.lanes[0].height = 60; });
        expect(snapshots[snapshots.length - 1].offsets).toEqual([0, 61, 102]);
        runInAction(() => { state.lanes.reverse(); });
        expect(snapshots[snapshots.length - 1].ids).toEqual(['b', 'a']);
        runInAction(() => { state.lanes[0].visible = false; });
        expect(snapshots[snapshots.length - 1]).toEqual({ ids: ['a'], offsets: [0, 61] });
        expect(builds).toBe(4);
        stop();
    });
});

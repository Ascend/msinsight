/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { OverviewIndex, OverviewTimelineIndex, getOverviewIndex, getOverviewSummary, overviewY, sampleOverview, summarizeOverview } from '../snapshotOverview';

describe('snapshot overview aggregation', () => {
    it('retains a one-event peak and valley inside the same pixel in event order', () => {
        const points = Array.from({ length: 10000 }, (_, timestamp) => ({ timestamp, totalSize: 10 }));
        points[421].totalSize = 1000;
        points[422].totalSize = 0;
        const sampled = sampleOverview(points, 0, 9999, 20);
        expect(sampled).toContain(points[421]);
        expect(sampled).toContain(points[422]);
        expect(sampled.indexOf(points[421])).toBeLessThan(sampled.indexOf(points[422]));
        expect(sampled[0]).toBe(points[0]);
        expect(sampled[sampled.length - 1]).toBe(points[9999]);
        expect(sampled.length).toBeLessThanOrEqual(80);
        expect(summarizeOverview(points)).toEqual({ min: 0, max: 1000, peak: points[421] });
    });

    it('keeps equal-time event ordering and handles a single-event window', () => {
        const points = [2, 8, 0, 4].map(totalSize => ({ timestamp: 4, totalSize }));
        expect(sampleOverview(points, 4, 4, 100)).toEqual(points);
        expect(overviewY(4, 4, 4, 72)).toBe(68);
        expect(sampleOverview([], 0, 0, 0)).toEqual([]);
        expect(summarizeOverview([]).peak).toBeUndefined();
    });

    it('ignores invalid and out-of-range coordinates', () => {
        const points = [{ timestamp: -1, totalSize: 2 }, { timestamp: 0, totalSize: NaN },
            { timestamp: 1, totalSize: 4 }, { timestamp: 10, totalSize: 7 }];
        expect(sampleOverview(points, 0, 5, 10)).toEqual([points[2]]);
    });
});

it('clips boundary segments, including a range between two sparse samples', () => {
    const points = [{ timestamp: 0, totalSize: 10 }, { timestamp: 100, totalSize: 30 }];
    const index = new OverviewIndex(points);
    expect(index.sample(25, 75, 100)).toEqual([{ timestamp: 25, totalSize: 15 }, { timestamp: 75, totalSize: 25 }]);
    expect(index.sample(100, 150, 100)).toEqual([points[1]]);
    expect(index.sample(101, 150, 100)).toEqual([]);
});

it('normalizes unordered input without mutating it or reordering equal-time events', () => {
    const points = [{ timestamp: 100, totalSize: 30 }, { timestamp: 50, totalSize: 1000 },
        { timestamp: NaN, totalSize: 20 }, { timestamp: 0, totalSize: 10 },
        { timestamp: 50, totalSize: 0 }, { timestamp: 75, totalSize: Infinity }];
    const original = points.map(point => ({ ...point }));
    const index = new OverviewIndex(points);
    expect(points).toEqual(original);
    expect(index.points).toEqual([points[3], points[1], points[4], points[0]]);
    expect(index.sample(0, 100, 1)).toEqual([points[3], points[1], points[4], points[0]]);
    expect(index.sample(25, 75, 100)).toEqual([
        { timestamp: 25, totalSize: 505 }, points[1], points[4], { timestamp: 75, totalSize: 15 },
    ]);
});

it('indexed sampling matches exact per-pixel extrema and clips sparse boundaries', () => {
    const points = Array.from({ length: 10000 }, (_, timestamp) => ({ timestamp, totalSize: (timestamp * 113) % 317 }));
    points[321].totalSize = 99999;
    points[322].totalSize = -99999;
    const index = new OverviewIndex(points);
    for (const width of [1, 20, 127]) {
        expect(index.sample(0, 9999, width)).toEqual(sampleOverview(points, 0, 9999, width));
    }
    const sparse = new OverviewIndex([{ timestamp: 0, totalSize: 10 }, { timestamp: 100, totalSize: 30 }]);
    expect(sparse.sample(25, 75, 100)).toEqual([{ timestamp: 25, totalSize: 15 }, { timestamp: 75, totalSize: 25 }]);
    expect(sparse.sample(101, 150, 100)).toEqual([]);
    const repeated = [2, 8, 0, 4].map(totalSize => ({ timestamp: 4, totalSize }));
    expect(new OverviewIndex(repeated).sample(4, 4, 100)).toEqual(repeated);
});

it('scans partial blocks exactly across block boundaries and reuses immutable payloads', () => {
    const points = Array.from({ length: 513 }, (_, timestamp) => ({ timestamp, totalSize: (timestamp * 113) % 317 }));
    const index = getOverviewIndex(points);
    expect(index.points).toBe(points);
    expect(getOverviewIndex(points)).toBe(index);
    expect(getOverviewSummary(points)).toBe(getOverviewSummary(points));
    expect(getOverviewIndex([...points])).not.toBe(index);
    for (const start of [0, 1, 63, 64, 65, 127, 128, 511]) {
        for (const end of [start, Math.min(512, start + 1), Math.min(512, start + 80), 512]) {
            for (const width of [1, 7, 64]) {
                expect(index.sample(start, end, width)).toEqual(sampleOverview(points, start, end, width));
            }
        }
    }
});

it('samples a window timeline on the global pixel grid without losing window extrema', () => {
    const points = Array.from({ length: 1000 }, (_, timestamp) => ({ timestamp, totalSize: (timestamp * 113) % 317 }));
    points[127].totalSize = 99999;
    points[128].totalSize = -99999;
    const segments = [0, 128, 577].map((start, index, starts) => ({
        start,
        end: (starts[index + 1] ?? points.length) - 1,
        points: points.slice(start, starts[index + 1] ?? points.length),
    }));
    const index = new OverviewTimelineIndex(segments);
    for (const width of [1, 7, 31, 1000]) {
        expect(index.sample(0, 999, width)).toEqual(sampleOverview(points, 0, 999, width));
    }
    expect(index.sample(128, 577, 1)).toEqual(sampleOverview(points, 128, 577, 1));
});

it('interpolates view edges across sparse windows and excludes out-of-window samples', () => {
    const index = new OverviewTimelineIndex([
        { start: 0, end: 49, points: [{ timestamp: -1, totalSize: 1000 }, { timestamp: 0, totalSize: 10 }] },
        { start: 50, end: 100, points: [{ timestamp: 100, totalSize: 30 }, { timestamp: 101, totalSize: 1000 }] },
    ]);
    expect(index.sample(25, 75, 100)).toEqual([{ timestamp: 25, totalSize: 15 }, { timestamp: 75, totalSize: 25 }]);
    expect(index.sample(0, 100, 100)).toEqual([{ timestamp: 0, totalSize: 10 }, { timestamp: 100, totalSize: 30 }]);
});

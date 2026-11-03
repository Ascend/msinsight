/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 */

export interface OverviewPoint {
    timestamp: number;
    totalSize: number;
}

export const summarizeOverview = (points: OverviewPoint[]): { min: number; max: number; peak?: OverviewPoint } => {
    let min = Infinity;
    let max = -Infinity;
    let peak: OverviewPoint | undefined;
    points.forEach(point => {
        if (!Number.isFinite(point.timestamp) || !Number.isFinite(point.totalSize)) return;
        min = Math.min(min, point.totalSize);
        if (point.totalSize > max) {
            max = point.totalSize;
            peak = point;
        }
    });
    return { min: peak ? min : 0, max: peak ? max : 0, peak };
};

// Retain both extrema and the entry/exit values of each screen column in input order.
// Allocation events are already ordered by the API; equal timestamps must retain their order.
export const sampleOverview = (points: OverviewPoint[], start: number, end: number, width: number): OverviewPoint[] => {
    const columns = Math.max(1, Math.floor(width));
    const span = Math.max(1, end - start);
    const result: OverviewPoint[] = [];
    let bucket = -1;
    let first = -1;
    let last = -1;
    let min = -1;
    let max = -1;
    const flush = (): void => {
        if (first < 0) return;
        const indices = [...new Set([first, min, max, last])].sort((a, b) => a - b);
        indices.forEach(index => result.push(points[index]));
    };
    points.forEach((point, index) => {
        if (!Number.isFinite(point.timestamp) || !Number.isFinite(point.totalSize) ||
            point.timestamp < start || point.timestamp > end) return;
        const column = Math.min(columns - 1, Math.floor((point.timestamp - start) / span * columns));
        if (column !== bucket) {
            flush();
            bucket = column;
            first = min = max = index;
        }
        last = index;
        if (point.totalSize < points[min].totalSize) min = index;
        if (point.totalSize > points[max].totalSize) max = index;
    });
    flush();
    return result;
};

export const overviewY = (value: number, min: number, max: number, height: number): number =>
    height - 4 - (value - min) / Math.max(1, max - min) * (height - 8);

// Cache only immutable payloads. Weak keys allow replaced windows/files to be collected.
const summaryCache = new WeakMap<OverviewPoint[], ReturnType<typeof summarizeOverview>>();
export const getOverviewSummary = (points: OverviewPoint[]): ReturnType<typeof summarizeOverview> => {
    let summary = summaryCache.get(points);
    if (!summary) {
        summary = summarizeOverview(points);
        summaryCache.set(points, summary);
    }
    return summary;
};

// Tree leaves cover blocks rather than individual events. Queries scan only their boundary blocks.
const INDEX_BLOCK_SIZE = 64;
export class OverviewIndex {
    readonly points: OverviewPoint[];
    private readonly size: number;
    private readonly minima: Int32Array;
    private readonly maxima: Int32Array;

    constructor(points: OverviewPoint[]) {
        this.points = points.every(point => Number.isFinite(point.timestamp) && Number.isFinite(point.totalSize))
            ? points
            : points.filter(point => Number.isFinite(point.timestamp) && Number.isFinite(point.totalSize));
        // Keep the normal ordered path linear; stable sorting preserves equal-time event order.
        if (this.points.some((point, index) => index > 0 && point.timestamp < this.points[index - 1].timestamp)) {
            this.points = [...this.points].sort((left, right) => left.timestamp - right.timestamp);
        }
        this.size = 2 ** Math.ceil(Math.log2(Math.max(1, Math.ceil(this.points.length / INDEX_BLOCK_SIZE))));
        this.minima = new Int32Array(this.size * 2).fill(-1);
        this.maxima = new Int32Array(this.size * 2).fill(-1);
        for (let index = 0; index < this.points.length; index++) {
            const node = this.size + Math.floor(index / INDEX_BLOCK_SIZE);
            this.minima[node] = this.extreme(this.minima[node], index, false);
            this.maxima[node] = this.extreme(this.maxima[node], index, true);
        }
        for (let node = this.size - 1; node > 0; node--) {
            this.minima[node] = this.extreme(this.minima[node * 2], this.minima[node * 2 + 1], false);
            this.maxima[node] = this.extreme(this.maxima[node * 2], this.maxima[node * 2 + 1], true);
        }
    }

    private extreme(a: number, b: number, maximum: boolean): number {
        if (a < 0) return b;
        if (b < 0) return a;
        const difference = this.points[a].totalSize - this.points[b].totalSize;
        return difference === 0 ? Math.min(a, b) : (maximum ? difference > 0 : difference < 0) ? a : b;
    }

    private bound(timestamp: number, upper = false): number {
        let left = 0;
        let right = this.points.length;
        while (left < right) {
            const middle = (left + right) >>> 1;
            if (this.points[middle].timestamp < timestamp || (upper && this.points[middle].timestamp === timestamp)) left = middle + 1;
            else right = middle;
        }
        return left;
    }

    private extrema(start: number, end: number): [number, number] {
        let min = -1;
        let max = -1;
        const take = (node: number): void => {
            min = this.extreme(min, this.minima[node], false);
            max = this.extreme(max, this.maxima[node], true);
        };
        const takePoint = (index: number): void => {
            min = this.extreme(min, index, false);
            max = this.extreme(max, index, true);
        };
        const firstBlock = Math.ceil(start / INDEX_BLOCK_SIZE);
        const lastBlock = Math.floor(end / INDEX_BLOCK_SIZE);
        const prefixEnd = Math.min(end, firstBlock * INDEX_BLOCK_SIZE);
        for (let index = start; index < prefixEnd; index++) takePoint(index);
        for (let index = Math.max(prefixEnd, lastBlock * INDEX_BLOCK_SIZE); index < end; index++) takePoint(index);
        for (let left = firstBlock + this.size, right = lastBlock + this.size; left < right; left >>= 1, right >>= 1) {
            if (left & 1) take(left++);
            if (right & 1) take(--right);
        }
        return [min, max];
    }

    sample(start: number, end: number, width: number, gridStart = start, gridEnd = end): OverviewPoint[] {
        const result: OverviewPoint[] = [];
        const columns = Math.max(1, Math.floor(width));
        const first = this.bound(start);
        const last = this.bound(end, true);
        const interpolate = (index: number, timestamp: number): OverviewPoint => {
            const left = this.points[index - 1];
            const right = this.points[index];
            return {
                timestamp,
                totalSize: left.totalSize + (right.totalSize - left.totalSize) *
                (timestamp - left.timestamp) / (right.timestamp - left.timestamp),
            };
        };
        if (first > 0 && first < this.points.length && this.points[first].timestamp > start) result.push(interpolate(first, start));
        let cursor = first;
        const firstColumn = Math.min(columns - 1, Math.max(0, Math.floor((start - gridStart) / Math.max(1, gridEnd - gridStart) * columns)));
        for (let column = firstColumn; column < columns && cursor < last; column++) {
            const next = column === columns - 1 ? last : Math.min(last, this.bound(gridStart + (gridEnd - gridStart) * (column + 1) / columns));
            if (next <= cursor) continue;
            const [min, max] = this.extrema(cursor, next);
            [...new Set([cursor, min, max, next - 1])].sort((a, b) => a - b).forEach(index => result.push(this.points[index]));
            cursor = next;
        }
        if (last > 0 && last < this.points.length && this.points[last - 1].timestamp < end) result.push(interpolate(last, end));
        return result;
    }

    endpoints(start: number, end: number): OverviewPoint[] {
        const first = this.bound(start);
        const last = this.bound(end, true) - 1;
        return first <= last ? [this.points[first], this.points[last]] : [];
    }
}

const indexCache = new WeakMap<OverviewPoint[], OverviewIndex>();
export const getOverviewIndex = (points: OverviewPoint[]): OverviewIndex => {
    let index = indexCache.get(points);
    if (!index) {
        index = new OverviewIndex(points);
        indexCache.set(points, index);
    }
    return index;
};

export interface OverviewSegment { points: OverviewPoint[]; start: number; end: number }

// Retain window references, never a flattened copy or a second full-data tree.
export class OverviewTimelineIndex {
    private readonly segments: Array<{ index: OverviewIndex; start: number; end: number }>;

    constructor(segments: OverviewSegment[]) {
        this.segments = segments.map(({ points, start, end }) => ({ index: getOverviewIndex(points), start, end }));
    }

    sample(start: number, end: number, width: number): OverviewPoint[] {
        const candidates: OverviewPoint[] = [];
        for (const segment of this.segments) {
            const [first, last] = segment.index.endpoints(segment.start, segment.end);
            if (!first) continue;
            if (last.timestamp < start) { candidates.push(last); continue; }
            if (first.timestamp > end) { candidates.push(first); continue; }
            if (first.timestamp < start) candidates.push(first);
            const sampled = segment.index.sample(Math.max(start, first.timestamp), Math.min(end, last.timestamp), width, start, end);
            for (const point of sampled) candidates.push(point);
            if (last.timestamp > end) candidates.push(last);
        }
        // Small pixel-sized index also clips/interpolates across sparse window boundaries.
        return new OverviewIndex(candidates).sample(start, end, width);
    }
}

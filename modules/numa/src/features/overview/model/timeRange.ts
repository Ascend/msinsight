export interface MetricTimeRange {
    startTime: number;
    endTime: number;
}

export const FULL_TIME_RANGE: MetricTimeRange = { startTime: 0, endTime: 0 };

export function normalizeTimeAnalysisRange(value: unknown): MetricTimeRange {
    if (!Array.isArray(value) || value.length < 2) return FULL_TIME_RANGE;
    const startTime = Math.max(0, Math.floor(Number(value[0])));
    const endTime = Math.max(0, Math.ceil(Number(value[1])));
    if (!Number.isSafeInteger(startTime) || !Number.isSafeInteger(endTime) || startTime >= endTime) {
        return FULL_TIME_RANGE;
    }
    return { startTime, endTime };
}

export function hasActiveTimeRange(range: MetricTimeRange): boolean {
    return range.startTime < range.endTime;
}

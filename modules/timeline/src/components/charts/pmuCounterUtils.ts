/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import type { PmuCounterRequest, PmuCounterResponse, PmuLaneMetadata, PmuNativeUnit } from '../../entity/pmuCounter';

export function getPmuTargetBuckets(width: number): number {
    return [256, 512, 1024, 2048, 4096].find(size => size >= width) ?? 4096;
}

export function buildPmuCounterRequest(
    metadata: PmuLaneMetadata, domainStart: number, domainEnd: number, offset: number, width: number,
): PmuCounterRequest {
    const startTime = Math.floor(Math.max(0, domainStart + offset));
    const endTime = Math.ceil(Math.max(startTime, domainEnd + offset));
    if (!Number.isSafeInteger(startTime) || !Number.isSafeInteger(endTime)) {
        throw new Error('PMU time range exceeds the safe relative timestamp range');
    }
    return {
        dbPath: metadata.dbPath,
        rankId: metadata.cardId,
        counterId: metadata.pmu.counterId ?? '',
        startTime,
        endTime,
        targetBuckets: getPmuTargetBuckets(width),
    };
}

export function getPmuUnitLabel(unit: PmuNativeUnit = 'unknown'): string {
    return unit === 'ns' ? 'ms' : unit;
}

export function getPmuLaneName(metadata: PmuLaneMetadata): string {
    return (metadata.pmu.eventName ?? metadata.threadName ?? '') + ' (' + getPmuUnitLabel(metadata.pmu.unit) + ')';
}

export function isPmuValue(value: number | null): value is number {
    return value !== null && Number.isFinite(value) && value >= 0;
}

export function toPmuChartSeries(data: PmuCounterResponse, offset: number): { dataListState: number[][]; intervalEnds: number[] } {
    const dataListState: number[][] = [];
    const intervalEnds: number[] = [];
    for (const segment of data.segments) {
        if (!isPmuValue(segment.value)) { continue; }
        dataListState.push([segment.startTime - offset, data.nativeUnit === 'ns' ? segment.value / 1e6 : segment.value]);
        intervalEnds.push(segment.endTime - offset);
    }
    return { dataListState, intervalEnds };
}

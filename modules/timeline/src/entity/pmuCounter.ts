/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import type { Theme } from '@emotion/react';
import type { MetaDataInnerBase } from './data';

export const PMU_GROUP_NAME = 'PMU';
export type PmuNativeUnit = 'count' | 'ns' | 'unknown';

export interface PmuMetadata {
    kind: 'group' | 'cpu' | 'event';
    nodeKey: string;
    sourceKey: string;
    counterId?: string;
    eventId?: string;
    cpu?: number;
    eventName?: string;
    eventSource?: number;
    unit?: PmuNativeUnit;
    intervalNs?: string;
    hasSamples?: boolean;
}

export interface PmuLaneMetadata extends MetaDataInnerBase {
    pmu: PmuMetadata;
    sourceLabel?: string;
}

export interface PmuCounterRequest {
    dbPath: string;
    rankId: string;
    counterId: string;
    startTime: number;
    endTime: number;
    targetBuckets: number;
}

export interface PmuSegment {
    startTime: number;
    endTime: number;
    value: number | null;
}

export interface PmuCounterResponse {
    counterId: string;
    status: 'ok' | 'empty' | 'unsupported' | 'invalid-data';
    nativeUnit: PmuNativeUnit;
    segments: PmuSegment[];
}

export type PmuChartData = PmuCounterResponse;
export interface PmuChartConfig {
    palette?: Array<keyof Theme['colorPalette']>;
}

type PmuIdentityMetadata = Pick<MetaDataInnerBase, 'dataSource' | 'dbPath'> & { pmu?: PmuMetadata };

export function getPmuDataSourceKey(dataSource: DataSource): string {
    return JSON.stringify([dataSource?.remote ?? '', dataSource?.port ?? 0,
        dataSource?.projectName ?? '', dataSource?.projectPath ?? [], dataSource?.dataPath ?? []]);
}

export function getPmuSourceIdentityKey(metadata: PmuIdentityMetadata): string {
    return JSON.stringify([getPmuDataSourceKey(metadata.dataSource), metadata.pmu?.sourceKey ?? metadata.dbPath]);
}

export function getPmuUnitIdentityKey(metadata: PmuIdentityMetadata): string {
    return JSON.stringify([getPmuSourceIdentityKey(metadata), metadata.pmu?.nodeKey ?? '']);
}

export function emptyPmuResponse(): PmuCounterResponse {
    return {
        counterId: '',
        status: 'empty',
        nativeUnit: 'unknown',
        segments: [],
    };
}

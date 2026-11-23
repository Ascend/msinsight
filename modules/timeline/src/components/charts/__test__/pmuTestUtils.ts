/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import type { PmuCounterResponse, PmuLaneMetadata, PmuSegment } from '../../../entity/pmuCounter';

export const pmuSource: DataSource = {
    remote: 'localhost', port: 9000, projectName: 'test', projectPath: ['/test'], dataPath: [], children: [],
};

export function pmuMetadata(): PmuLaneMetadata {
    return {
        dataSource: pmuSource,
        dbPath: '/test/db',
        cardId: 'rank0',
        metaType: 'PMU_COUNTER',
        offsetSide: 'host',
        pmu: {
            kind: 'event',
            sourceKey: 'db-key',
            nodeKey: 'counter-1',
            counterId: '1',
            eventId: '2',
            cpu: 7,
            eventName: 'instructions',
            eventSource: 2,
            unit: 'count',
            intervalNs: '100',
            hasSamples: true,
        },
    };
}

export function pmuSegment(overrides: Partial<PmuSegment> = {}): PmuSegment {
    return { startTime: 10, endTime: 20, value: 3, ...overrides };
}

export function pmuResponse(overrides: Partial<PmuCounterResponse> = {}): PmuCounterResponse {
    return {
        counterId: '1',
        status: 'ok',
        nativeUnit: 'count',
        segments: [pmuSegment()],
        ...overrides,
    };
}

export function deferred<T>(): { promise: Promise<T>; resolve: (value: T) => void; reject: (reason?: unknown) => void } {
    let resolve!: (value: T) => void;
    let reject!: (reason?: unknown) => void;
    const promise = new Promise<T>((_resolve, _reject) => { resolve = _resolve; reject = _reject; });
    return { promise, resolve, reject };
}

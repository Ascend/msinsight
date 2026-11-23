/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import { buildPmuCounterRequest, getPmuLaneName, getPmuTargetBuckets, getPmuUnitLabel, toPmuChartSeries } from './pmuCounterUtils';
import { getPmuSourceIdentityKey, getPmuUnitIdentityKey } from '../../entity/pmuCounter';
import { pmuMetadata, pmuResponse, pmuSegment } from './__test__/pmuTestUtils';

describe('PMU interval semantics', () => {
    it('adapts explicit ranges without turning gaps into zero or extending the tail', () => {
        const data = pmuResponse({
            segments: [
                pmuSegment({ value: 0 }),
                pmuSegment({ startTime: 20, endTime: 30, value: null }),
                pmuSegment({ startTime: 30, endTime: 40, value: 6 }),
            ],
        });
        expect(toPmuChartSeries(data, 5)).toEqual({
            dataListState: [[5, 0], [25, 6]], intervalEnds: [15, 35],
        });
        expect(data.segments[0].startTime).toBe(10);
        expect(data.segments[1].value).toBeNull();
    });

    it('converts time values once for both the metric scale and tooltip', () => {
        const data = pmuResponse({ nativeUnit: 'ns', segments: [pmuSegment({ value: 2000 })] });
        expect(toPmuChartSeries(data, 0)).toEqual({ dataListState: [[10, 0.002]], intervalEnds: [20] });
        expect(data.segments[0].value).toBe(2000);
        expect(toPmuChartSeries(pmuResponse({ segments: [pmuSegment({ value: null })] }), 0))
            .toEqual({ dataListState: [], intervalEnds: [] });
    });

    it('applies the Host offset only to the request and rounds outwards in relative ns', () => {
        const request = buildPmuCounterRequest(pmuMetadata(), 20.2, 100.1, 5, 600);
        expect(request).toEqual({ dbPath: '/test/db', rankId: 'rank0', counterId: '1', startTime: 25, endTime: 106, targetBuckets: 1024 });
        expect(buildPmuCounterRequest(pmuMetadata(), 0, 100, -20, 100).startTime).toBe(0);
        expect(() => buildPmuCounterRequest(pmuMetadata(), 0, 1e19, 0, 100)).toThrow('safe');
        expect(getPmuTargetBuckets(8000)).toBe(4096);
    });

    it('keeps event names and time units consistent', () => {
        const metadata = pmuMetadata();
        expect(getPmuLaneName(metadata)).toBe('instructions (count)');
        expect(getPmuLaneName({ ...metadata, pmu: { ...metadata.pmu, unit: 'ns' } })).toBe('instructions (ms)');
        expect(getPmuUnitLabel('ns')).toBe('ms');
        expect(getPmuUnitLabel('unknown')).toBe('unknown');
    });

    it('isolates group and leaf identities by connection and DB registration, independent of rank', () => {
        const meta = pmuMetadata();
        expect(getPmuSourceIdentityKey({ ...meta, cardId: 'rank1' } as typeof meta)).toBe(getPmuSourceIdentityKey(meta));
        expect(getPmuSourceIdentityKey({ ...meta, dataSource: { ...meta.dataSource, port: 2 } })).not.toBe(getPmuSourceIdentityKey(meta));
        expect(getPmuSourceIdentityKey({ ...meta, pmu: { ...meta.pmu, sourceKey: 'other' } })).not.toBe(getPmuSourceIdentityKey(meta));
        expect(getPmuUnitIdentityKey({ ...meta, pmu: { ...meta.pmu, nodeKey: 'other' } })).not.toBe(getPmuUnitIdentityKey(meta));
    });
});

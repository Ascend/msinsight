import type { CounterMetaData } from '../../../entity/data';
import { getCounterLaneDisplayName, getCounterLegend, getCounterSeriesMode } from '../counterUnit';

const createCounterMetaData = (threadName: string, dataType: string[]): CounterMetaData => ({
    dataSource: { remote: 'local' } as unknown as DataSource,
    cardId: '0',
    dbPath: '/tmp/test.db',
    metaType: 'TEST',
    processId: '1',
    threadName,
    dataType,
});

describe('getCounterLaneDisplayName', () => {
    it('appends the unit from parenthesized data type', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('CPU 0', ['Usage(%)']))).toBe('CPU 0 (%)');
        expect(getCounterLaneDisplayName(createCounterMetaData('HBM 0 read/Bandwidth', ['Bandwidth(Byte/s)'])))
            .toBe('HBM 0 read/Bandwidth (Byte/s)');
        expect(getCounterLaneDisplayName(createCounterMetaData('AI Core Freq', ['Frequency(Mhz)'])))
            .toBe('AI Core Freq (Mhz)');
        expect(getCounterLaneDisplayName(createCounterMetaData('CPU 239', ['Frequency(KHz)'])))
            .toBe('CPU 239 (KHz)');
    });

    it('appends unit-only data type values', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('roceTxPkt', ['Packet/s']))).toBe('roceTxPkt (Packet/s)');
        expect(getCounterLaneDisplayName(createCounterMetaData('rxBytes', ['Byte']))).toBe('rxBytes (Byte)');
        expect(getCounterLaneDisplayName(createCounterMetaData('Total Cycle', ['Cycle']))).toBe('Total Cycle (Cycle)');
    });

    it('appends one unit for multi-series counters with the same unit', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('DDR', ['Read(Byte/s)', 'Write(Byte/s)']))).toBe('DDR (Byte/s)');
    });

    it('keeps the original name for mixed units', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('Mixed', ['Usage(%)', 'Bandwidth(Byte/s)']))).toBe('Mixed');
    });

    it('does not duplicate an existing unit suffix', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('CPU 0 (%)', ['Usage(%)']))).toBe('CPU 0 (%)');
        expect(getCounterLaneDisplayName(createCounterMetaData('CPU 0 (ratio)', ['Usage(%)']))).toBe('CPU 0 (ratio)');
    });

    it('keeps the original name when no clear unit exists', () => {
        expect(getCounterLaneDisplayName(createCounterMetaData('Unknown Counter', ['unknownMetric']))).toBe('Unknown Counter');
    });
});

describe('getCounterSeriesMode', () => {
    it('overlays the two independent UB port values and exposes their legend', () => {
        const metadata = createCounterMetaData('UB Port004', ['Rx Bandwidth(Byte/s)', 'Tx Bandwidth(Byte/s)']);
        metadata.metaType = 'UB';
        expect(getCounterSeriesMode(metadata)).toBe('overlay');
        expect(getCounterLegend(metadata)).toEqual(['Rx Bandwidth(Byte/s)', 'Tx Bandwidth(Byte/s)']);
    });

    it('keeps existing counters and single-value UB counters stacked without a legend', () => {
        const ubMetadata = createCounterMetaData('UB Port004', ['Rx Bandwidth(Byte/s)']);
        ubMetadata.metaType = 'UB';
        expect(getCounterSeriesMode(ubMetadata)).toBe('stacked');
        expect(getCounterLegend(ubMetadata)).toBeUndefined();
        expect(getCounterSeriesMode(createCounterMetaData('DDR', ['Read(Byte/s)', 'Write(Byte/s)']))).toBe('stacked');
    });
});

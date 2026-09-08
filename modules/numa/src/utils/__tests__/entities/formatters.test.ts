/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import i18n from '@insight/lib/i18n';
import { formatMetric, formatMetricValue, formatTimelineTimestamp } from '@/entities/numa/formatters';
import { getMetricDescription } from '@/entities/numa/metricDescriptions';
import type { Metric } from '@/entities/numa/types';

const metric: Metric = {
    key: 'crossSocketRead', label: 'Cross Socket Read Traffic', description: 'Database description.', unit: 'GOps', value: 1,
};
const description = (item: Metric, locale: 'zhCN' | 'enUS'): string =>
    getMetricDescription(item, locale, i18n.getFixedT(locale, 'numa'));

describe('NUMA formatters', () => {
    it.each([
        [9.999, '10.00 GOps'],
        [10, '10.0 GOps'],
        [99.99, '100.0 GOps'],
        [100, '100 GOps'],
        [-10.001, '-10.0 GOps'],
        [1.2, '1.20 GOps'],
    ])('formats metric value %s by magnitude', (value, expected) => {
        expect(formatMetricValue(value, 'GOps')).toBe(expected);
    });

    it('formats unavailable and legacy metrics', () => {
        const item = {
            key: 'dramRead', label: 'DRAM Read Traffic', description: '', unit: 'GB', value: 1.25,
        };
        expect(formatMetric({ ...item, hasValue: false }, 'No data')).toBe('No data');
        expect(formatMetric(item, 'No data')).toBe('1.25 GB');
        expect(formatMetricValue(1.2, '')).toBe('1.20');
    });

    it.each([
        [99, 100, '00:00.000.000.000'],
        [59_999_999_999, 0, '00:59.999.999.999'],
        [60_000_000_000, 0, '01:00.000.000.000'],
        [3_600_000_000_000, 0, '01:00:00.000.000.000'],
        [3 * 3_600_000_000_000 + 4 * 60_000_000_000 + 5 * 1_000_000_000 + 6 * 1_000_000 + 7 * 1_000 + 108,
            100, '03:04:05.006.007.008'],
    ])('formats Timeline timestamp %s from origin %s', (timestamp, origin, expected) => {
        expect(formatTimelineTimestamp(timestamp, origin)).toBe(expected);
    });

    it('uses database descriptions, localization, and fallback text', () => {
        const zh = i18n.getFixedT('zhCN', 'numa');
        expect(description(metric, 'enUS')).toBe('Database description.');
        expect(description(metric, 'zhCN')).toBe(zh('metricDescriptions.crossSocketRead'));
        expect(description({ ...metric, key: 'vendorMetric', description: '' }, 'zhCN'))
            .toBe(zh('metricDescriptions.noDescription'));
    });
});

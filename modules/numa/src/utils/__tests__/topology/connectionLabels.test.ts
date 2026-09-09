import type { Connection, Metric } from '@/entities/numa/types';
import {
    buildConnectionMetricLabels,
    METRIC_LABEL_OFFSET,
    segmentsCross,
} from '@/features/topology/model/connectionLabels';
import type { Point } from '@/features/topology/model/types';

const metric = (key: string, value: number, hasValue: boolean = true): Metric => ({
    key, value, hasValue, unit: 'GB', label: key, description: '',
});

const connection = (type: Connection['type'], metrics: Metric[]): Connection => ({
    id: type,
    type,
    source: 'source',
    target: 'target',
    label: type,
    description: '',
    metrics,
});

describe('connection metric labels', () => {
    it.each([
        [{ x: 100, y: 0 }, ['21.5 GB →', '← 27.3 GB'], ['middle', 'middle']],
        [{ x: 0, y: 100 }, ['21.5 GB ↓', '↑ 27.3 GB'], ['end', 'start']],
    ])('keeps directional labels readable for vector %o', (target: Point, texts, anchors) => {
        const labels = buildConnectionMetricLabels(connection('socket', [
            metric('sourceToTarget', 21.5),
            metric('targetToSource', 27.3),
        ]), { x: 0, y: 0 }, target, false);

        expect(labels.map(({ text, textAnchor }) => [text, textAnchor])).toEqual([
            [texts[0], anchors[0]],
            [texts[1], anchors[1]],
        ]);
        expect(labels.every(({ rotation }) => rotation === 0)).toBe(true);
    });

    it('renders a directionless NUMA summary and placeholders for missing memory metrics', () => {
        const numaLabels = buildConnectionMetricLabels(
            connection('numa', [{ ...metric('crossScclRead', 0.06), unit: 'GOps' }]),
            { x: 0, y: 0 },
            { x: 0, y: 100 },
            false,
        );
        expect(numaLabels[0]).toMatchObject({
            text: '0.06 GOps',
            position: { x: METRIC_LABEL_OFFSET, y: 50 },
            rotation: 0,
        });

        const memoryLabels = buildConnectionMetricLabels(
            connection('memory', [metric('dramRead', 0, false)]),
            { x: 0, y: 0 },
            { x: 100, y: 0 },
            false,
        );
        expect(memoryLabels.map(({ text }) => text)).toEqual(['← --', '-- →']);
    });

    it.each([Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY])(
        'renders a placeholder for non-finite metric values (%s)',
        value => {
            const labels = buildConnectionMetricLabels(
                connection('numa', [metric('crossScclRead', value)]),
                { x: 0, y: 0 },
                { x: 0, y: 100 },
                false,
            );

            expect(labels[0].text).toBe('--');
        },
    );

    it('separates crossing labels and caps their text length', () => {
        const labels = buildConnectionMetricLabels(connection('socket', [
            metric('sourceToTarget', 123456789),
            metric('targetToSource', 987654321),
        ]), { x: 0, y: 0 }, { x: 100, y: 100 }, true);

        expect(labels.map(({ progress }) => progress)).toEqual([0.68, 0.32]);
        expect(labels.every(({ rotation }) => rotation === 45)).toBe(true);
        expect(labels.every(({ maxLength }) => maxLength < 40)).toBe(true);
    });

    it.each([
        [[{ x: 0, y: 0 }, { x: 100, y: 100 }], [{ x: 0, y: 100 }, { x: 100, y: 0 }], true],
        [[{ x: 0, y: 0 }, { x: 100, y: 0 }], [{ x: 100, y: 0 }, { x: 100, y: 100 }], false],
        [[{ x: 0, y: 0 }, { x: 100, y: 0 }], [{ x: 0, y: 20 }, { x: 100, y: 20 }], false],
    ])('detects only interior segment crossings', (first, second, expected) => {
        expect(segmentsCross(first as [Point, Point], second as [Point, Point])).toBe(expected);
    });
});

import type { TFunction } from 'i18next';
import type { Locale, Metric } from './types';

const LOCALIZED_METRIC_KEYS = new Set([
    'crossSocketRead',
    'totalCrossSocketRead',
    'crossScclRead',
    'totalRead',
    'innerRead',
    'dramRead',
    'dramWrite',
    'llcTraffic',
    'totalDramTraffic',
    'sourceToTarget',
    'targetToSource',
]);

export function getMetricDescription(
    metric: Metric,
    locale: Locale,
    t: TFunction<'numa'>,
): string {
    if (locale === 'zhCN' && LOCALIZED_METRIC_KEYS.has(metric.key)) {
        return t(`metricDescriptions.${metric.key}`);
    }
    return metric.description || t('metricDescriptions.noDescription');
}

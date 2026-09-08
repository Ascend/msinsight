import React from 'react';
import type { TFunction } from 'i18next';
import { formatMetric } from '@/entities/numa/formatters';
import type { Locale, Metric } from '@/entities/numa/types';
import { MetricLabel } from './MetricLabel';

interface SystemMetricsProps {
    metrics: Metric[] | null;
    locale: Locale;
    onSelect: () => void;
    t: TFunction<'numa'>;
}

export const SystemMetrics: React.FC<SystemMetricsProps> = ({ metrics, locale, onSelect, t }) => {
    if (metrics === null) return null;
    return (
        <section className="metric-strip" aria-label={t('systemMetrics')}>
            {metrics.map((metric, index) => (
                <button
                    key={`${metric.key}-${index}`}
                    className="metric-card"
                    onClick={onSelect}
                >
                    <MetricLabel metric={metric} locale={locale} t={t} />
                    <strong>{formatMetric(metric, t('noDataCollected'))}</strong>
                </button>
            ))}
        </section>
    );
};

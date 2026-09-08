import React from 'react';
import type { TFunction } from 'i18next';
import { formatMetric } from '@/entities/numa/formatters';
import type { Locale, Metric } from '@/entities/numa/types';
import { MetricLabel } from './MetricLabel';

interface DetailsPanelProps {
    title: string;
    description: string;
    metrics: Metric[];
    locale: Locale;
    t: TFunction<'numa'>;
}

export const DetailsPanel: React.FC<DetailsPanelProps> = ({ title, description, metrics, locale, t }) => (
    <aside className="details-panel">
        <div className="details-heading">
            <div>
                <span className="panel-kicker">{t('details')}</span>
                <h2>{title}</h2>
            </div>
        </div>
        <p className="details-description">{description}</p>
        <div className="details-metrics">
            {metrics.map((metric, index) => (
                <article key={`${metric.key}-${index}`} className="detail-row">
                    <div><MetricLabel metric={metric} locale={locale} t={t} /></div>
                    <strong>{formatMetric(metric, t('noDataCollected'))}</strong>
                </article>
            ))}
            {metrics.length === 0 && (
                <div className="detail-placeholder">{t('selectTarget')}</div>
            )}
        </div>
    </aside>
);

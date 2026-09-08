import React from 'react';
import { MITooltipHelp } from '@insight/lib/components';
import type { TFunction } from 'i18next';
import { getMetricDescription } from '@/entities/numa/metricDescriptions';
import type { Locale, Metric } from '@/entities/numa/types';

interface MetricLabelProps {
    metric: Metric;
    locale: Locale;
    t: TFunction<'numa'>;
}

export const MetricLabel: React.FC<MetricLabelProps> = ({ metric, locale, t }) => {
    const description = getMetricDescription(metric, locale, t);
    return (
        <span className="metric-label-with-help">
            <span>{metric.label}</span>
            <span className="metric-help" aria-label={description}>
                <MITooltipHelp
                    title={description}
                    placement="topLeft"
                    color="var(--surface)"
                    overlayStyle={{ maxWidth: 360 }}
                    overlayInnerStyle={{
                        color: 'var(--text)',
                        backgroundColor: 'var(--surface)',
                        border: '1px solid var(--border)',
                    }}
                />
            </span>
        </span>
    );
};

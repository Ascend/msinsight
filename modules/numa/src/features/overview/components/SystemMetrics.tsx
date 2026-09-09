/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

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

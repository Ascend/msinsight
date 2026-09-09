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

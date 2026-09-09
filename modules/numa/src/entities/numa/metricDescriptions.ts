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

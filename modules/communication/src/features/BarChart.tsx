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
import React, { useMemo } from 'react';
import { useTheme } from '@emotion/react';
import { format, type EChartsOption } from 'echarts';
import { useTranslation } from 'react-i18next';
import type { BarResult } from '../domain/types';
import { Chart } from '../components/Chart';

const AXIS_LABEL_WIDTH = 120;

/** Fit both ends within the axis label width, including mixed Chinese and Latin names. */
function middleEllipsis(text: string, font: string): string {
    const fits = (value: string): boolean => format.getTextRect(value, font).width <= AXIS_LABEL_WIDTH;
    if (fits(text)) { return text; }
    const characters = Array.from(text);
    const shorten = (count: number): string => `${characters.slice(0, Math.ceil(count / 2)).join('')}…${characters.slice(characters.length - Math.floor(count / 2)).join('')}`;
    let low = 0; let high = characters.length - 1;
    while (low < high) {
        const count = Math.ceil((low + high) / 2);
        if (fits(shorten(count))) { low = count; } else { high = count - 1; }
    }
    return shorten(low);
}

export function BarChart({ data, dark, metric, unit }: {
    data: BarResult; dark: boolean; metric: string; unit?: string;
}): JSX.Element {
    const theme = useTheme();
    const { t } = useTranslation('communication');
    const axisName = unit ? t('modern.metricWithUnit', { metric, unit }) : metric;
    const option = useMemo<EChartsOption>(() => {
        const items = data.items;
        const unitSuffix = unit ? ` ${unit}` : '';
        const fontSize = Number.parseFloat(theme.fontSizeExtraSmall);
        const labels = new Map(items.map(item => [item.name, middleEllipsis(item.name, `${fontSize}px ${theme.fontFamily}`)]));
        return {
            animation: false,
            legend: { show: false },
            // Reserve space for rotated labels, including the first label's left edge and the zoom slider.
            grid: { left: 110, right: 20, top: 50, bottom: items.length > 20 ? 60 : 16, containLabel: true },
            tooltip: {
                trigger: 'axis',
                valueFormatter: value => typeof value === 'number' && Number.isFinite(value) ? `${value.toFixed(6)}${unitSuffix}` : '-',
                renderMode: 'html',
                confine: true,
                enterable: true,
                extraCssText: 'max-width: min(360px, calc(100% - 32px)); max-height: calc(100% - 16px); overflow: auto; white-space: normal; overflow-wrap: anywhere;',
            },
            xAxis: {
                type: 'category',
                data: items.map(item => item.name),
                axisLabel: { rotate: 40, width: AXIS_LABEL_WIDTH, fontSize, fontFamily: theme.fontFamily, formatter: (name: string) => labels.get(name) ?? name },
            },
            yAxis: { type: 'value', nameLocation: 'end', nameTextStyle: { align: 'center' }, name: axisName },
            dataZoom: items.length > 20 ? [{ type: 'slider', start: 0, end: Math.min(100, 20 / items.length * 100), bottom: 0 }] : [],
            series: [{ type: 'bar', name: metric, barMaxWidth: 40, itemStyle: { color: theme.primaryColor }, data: items.map(item => item.value) }],
        };
    }, [data, theme, metric, axisName, unit]);
    return <Chart option={option} dark={dark} label={axisName} />;
}

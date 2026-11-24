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
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import { Button, Checkbox, InputNumber, Select, Tooltip } from '@insight/lib/components';
import { ResetIcon } from '@insight/lib/icon';
import type { ChartsHandle } from '@insight/lib/components/MIChart';
import type { MatrixEdge } from '../../domain/types';
import { Chart } from '../../components/Chart';
import { Result } from '../../components/Section';
import type { Query } from '../../state/useQuery';
import { matrixCells, matrixExtent, matrixRanks, type Metric, type ValueRange } from './data';
import { matrixHeight, matrixOptions, updateMatrixLabels, resetMatrixZoom } from './options';
import { FilterLabel, FilterRow } from '../../components/filters';
import { ChartToolbar, ChartHint, ResetZoomButton } from '../../components/chartToolbar';
import './style.css';

const InnerCheckbox = styled(Checkbox)({
    display: 'inline-flex', alignItems: 'center', '.ant-checkbox': { top: 0 },
});

const EMPTY_EDGES: MatrixEdge[] = [];

export function Matrix({ query, dark, stage, operatorFilter }: {
    query: Query<MatrixEdge[]>; dark: boolean; stage: string; operatorFilter: React.ReactNode;
}): JSX.Element {
    const edges = query.data ?? EMPTY_EDGES;
    const { t } = useTranslation('communication');
    const theme = useTheme();
    const [metric, setMetric] = useState<Metric>('bandwidth');
    const [inner, setInner] = useState(false);
    const ranks = useMemo(() => matrixRanks(edges, stage), [edges, stage]);
    const extent = useMemo(() => matrixExtent(matrixCells(edges, ranks, metric, inner)), [edges, ranks, metric, inner]);
    const [range, setRange] = useState<ValueRange>(extent);
    const [applied, setApplied] = useState<ValueRange>();
    const chart = useRef<ChartsHandle>(null);
    const [zoomed, setZoomed] = useState(false);
    useEffect(() => { setRange(extent); setApplied(undefined); }, [extent, ranks.length]);
    const invalid = range[0] !== null && range[1] !== null && range[0] > range[1];
    const option = useMemo(() => matrixOptions(
        matrixCells(edges, ranks, metric, inner, applied), ranks, metric,
        [applied?.[0] ?? extent[0], applied?.[1] ?? extent[1]], theme, t,
    ), [edges, ranks, metric, inner, applied, extent, theme, t]);
    useEffect(() => { setZoomed(false); }, [option]);
    const onZoom = useCallback(() => {
        const instance = chart.current?.chartInstance;
        if (instance) { setZoomed(updateMatrixLabels(instance)); }
    }, []);
    const events = useMemo(() => ({ datazoom: onZoom }), [onZoom]);
    return <>
        <FilterRow>
            {operatorFilter}
            <FilterLabel>{t('searchCriteria.CommunicationMatrixType')} <Select aria-label={t('searchCriteria.CommunicationMatrixType')} value={metric}
                width={200} onChange={setMetric} options={[
                    { value: 'bandwidth', label: `${t('searchCriteria.Bandwidth')}(GB/s)` },
                    { value: 'transitSize', label: `${t('searchCriteria.TransitSize')}(MB)` },
                    { value: 'transportType', label: t('searchCriteria.TransportType') },
                    { value: 'transitTime', label: `${t('searchCriteria.TransitTime')}(ms)` },
                ]} /></FilterLabel>
            <InnerCheckbox checked={inner} onChange={event => setInner(event.target.checked)}>
                <FilterLabel as="span">{t('searchCriteria.ShowInnerCommunication')}</FilterLabel>
            </InnerCheckbox>
            {metric !== 'transportType' && <><FilterLabel as="span">{t('searchCriteria.VisibleRange')}</FilterLabel>
                <InputNumber aria-label={t('buttonText:Min')} size="small" min={extent[0]} max={extent[1]} step={0.1} value={range[0]}
                    onChange={value => setRange([typeof value === 'number' ? value : null, range[1]])} />~
                <InputNumber aria-label={t('buttonText:Max')} size="small" min={extent[0]} max={extent[1]} step={0.1} value={range[1]}
                    onChange={value => setRange([range[0], typeof value === 'number' ? value : null])} />
                <Button type="primary" disabled={invalid} onClick={() => setApplied(range)}>{t('Confirm', { ns: 'buttonText' })}</Button>
                {invalid && <span role="alert">{t('buttonText:limitMinMax')}</span>}</>}
        </FilterRow>
        <Result query={query} empty={!query.data?.length}>
            {() => <>
                <ChartToolbar>
                    <ChartHint>{t('modern.matrixNavigation')}</ChartHint>
                    <Tooltip title={t('Reset Zoom')}><ResetZoomButton type="text" aria-label={t('Reset Zoom')} disabled={!zoomed}
                        icon={<ResetIcon disabled={!zoomed} />}
                        onClick={() => { if (chart.current?.chartInstance) { resetMatrixZoom(chart.current.chartInstance); } }} /></Tooltip>
                </ChartToolbar>
                {/* ECharts consumes wheel defaults even when its modifier condition is unmet. */}
                <div className="communication-matrix-plot" onWheelCapture={event => { if (!event.ctrlKey) { event.stopPropagation(); } }}>
                    <Chart ref={chart} option={option} dark={dark} height={matrixHeight(ranks.length)} label={t('modern.matrix')} onEvents={events} />
                </div>
            </>}
        </Result>
    </>;
}

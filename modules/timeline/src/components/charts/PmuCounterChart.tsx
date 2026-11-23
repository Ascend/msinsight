/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
import React, { useEffect, useMemo } from 'react';
import { observer } from 'mobx-react-lite';
import { runInAction } from 'mobx';
import { useTheme } from '@emotion/react';
import { useTranslation } from 'react-i18next';
import type { ChartProps } from '../../entity/chart';
import type { PmuLaneMetadata } from '../../entity/pmuCounter';
import { colorPalette } from '../../insight/units/utils';
import { hashToNumber } from '../../utils/colorUtils';
import { FilledLineChartRenderer } from './FilledLineChart';
import { usePmuCounterData } from './usePmuCounterData';
import { getPmuLaneName, toPmuChartSeries } from './pmuCounterUtils';

/** Adapt interval data to the same renderer used by CPU/NPU metric counters. */
export const PmuCounterChart = observer(({ session, unit, metadata, width, height, margin, title, palette }: ChartProps<'pmuCounter'>): JSX.Element => {
    const pmuMetadata = metadata as PmuLaneMetadata;
    const { t } = useTranslation('timeline');
    const theme = useTheme();
    const { data, loading, error, retry, offset } = usePmuCounterData(session, pmuMetadata, unit, width);
    const series = useMemo(() => toPmuChartSeries(data, offset), [data, offset]);
    const eventName = pmuMetadata.pmu.eventName ?? pmuMetadata.threadName ?? '';
    const colorKey = colorPalette[hashToNumber(eventName + (pmuMetadata.threadName ?? eventName), colorPalette.length)];
    useEffect(() => {
        runInAction(() => { unit.counterRenderWidth = width; });
        return () => { runInAction(() => { unit.counterRenderWidth = 0; }); };
    }, [unit, width]);

    let message = '';
    if (loading) {
        message = data.segments.length === 0 ? t('pmuCounter.loading') : '';
    } else if (error) {
        message = t('pmuCounter.queryFailed');
    } else if (data.status === 'unsupported') {
        message = t('pmuCounter.unsupported');
    } else if (data.status === 'invalid-data') {
        message = t('pmuCounter.invalidData');
    } else if (data.segments.length === 0) {
        message = t(pmuMetadata.pmu.hasSamples === false ? 'pmuCounter.noSamples' : 'pmuCounter.noDataInRange');
    }
    return <FilledLineChartRenderer session={session} unit={unit} metadata={metadata} width={width} height={height}
        margin={margin} title={title} palette={palette ?? [colorKey]} dataListState={series.dataListState} intervalEnds={series.intervalEnds}
        renderTooltip={value => new Map([[getPmuLaneName(pmuMetadata), `${value[1]}`]])}>
        {message && <div role="status" style={{ position: 'absolute', top: '42%', width: '100%', textAlign: 'center', color: theme.fontColor }}>
            {message} {error && <button onClick={event => { event.stopPropagation(); retry(); }}>{t('pmuCounter.retry')}</button>}
        </div>}
    </FilledLineChartRenderer>;
});

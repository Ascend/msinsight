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

import React, { useMemo, useRef, useEffect } from 'react';
import { MIChart, type ChartsHandle } from '@insight/lib/components';
import type { EChartsOption, GridComponentOption as GridOption, YAXisComponentOption as YAXisOption, DataZoomComponentOption as DataZoomOption } from 'echarts';
import { observer } from 'mobx-react';
import { type Theme, useTheme } from '@emotion/react';
import { formatTime } from '@/utils/utils';

/** 时间范围 */
type Range = [number, number];

type DataZoomItem = {
    batch?: DataZoomItem[];
    dataZoomId?: string;
    end?: number;
    start?: number;
    type?: string;
};

/** 组件 Props */
type DataZoomProps = {
    dataSource: Array<[number, number]>; // 缩略图趋势数据源
    minTime: number; // 时间范围最小值
    maxTime: number; // 时间范围最大值
    width?: string | number; // chart 宽度
    dataZoomHeight?: number; // dataZoom 高度
    offsetLeft?: number; // dataZoom 左侧偏移量(与独立的主图左对齐)
    offsetRight?: number; // dataZoom 右侧偏移量(与独立的主图右对齐)
    selectedZoomChange?: (range: Range) => void; // dataZoom 选中范围改变回调
    selectedRange?: Range;
    module: 'leaks' | 'memsnapshot'; // 数据类型，影响时间戳转化
};

type GetOptionParams = {
    dataSource: Array<[number, number]>;
    isDataZoom: boolean;
    height?: number;
    dataZoomHeight?: number;
    offsetLeft?: number;
    offsetRight?: number;
    minTime: number;
    maxTime: number;
    theme: Theme;
    module: string;
};

const DATA_ZOOM_HEIGHT = 30; // dataZoom默认高度
const DATA_ZOOM_OFFSET = 6; // dataZoom偏移量（顶部滑块的高度）

function getGridOption(offsetLeft?: number, offsetRight?: number): GridOption {
    const grid = { top: 0, bottom: 0 } as GridOption;
    if (offsetLeft) {
        grid.left = offsetLeft;
    }
    if (offsetRight) {
        grid.right = offsetRight;
    }
    return grid;
}

function getYAxisOption(isDataZoom: boolean, dataSource: Array<[number, number]>): YAXisOption {
    const yAxis = {
        type: 'value',
        axisLine: { show: false },
        axisTick: { show: false },
        axisLabel: { show: false },
        splitLine: { show: false },
    } as YAXisOption;
    if (!isDataZoom && dataSource.length > 0) {
        let dataZoomYMin = Infinity;
        let dataZoomYMax = 0;
        dataSource.forEach(item => {
            if (item[1] < dataZoomYMin) {
                dataZoomYMin = item[1];
            }
            if (item[1] > dataZoomYMax) {
                dataZoomYMax = item[1];
            }
        });
        yAxis.min = dataZoomYMin;
        yAxis.max = dataZoomYMax;
    }
    return yAxis;
}

function getDataZoomOption(module: string, dataZoomHeight?: number): DataZoomOption[] {
    return [
        {
            type: 'slider',
            top: 0,
            realtime: true,
            xAxisIndex: 0,
            showDataShadow: false,
            height: dataZoomHeight ?? DATA_ZOOM_HEIGHT,
            backgroundColor: 'transparent',
            moveOnMouseMove: false, // 禁止在非选中区域或滑块外拖动
            labelFormatter: (val: number) => formatTime(val, module),
            minSpan: 1,
        },
        {
            type: 'inside',
            xAxisIndex: 0,
            realtime: true,
            moveOnMouseMove: false,
        },
    ] as DataZoomOption[];
}

function getOptions({ dataSource, minTime, maxTime, isDataZoom, dataZoomHeight, offsetLeft, offsetRight, theme, module }: GetOptionParams): EChartsOption {
    return {
        animation: false,
        grid: getGridOption(offsetLeft, offsetRight),
        xAxis: {
            type: 'value',
            boundaryGap: false,
            min: minTime,
            max: maxTime,
            axisLine: { show: false },
            axisTick: { show: false },
            axisLabel: { show: false },
            splitLine: { show: false },
        },
        yAxis: getYAxisOption(isDataZoom, dataSource),
        series: [
            {
                type: 'line',
                color: 'rgba(24, 144, 255, 0.25)',
                symbol: 'none',
                lineStyle: isDataZoom ? { opacity: 0 } : { width: 1, color: '#516489' }, // 不显示主图线
                areaStyle: isDataZoom ? { opacity: 0 } : { color: theme.mode === 'dark' ? '#2A2F37' : '#D4E1FD' },
                data: dataSource,
            },
        ],
        dataZoom: isDataZoom ? getDataZoomOption(module, dataZoomHeight) : undefined,
    } as EChartsOption;
}

const MemoryDataZoom = observer(
    ({
        width,
        dataZoomHeight,
        offsetLeft,
        offsetRight,
        dataSource,
        minTime,
        maxTime,
        module,
        selectedZoomChange,
        selectedRange,
    }: DataZoomProps): React.ReactElement => {
        const theme = useTheme();
        const chartRef = useRef<ChartsHandle | null>(null);
        const dataZoomRef = useRef<ChartsHandle | null>(null);
        const timeRangeRef = useRef<Range>([minTime, maxTime]);
        const syncingRef = useRef(false);

        /**
         * 趋势图配置项
         */
        const chartOptions: EChartsOption = useMemo(() => {
            return getOptions({ dataSource, minTime, maxTime, isDataZoom: false, height: 0, dataZoomHeight, offsetLeft, offsetRight, theme, module });
        }, [dataSource, dataZoomHeight, offsetLeft, offsetRight, maxTime, minTime, theme, module]);

        /**
         * 缩略图配置项
         */
        const dataZoomOptions: EChartsOption = useMemo(() => {
            const options = getOptions({ dataSource, minTime, maxTime, isDataZoom: true, height: 0, dataZoomHeight, offsetLeft, offsetRight, theme, module });
            if (selectedRange !== undefined && minTime < maxTime && Array.isArray(options.dataZoom)) {
                const offsetTime = maxTime - minTime;
                const start = Math.max(0, Math.min(100, (selectedRange[0] - minTime) / offsetTime * 100));
                const end = Math.max(0, Math.min(100, (selectedRange[1] - minTime) / offsetTime * 100));
                options.dataZoom = options.dataZoom.map(item => ({ ...item, start, end }));
            }
            return options;
        }, [dataSource, dataZoomHeight, offsetLeft, offsetRight, maxTime, minTime, selectedRange?.[0], selectedRange?.[1], theme, module]);

        const handleDataZoom = (params: any): void => {
            if (syncingRef.current) return;
            const { start, end, batch } = params as DataZoomItem;
            const batchItem = batch?.[0];
            const _start = batchItem?.start ?? start;
            const _end = batchItem?.end ?? end;
            if (_start === undefined || _end === undefined) return;
            const [_minTime, _maxTime] = timeRangeRef.current;
            const offsetTime = _maxTime - _minTime;
            const startTime = _minTime + Math.floor(offsetTime * _start / 100);
            const endTime = _minTime + Math.floor(offsetTime * _end / 100);
            selectedZoomChange?.([startTime, endTime]);
        };

        useEffect(() => {
            timeRangeRef.current = [minTime, maxTime];
        }, [minTime, maxTime]);

        useEffect(() => {
            if (!selectedZoomChange) return;
            let disposed: boolean = false;
            let chartInstance = dataZoomRef.current?.getInstance();

            const retryGetInstance = (): void => {
                if (disposed) return;

                chartInstance = dataZoomRef.current?.getInstance();
                if (!chartInstance) {
                    requestAnimationFrame(retryGetInstance);
                    return;
                }
                chartInstance.on('dataZoom', handleDataZoom);
            };

            !chartInstance && retryGetInstance();

            return (): void => {
                disposed = true;
                chartInstance?.off('dataZoom', handleDataZoom);
            };
        }, []);

        useEffect(() => {
            if (!selectedZoomChange) return;
            const chartInstance = dataZoomRef.current?.getInstance();
            chartInstance?.off('dataZoom', handleDataZoom);
            chartInstance?.on('dataZoom', handleDataZoom);
        }, [dataSource, maxTime, minTime]);

        useEffect(() => {
            if (selectedRange === undefined || minTime >= maxTime) return;
            const chartInstance = dataZoomRef.current?.getInstance();
            if (!chartInstance) return;
            const offsetTime = maxTime - minTime;
            const start = Math.max(0, Math.min(100, (selectedRange[0] - minTime) / offsetTime * 100));
            const end = Math.max(0, Math.min(100, (selectedRange[1] - minTime) / offsetTime * 100));
            syncingRef.current = true;
            chartInstance.dispatchAction({ type: 'dataZoom', start, end });
            requestAnimationFrame(() => {
                syncingRef.current = false;
            });
        }, [selectedRange?.[0], selectedRange?.[1], minTime, maxTime]);

        return (
            dataSource.length > 0
                ? <div style={{ position: 'relative', width: typeof width === 'number' ? `${width}px` : width ?? '100%' }}>
                    <div style={{ position: 'absolute', bottom: 0, left: 0, width: '100%' }}>
                        <MIChart
                            ref={chartRef}
                            options={chartOptions}
                            height={`${(dataZoomHeight ?? DATA_ZOOM_HEIGHT)}px`}
                        />
                    </div>
                    <MIChart
                        ref={dataZoomRef}
                        options={dataZoomOptions}
                        height={`${(dataZoomHeight ?? DATA_ZOOM_HEIGHT) + DATA_ZOOM_OFFSET}px`}
                    />
                </div>
                : <></>
        );
    },
);

export default MemoryDataZoom;

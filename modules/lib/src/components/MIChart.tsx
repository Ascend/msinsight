/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import React, { useEffect, useRef, useState, useImperativeHandle, forwardRef } from 'react';
import * as echarts from 'echarts';
import { isArray, merge } from 'lodash';
import { useTheme } from '@emotion/react';
import ResizeObserver from 'resize-observer-polyfill';

type EChartsOption = echarts.EChartsOption;
type ECharts = echarts.ECharts;

interface ChartProps {
    options: EChartsOption;
    loading?: boolean;
    width?: string;
    height?: string;
    onEvents?: {
        [key: string]: (params?: any) => void;
    };
}

export interface ChartsHandle {
    chartDom: HTMLDivElement | null;
    chartInstance: ECharts | null;
    getChartDom: () => HTMLDivElement | null;
    getInstance: () => ECharts | null;
}

const chartColors = [
    '#0062DC', '#279C6E', '#34B1B9', '#6037DB', '#FF5432', '#0077FF', '#D53F78', '#2F9CE0', '#EE891D', '#AA38CE', '#75A105',
    '#E44222', '#0158C5', '#248B62', '#2F9EA6', '#026AE5', '#E32A2A', '#BF376C', '#2A8CC9', '#5631C4', '#D57B18', '#9831B9', '#699006',
    '#CB2F0F', '#004EB0', '#1F7C58', '#2A8D93', '#005FCC', '#CA2425', '#AA3260', '#247CB3', '#4C2CAF', '#BE6D17', '#882CA4', '#5D8004',
    '#FE6E50', '#2679E1', '#48AA82', '#51BCC3', '#278AFF', '#FD4444', '#DB5B8C', '#4CAAE4', '#7754E0', '#F09A3D', '#B655D4', '#89AF2A',
    '#FF876F', '#4D91E5', '#67B999', '#71C8CE', '#4DA0FF', '#FD5857', '#E178A0', '#6DB9E8', '#9073E5', '#F3AC60', '#C374DC', '#9EBD51',
];

const defaultOptions: EChartsOption = {
    color: chartColors,
    textStyle: {
        fontFamily: '\'Inter\', -apple-system, BlinkMacSystemFont, \'Segoe UI\', Roboto, Oxygen, Ubuntu, Cantarell, \'Fira Sans\', \'Droid Sans\', sans-serif',
    },
    legend: {
        pageIconColor: '#0077FF',
    },
    tooltip: {
        borderWidth: 0,
        padding: 16,
    },
    toolbox: {
        show: false,
        iconStyle: { borderColor: '#8D98AA' },
        feature: {
            dataZoom: {
                yAxisIndex: 'none',
                emphasis: { iconStyle: { textPosition: 'top' } },
            },
            restore: {
                emphasis: { iconStyle: { textPosition: 'top' } },
            },
            dataView: {
                emphasis: { iconStyle: { textPosition: 'top' } },
            },
        },
        top: 20,
        right: 10,
    },
    xAxis: {
        axisLabel: {
            color: '#8D98AA',
        },
        nameTextStyle: {
            color: '#8D98AA',
        },
    },
    yAxis: {
        axisLabel: {
            color: '#8D98AA',
        },
        nameTextStyle: {
            color: '#8D98AA',
        },
        splitLine: {
            lineStyle: {
                type: 'dashed',
            },
        },
    },
    series: [],
};

const lightChartOptions: EChartsOption = merge({}, defaultOptions, {
    title: {
        textStyle: { color: '#4E5865' },
    },
    legend: {
        textStyle: {
            color: '#4E5865',
        },
        pageTextStyle: {
            color: '#4E5865',
        },
    },
    tooltip: {
        backgroundColor: '#EBEFF6',
        textStyle: {
            color: '#4E5865',
        },
    },
    toolbox: {
        feature: {
            dataView: {
                backgroundColor: '#EBEFF6',
                textareaColor: '#EBEFF6',
                textColor: '#4E5865',
                buttonColor: '#0077FF',
            },
        },
    },
    xAxis: {
        axisLine: {
            lineStyle: {
                color: '#8D98AA',
            },
        },
        splitLine: {
            lineStyle: {
                color: '#E2E4EF',
            },
        },
    },
    yAxis: {
        axisLine: {
            lineStyle: {
                color: '#8D98AA',
            },
        },
        splitLine: {
            lineStyle: {
                color: '#E2E4EF',
            },
        },
    },
});
const darkChartOptions: EChartsOption = merge({}, defaultOptions, {
    title: {
        textStyle: { color: '#ffffff' },
    },
    legend: {
        textStyle: {
            color: '#D2DCE9',
        },
        pageTextStyle: {
            color: '#D2DCE9',
        },
    },
    tooltip: {
        backgroundColor: '#2A2F37',
        textStyle: {
            color: '#D2DCE9',
        },
    },
    toolbox: {
        feature: {
            dataView: {
                backgroundColor: '#2A2F37',
                textareaColor: '#2A2F37',
                textColor: '#D2DCE9',
                buttonColor: '#0077FF',
            },
        },
    },
    xAxis: {
        axisLine: {
            lineStyle: {
                color: '#3E4551',
            },
        },
        splitLine: {
            lineStyle: {
                color: '#3E4551',
            },
        },
    },
    yAxis: {
        axisLine: {
            lineStyle: {
                color: '#3E4551',
            },
        },
        splitLine: {
            lineStyle: {
                color: '#3E4551',
            },
        },
    },
});

const getOptionsWithAxisConfig = (themeMode: string, options: EChartsOption): EChartsOption => {
    const themeOptions = themeMode === 'dark' ? darkChartOptions : lightChartOptions;
    let xAxis;
    let yAxis;
    if (isArray(options.xAxis)) {
        xAxis = options.xAxis.map((item) => merge({}, themeOptions.xAxis, item));
    }

    if (isArray(options.yAxis)) {
        yAxis = options.yAxis.map((item) => merge({}, themeOptions.yAxis, item));
    }

    const newOptions = { ...options };

    if (xAxis) {
        newOptions.xAxis = xAxis;
    }

    if (yAxis) {
        newOptions.yAxis = yAxis;
    }

    return newOptions;
};

export const MIChart = forwardRef<ChartsHandle, ChartProps>(
    ({ options, loading = false, width = '100%', height = '400px', onEvents = {} }, ref) => {
        const chartRef = useRef<HTMLDivElement>(null);
        const chartInstanceRef = useRef<ECharts | null>(null);
        const [chartInstance, setChartInstance] = useState<ECharts | null>(null);
        const theme = useTheme();

        useEffect(() => {
            const chartDom = chartRef.current;
            if (!chartDom) {
                return undefined;
            }
            const resizeChart = (): void => {
                // Hidden tabs have no layout; initialize when their container becomes visible.
                if (chartDom.clientWidth === 0 || chartDom.clientHeight === 0) {
                    return;
                }
                if (chartInstanceRef.current) {
                    chartInstanceRef.current.resize();
                } else {
                    chartInstanceRef.current = echarts.init(chartDom);
                    setChartInstance(chartInstanceRef.current);
                }
            };
            const resizeObserver = new ResizeObserver(resizeChart);
            resizeObserver.observe(chartDom);
            window.addEventListener('resize', resizeChart);
            resizeChart();
            return (): void => {
                resizeObserver.disconnect();
                window.removeEventListener('resize', resizeChart);
                chartInstanceRef.current?.dispose();
                chartInstanceRef.current = null;
            };
        }, []);

        useEffect(() => {
            if (chartInstance) {
                const themeOptions = theme.mode === 'dark' ? darkChartOptions : lightChartOptions;
                const newOptions = getOptionsWithAxisConfig(theme.mode, options);
                const mergedOptions = merge({}, themeOptions, newOptions);
                chartInstance.setOption(mergedOptions, true);
            }
        }, [chartInstance, options, theme.mode]);

        useEffect(() => {
            Object.keys(onEvents).forEach((eventName) => {
                chartInstance?.on(eventName, onEvents[eventName]);
            });
            return (): void => {
                Object.keys(onEvents).forEach((eventName) => {
                    chartInstance?.off(eventName, onEvents[eventName]);
                });
            };
        }, [chartInstance, onEvents]);

        useEffect(() => {
            if (loading) {
                chartInstance?.showLoading({
                    text: '',
                    color: theme.primaryColor,
                    textColor: theme.textColorPrimary,
                    maskColor: theme.maskColor,
                });
            } else {
                chartInstance?.hideLoading();
            }
        }, [chartInstance, loading, theme.primaryColor, theme.textColorPrimary, theme.maskColor]);

        useImperativeHandle(ref, (): ChartsHandle => ({
            chartDom: chartRef.current,
            chartInstance: chartInstanceRef.current,
            getChartDom: () => chartRef.current,
            getInstance: () => chartInstanceRef.current,
        }));

        return <div ref={chartRef} style={{ width, height }} />;
    },
);

MIChart.displayName = 'MIChart';

export default MIChart;

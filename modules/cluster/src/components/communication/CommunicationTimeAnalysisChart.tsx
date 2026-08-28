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
import * as echarts from 'echarts';

import type { Session } from '../../entity/session';
import React, { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react-lite';
import { getBaselineName, getCompareName, Loading } from '../Common';
import { colorPalette, hashToNumber } from '../../utils/colorUtil';
import { Dropdown } from '@insight/lib/components';
import { type MenuProps, Button, notification, Spin } from 'antd';
import connector from '../../connection';
import i18n from '@insight/lib/i18n';
import { themeInstance } from '@insight/lib/theme';
import { type Theme } from '@emotion/react';
import { disposeAdaptiveEchart, getAdaptiveEchart, getDefaultChartOptions, safeStr } from '@insight/lib/utils';
import { ChartZoomData, ClickOperatorItem, CompareData, FormatterParams } from '../../utils/interface';
import { queryTimelineUnitKernelDetail } from '../../utils/RequestUtils';
import { useEventBus } from '../../utils/eventBus';
import { shouldShowTailAlignTip } from './tailAlign';
import type { ECharts, InsideDataZoomComponentOption } from 'echarts';
import {
    buildCommunicationWebGLIndex,
    CommunicationOperatorSource,
    findCommunicationOperator,
    hitTestCommunicationOperator,
    selectVisibleCommunicationOperators,
    type CommunicationWebGLIndex,
    type CommunicationWebGLOperator,
} from './communicationTimeWebglData';
import {
    createCommunicationTimeWebGLRenderer,
    getCommunicationWebGLRankGeometry,
    type CommunicationTimeWebGLRenderer,
    type CommunicationWebGLLayout,
} from './CommunicationTimeWebGLRenderer';

// 定义点击慢操作排名时的回调参数接口
interface OnClickSlowRankOpCallbackParams {
    startValue: number;
    endValue: number;
    rankId: string | number;
    name: string;
}

type RectItemValues = [number, number, number, number, number, string];

const DEFAULT_CHART_HEIGHT = 460;
const DEFAULT_INNER_CHART_HEIGHT = 300;
const DEFAULT_CHART_ZOOM_HEIGHT = 400;
const MIN_CHART_ITEM_HEIGHT = 30;
const MAX_CHART_HEIGHT = 800;
const NS_TO_MS_FACTOR = 0.000001;
// Communication 缩略图初始化最大可见rank数量
const INITINAL_MAX_VISIBLE_RANK_NUMBER = 516;
// Communication 缩略图初始化最大可见算子数
const MAX_VISIBLE_OPERATOR_NUMBER = 10000;
// 支持Y轴定位的最小数量
const START_POSITION_AXIS_Y = 20;

// 計算位置
function initDataZoom(chartOption: any, totalNum: number, dataLength: number, communicationChartZoomData?: ChartZoomData): void {
    if (dataLength <= 0 || totalNum <= 0 || chartOption.dataZoom.length <= 1) {
        return;
    }
    // 计算 Communication 缩略图纵轴显示范围，限定最多显示516列，如果rank数量超过516则计算比例，计算 范围 = (516 ÷ rank数量) * 100
    // 显示区间为[100 - 范围, 100]
    if (dataLength > INITINAL_MAX_VISIBLE_RANK_NUMBER) {
        const yPercentage = Math.ceil(INITINAL_MAX_VISIBLE_RANK_NUMBER / dataLength * 100);
        chartOption.dataZoom[1].start = 100 - yPercentage;
        chartOption.dataZoom[1].end = 100;
    } else {
        chartOption.dataZoom[1].start = 0;
        chartOption.dataZoom[1].end = 100;
    }
    // 计算 Communication 缩略图横轴显示范围，限定最多显示10000条（10000是估值，实际显示范围会有所波动），计算 范围 = (10000 ÷ 数据总量) * 100
    // 显示区间为[0, 范围]
    if (totalNum > MAX_VISIBLE_OPERATOR_NUMBER) {
        const xPercentage = Math.ceil(MAX_VISIBLE_OPERATOR_NUMBER / totalNum * 100);
        chartOption.dataZoom[0].start = communicationChartZoomData?.start ?? 0;
        chartOption.dataZoom[0].end = communicationChartZoomData?.end ?? xPercentage;
    } else {
        chartOption.dataZoom[0].start = communicationChartZoomData?.start ?? 0;
        chartOption.dataZoom[0].end = communicationChartZoomData?.end ?? 100;
    }
}

export function initWebGLDataZoom(chartOption: any, communicationChartZoomData?: ChartZoomData): void {
    if (chartOption.dataZoom.length <= 1) {
        return;
    }
    chartOption.dataZoom[0].start = communicationChartZoomData?.start ?? 0;
    chartOption.dataZoom[0].end = communicationChartZoomData?.end ?? 100;
    chartOption.dataZoom[1].start = 0;
    chartOption.dataZoom[1].end = 100;
}
enum compareSource {
    COMPARISON = 0,
    BASELINE = 1,
}
const sourceIndex = 4;
function wrapData(dataSource: AnalysisChartData, isCompare: boolean, communicationChartZoomData?: ChartZoomData): any {
    const data: any = [];
    const yAxisData: string[] = [];
    const dataLength = Math.max(dataSource?.data?.length, 0);
    const theme = themeInstance.getThemeType();
    let totalNumber = 0;
    for (let i = dataLength - 1; i >= 0; --i) {
        totalNumber += dataSource.data[i].lists.compare.length;
        const rankId = dataSource.data[i].rankId;
        yAxisData.push(rankId);
        dataSource.data[i].lists?.compare.forEach((item, _) => {
            data.push(getRenderData({ item, rankId, theme, source: compareSource.COMPARISON }));
        });
        if (isCompare) {
            dataSource.data[i].lists?.baseline.forEach((item, _) => {
                data.push(getRenderData({ item, rankId, theme, source: compareSource.BASELINE }));
            });
        }
    }
    option.yAxis.data = yAxisData;
    option.xAxis.min = nsToMs(dataSource.minTime);
    option.xAxis.max = nsToMs(dataSource.maxTime);
    const dataHeight = calculateDataHeight(dataSource);
    option.grid.height = dataHeight;
    option.dataZoom[0].top = dataHeight - DEFAULT_INNER_CHART_HEIGHT + DEFAULT_CHART_ZOOM_HEIGHT;
    initDataZoom(option, totalNumber, dataLength, communicationChartZoomData);
    option.series = getSeries({ data, isCompare });
    option.tooltip = getTooltip({ isCompare });
    return option;
}

const getRenderData = ({ item, rankId, source, theme }: { item: OperatorTimeItem; rankId: string; source: compareSource; theme: Theme }): any => {
    const startTime = nsToMs(item.startTime);
    const duration = nsToMs(item.duration);
    const endTime = startTime + duration;
    return {
        name: `${rankId}-${item.operatorName}`,
        value: [rankId, startTime, endTime, duration, source, item.operatorName],
        itemStyle: {
            normal: {
                color: theme.colorPalette[colorPalette[hashToNumber(item.operatorName, colorPalette.length)]],
            },
        },
    };
};

const baseSeire = {
    type: 'custom',
    itemStyle: {
        opacity: 1,
    },
    encode: {
        x: [1, 2],
        y: 0,
    },
    data: [],
};
function getSeries({ isCompare, data }: { isCompare: boolean; data: any[] }): any[] {
    return [{ ...baseSeire, data, renderItem: getRenderItem(isCompare) }];
}

function getRenderItem(isCompare: boolean): any {
    return (params: any, api: any): any => {
        const categoryIndex = api.value(0);
        const start = api.coord([api.value(1), categoryIndex]);
        const end = api.coord([api.value(2), categoryIndex]);
        const height = api.size([0, 1])[1] * 0.6 * (isCompare ? 0.5 : 1);
        let y;
        if (isCompare) {
            const isComparison = api.value(4) === compareSource.COMPARISON;
            // 对比在上，基线在下
            y = isComparison ? start[1] - height : start[1] + (height / 3);
        } else {
            y = start[1] - (height / 2);
        }
        const rectShape = echarts.graphic.clipRectByRect(
            {
                x: start[0],
                y,
                width: end[0] - start[0],
                height,
            },
            {
                x: params.coordSys.x,
                y: params.coordSys.y,
                width: params.coordSys.width,
                height: params.coordSys.height,
            },
        );
        return (
            {
                type: 'rect',
                transition: ['shape'],
                shape: rectShape,
                name: 'op',
                style: api.style(),
                emphasis: {
                    style: {
                        stroke: '#ffd666',
                        lineWidth: 4,
                        shadowBlur: 12,
                        shadowColor: 'rgba(255, 214, 102, 0.9)',
                    },
                },
            }
        );
    };
}

function getTooltip({ isCompare }: { isCompare: boolean }): any {
    return {
        formatter: (params: FormatterParams): string => {
            let tooltipMarkup = `${params.marker} `;
            let getName = (val: string): string => val;
            if (isCompare) {
                const isBaseline = params.value[sourceIndex] === compareSource.BASELINE;
                getName = isBaseline ? getBaselineName : getCompareName;
            }
            tooltipMarkup += getTipLineStr('Rank ID', `${params.value[0]}`);
            tooltipMarkup += getTipLineStr(getName('Operator Name'), `${params.value[5]}`);
            tooltipMarkup += getTipLineStr(getName('Start Time'), `${numberToStr(params.value[1])}ms`);
            tooltipMarkup += getTipLineStr(getName('Elapse Time'), `${numberToStr(params.value[3])}ms`);
            return tooltipMarkup;
        },
    };
}
function numberToStr(value: number): string {
    return `${value.toFixed(6).replace(/\.?0+$/, '')}`;
}

function nsToMs(value: number): number {
    return value * NS_TO_MS_FACTOR;
}
function msToNs(value: number): number {
    return Math.round(value / NS_TO_MS_FACTOR);
}

function getTipLineStr(name: string, value: string): string {
    let html = `${i18n.t(`tableHead.${name}`, { ns: 'communication' })}: `;
    html += `<strong style="color: black">${safeStr((`${value}`))}</strong><br/>`;
    return html;
}

const option: any = {
    textStyle: getDefaultChartOptions().textStyle,
    dataZoom: [
        {
            type: 'slider',
            filterMode: 'weakFilter',
            showDataShadow: false,
            top: DEFAULT_CHART_ZOOM_HEIGHT,
            labelFormatter: '',
            start: 0,
            end: 100,
            xAxisIndex: 0,
            bottom: 10,
            height: 20,
            borderColor: '#d2dbee80',
        },
        {
            type: 'slider',
            filterMode: 'weakFilter',
            showDataShadow: false,
            labelFormatter: '',
            start: 0,
            end: 100,
            yAxisIndex: 0,
            right: 10,
            width: 20,
            borderColor: '#d2dbee80',
        },
        {
            type: 'inside',
            filterMode: 'weakFilter',
            zoomOnMouseWheel: 'ctrl',
            moveOnMouseWheel: 'shift',
        },
    ],
    grid: {
        left: 100,
        right: 120,
        height: DEFAULT_INNER_CHART_HEIGHT,
    },
    xAxis: {
        scale: true,
        name: 'Time(ms)',
        axisLabel: {
            formatter: function (val: number) {
                return numberToStr(Math.max(0, val));
            },
        },
    },
    yAxis: {
        data: [],
        name: 'Rank ID',
    },
    series: [],
};

// 定义操作时间项的接口
export interface OperatorTimeItem {
    operatorName: string;
    startTime: number;
    duration: number;
}

// 定义操作时间信息的接口
export interface OperatorTimeInfo {
    rankId: string;
    dbPath: string;
    lists: CompareData<OperatorTimeItem[]>;
}

// 定义分析图表数据的接口
export interface AnalysisChartData {
    minTime: number;
    maxTime: number;
    data: OperatorTimeInfo[];
}

// 定义操作详情的接口
interface OpDetail {
    name: string;
    rankId: number;
    dbPath: string;
    timestamp: number;
    duration: number;
}
let selectedOpDetail: OpDetail | null;

interface ChartInstance {
    chart: echarts.ECharts;
    resizeObserver: ResizeObserver;
    webgl?: CommunicationWebGLState;
    cleanup: () => void;
}

interface CommunicationWebGLState {
    renderer: CommunicationTimeWebGLRenderer;
    index: CommunicationWebGLIndex | null;
    dataSource: AnalysisChartData | null;
    session: Session | null;
    isCompare: boolean;
    layout: CommunicationWebGLLayout | null;
    tooltip: HTMLDivElement;
    renderFrameId: number | null;
    mouseMoveHandler: (event: any) => void;
    mouseOutHandler: () => void;
    contextMenuHandler: (event: any) => void;
    dataZoomHandler: () => void;
}

// 全局存储（按容器隔离，支持多图表）
const chartInstanceMap: WeakMap<HTMLElement, ChartInstance> = new WeakMap<HTMLElement, ChartInstance>();

const getWebGLChartOption = (
    dataSource: AnalysisChartData,
    communicationChartZoomData?: ChartZoomData,
): any => {
    const dataHeight = calculateDataHeight(dataSource);
    const dataZoom = option.dataZoom.map((item: any) => ({ ...item }));
    const chartOption = {
        animation: false,
        textStyle: option.textStyle,
        dataZoom,
        grid: { ...option.grid, height: dataHeight },
        xAxis: {
            ...option.xAxis,
            min: nsToMs(dataSource.minTime),
            max: nsToMs(dataSource.maxTime),
        },
        yAxis: {
            ...option.yAxis,
            data: (dataSource.data ?? []).slice().reverse().map(item => item.rankId),
        },
        tooltip: { show: false },
        series: [],
    };
    chartOption.dataZoom[0].top = dataHeight - DEFAULT_INNER_CHART_HEIGHT + DEFAULT_CHART_ZOOM_HEIGHT;
    initWebGLDataZoom(chartOption, communicationChartZoomData);
    return chartOption;
};

export const getEChartsTooltipStyle = (): {
    background: string;
    color: string;
    boxShadow: string;
    borderRadius: string;
    borderStyle: string;
    borderWidth: string;
    padding: string;
    fontFamily: string;
    fontSize: string;
    lineHeight: string;
} => {
    const fontFamily = getDefaultChartOptions().textStyle.fontFamily;
    return {
        background: '#fff',
        color: '#666',
        boxShadow: '1px 2px 10px rgba(0, 0, 0, .2)',
        borderRadius: '4px',
        borderStyle: 'solid',
        borderWidth: '1px',
        padding: '10px',
        fontFamily,
        fontSize: '14px',
        lineHeight: '21px',
    };
};

const createWebGLTooltip = (chartDom: HTMLElement): HTMLDivElement => {
    const tooltip = document.createElement('div');
    Object.assign(tooltip.style, {
        position: 'absolute',
        display: 'none',
        pointerEvents: 'none',
        zIndex: '3',
        maxWidth: '360px',
        ...getEChartsTooltipStyle(),
        whiteSpace: 'nowrap',
    });
    chartDom.appendChild(tooltip);
    return tooltip;
};

const getWebGLOperatorColor = (operator: CommunicationWebGLOperator): string => {
    const theme = themeInstance.getThemeType();
    return theme.colorPalette[colorPalette[operator.colorIndex]];
};

const getWebGLOperatorTooltip = (
    operator: CommunicationWebGLOperator,
    rankId: string,
    isCompare: boolean,
): string => {
    let getName = (value: string): string => value;
    if (isCompare) {
        getName = operator.source === CommunicationOperatorSource.BASELINE ? getBaselineName : getCompareName;
    }
    const operatorColor = safeStr(getWebGLOperatorColor(operator));
    let markup = '<span style="display:inline-block;margin-right:4px;border-radius:10px;' +
        `width:10px;height:10px;background-color:${operatorColor};"></span> `;
    markup += getTipLineStr('Rank ID', rankId);
    markup += getTipLineStr(getName('Operator Name'), operator.operatorName);
    markup += getTipLineStr(getName('Start Time'), `${numberToStr(operator.startTime)}ms`);
    markup += getTipLineStr(getName('Elapse Time'), `${numberToStr(operator.duration)}ms`);
    return markup;
};

const getWebGLLayout = (
    chartDom: HTMLElement,
    chart: echarts.ECharts,
    index: CommunicationWebGLIndex,
    isCompare: boolean,
): CommunicationWebGLLayout | null => {
    if (index.ranks.length === 0) {
        return null;
    }
    const gridModel = (chart as any).getModel()?.getComponent('grid', 0);
    const gridCoordinateSystem = gridModel?.coordinateSystem;
    const coordinateSystem = gridCoordinateSystem?.getCartesian?.(0, 0) ??
        gridCoordinateSystem?.getCartesians?.()?.[0];
    const gridRect = gridCoordinateSystem?.getRect?.() ?? coordinateSystem?.getArea?.();
    const xAxis = coordinateSystem?.getAxis?.('x');
    const yAxis = coordinateSystem?.getAxis?.('y');
    const xExtent = xAxis?.scale?.getExtent?.();
    const yExtent = yAxis?.scale?.getExtent?.();
    const yAxisExtent = yAxis?.getExtent?.();
    if (!gridRect || !xExtent || xExtent.length < 2 || !yExtent || !yAxisExtent) {
        return null;
    }
    const xMin = Math.min(xExtent[0], xExtent[1]);
    const xMax = Math.max(xExtent[0], xExtent[1]);
    const getRankY = (rankIndex: number): number => {
        const rankId = index.ranks[rankIndex]?.rankId;
        if (rankId === undefined) {
            return Number.NaN;
        }
        const point = coordinateSystem?.dataToPoint?.([xMin, rankId]) ??
            chart.convertToPixel({ gridIndex: 0 }, [xMin, rankId]);
        return Array.isArray(point) ? point[1] : Number.NaN;
    };
    const bandWidth = Math.abs(yAxis?.getBandWidth?.() ?? gridRect.height / Math.max(1, index.ranks.length));
    const rankGeometry = getCommunicationWebGLRankGeometry({
        rankCount: index.ranks.length,
        visibleExtent: yExtent,
        axisExtent: yAxisExtent,
        bandWidth,
        getRankY,
    });
    if (!rankGeometry) {
        return null;
    }
    return {
        canvasWidth: chartDom.clientWidth,
        canvasHeight: chartDom.clientHeight,
        gridLeft: gridRect.x,
        gridTop: gridRect.y,
        gridWidth: gridRect.width,
        gridHeight: gridRect.height,
        xMin,
        xMax,
        ...rankGeometry,
        isCompare,
    };
};

const getWebGLViewport = (
    layout: CommunicationWebGLLayout,
    index: CommunicationWebGLIndex,
): { xMin: number; xMax: number; yStartIndex: number; yEndIndex: number } => {
    const firstVisibleRank = layout.rankStep === 0
        ? 0
        : (layout.gridTop - layout.rankY0) / layout.rankStep;
    const lastVisibleRank = layout.rankStep === 0
        ? index.ranks.length - 1
        : (layout.gridTop + layout.gridHeight - layout.rankY0) / layout.rankStep;
    return {
        xMin: layout.xMin,
        xMax: layout.xMax,
        yStartIndex: Math.max(0, Math.min(firstVisibleRank, lastVisibleRank)),
        yEndIndex: Math.min(index.ranks.length - 1, Math.max(firstVisibleRank, lastVisibleRank)),
    };
};

const renderWebGLChart = (chartDom: HTMLElement): void => {
    const instance = chartInstanceMap.get(chartDom);
    const state = instance?.webgl;
    if (!instance || !state?.index) {
        return;
    }
    const layout = getWebGLLayout(chartDom, instance.chart, state.index, state.isCompare);
    if (!layout) {
        state.renderer.clear();
        return;
    }
    state.layout = layout;
    const visible = selectVisibleCommunicationOperators(state.index, getWebGLViewport(layout, state.index));
    const theme = themeInstance.getThemeType();
    state.renderer.setColors(colorPalette.map(color => theme.colorPalette[color]));
    state.renderer.setData(visible, layout);
};

const scheduleWebGLRender = (chartDom: HTMLElement): void => {
    const state = chartInstanceMap.get(chartDom)?.webgl;
    if (!state || state.renderFrameId !== null) {
        return;
    }
    state.renderFrameId = window.requestAnimationFrame(() => {
        state.renderFrameId = null;
        renderWebGLChart(chartDom);
    });
};

const hitTestWebGLChart = (
    state: CommunicationWebGLState,
    offsetX: number,
    offsetY: number,
): CommunicationWebGLOperator | null => {
    const { index, layout } = state;
    if (!index || !layout || offsetX < layout.gridLeft || offsetX > layout.gridLeft + layout.gridWidth ||
        offsetY < layout.gridTop || offsetY > layout.gridTop + layout.gridHeight) {
        return null;
    }
    const rankIndex = Math.round((offsetY - layout.rankY0) / layout.rankStep);
    if (!Number.isFinite(rankIndex) || !index.ranks[rankIndex]) {
        return null;
    }
    const rankCenter = layout.rankY0 + rankIndex * layout.rankStep;
    const operatorHeight = layout.rowHeight * 0.6 * (layout.isCompare ? 0.5 : 1);
    let source: CommunicationOperatorSource | undefined;
    if (layout.isCompare) {
        if (offsetY >= rankCenter - operatorHeight && offsetY <= rankCenter) {
            source = CommunicationOperatorSource.COMPARISON;
        } else if (offsetY >= rankCenter + operatorHeight / 3 && offsetY <= rankCenter + operatorHeight * 4 / 3) {
            source = CommunicationOperatorSource.BASELINE;
        } else {
            return null;
        }
    } else if (Math.abs(offsetY - rankCenter) > operatorHeight / 2) {
        return null;
    }
    const time = layout.xMin + (offsetX - layout.gridLeft) / layout.gridWidth * (layout.xMax - layout.xMin);
    return hitTestCommunicationOperator(index, rankIndex, time, source);
};

const hideWebGLTooltip = (state: CommunicationWebGLState): void => {
    state.tooltip.style.display = 'none';
    state.renderer.setHoveredOperator(null);
};

const showWebGLTooltip = (
    chartDom: HTMLElement,
    state: CommunicationWebGLState,
    operator: CommunicationWebGLOperator,
    offsetX: number,
    offsetY: number,
): void => {
    const rankId = state.index?.ranks[operator.rankIndex]?.rankId ?? '';
    state.tooltip.style.borderColor = getWebGLOperatorColor(operator);
    state.tooltip.innerHTML = getWebGLOperatorTooltip(operator, rankId, state.isCompare);
    state.tooltip.style.display = 'block';
    const left = Math.min(offsetX + 12, Math.max(0, chartDom.clientWidth - state.tooltip.offsetWidth - 8));
    const top = Math.min(offsetY + 12, Math.max(0, chartDom.clientHeight - state.tooltip.offsetHeight - 8));
    state.tooltip.style.left = `${left}px`;
    state.tooltip.style.top = `${top}px`;
    state.renderer.setHoveredOperator(operator.id);
};

const disposeWebGLState = (chart: echarts.ECharts, state: CommunicationWebGLState): void => {
    if (state.renderFrameId !== null) {
        window.cancelAnimationFrame(state.renderFrameId);
    }
    chart.off('datazoom', state.dataZoomHandler);
    chart.getZr().off('mousemove', state.mouseMoveHandler);
    chart.getZr().off('globalout', state.mouseOutHandler);
    chart.getZr().off('contextmenu', state.contextMenuHandler);
    state.tooltip.remove();
    state.renderer.dispose();
};

const switchToEChartsFallback = (chartDom: HTMLElement): void => {
    const instance = chartInstanceMap.get(chartDom);
    const state = instance?.webgl;
    if (!instance || !state) {
        return;
    }
    const { dataSource, session } = state;
    const zoomData = getZoomData(instance.chart);
    disposeWebGLState(instance.chart, state);
    instance.webgl = undefined;
    chartDom.dataset.renderer = 'echarts';
    if (dataSource && session) {
        instance.chart.setOption(wrapData(
            dataSource,
            session.isCompare,
            zoomData,
        ), { notMerge: true });
    }
};

const initWebGLState = (
    chartDom: HTMLElement,
    chart: echarts.ECharts,
    setDropDownVisible: (_: boolean) => void,
): CommunicationWebGLState | undefined => {
    const renderer = createCommunicationTimeWebGLRenderer(chartDom, () => switchToEChartsFallback(chartDom));
    if (!renderer) {
        chartDom.dataset.renderer = 'echarts';
        return undefined;
    }
    chartDom.dataset.renderer = 'webgl';
    const tooltip = createWebGLTooltip(chartDom);
    const state = {
        renderer,
        index: null,
        dataSource: null,
        session: null,
        isCompare: false,
        layout: null,
        tooltip,
        renderFrameId: null,
    } as CommunicationWebGLState;
    state.mouseMoveHandler = (event: any): void => {
        const operator = hitTestWebGLChart(state, event.offsetX, event.offsetY);
        if (!operator) {
            hideWebGLTooltip(state);
            return;
        }
        showWebGLTooltip(chartDom, state, operator, event.offsetX, event.offsetY);
    };
    state.mouseOutHandler = (): void => hideWebGLTooltip(state);
    state.contextMenuHandler = (event: any): void => {
        const operator = hitTestWebGLChart(state, event.offsetX, event.offsetY);
        if (!operator) {
            setDropDownVisible(false);
            return;
        }
        event.event?.preventDefault?.();
        hideWebGLTooltip(state);
        const rank = state.index?.ranks[operator.rankIndex];
        selectedOpDetail = {
            name: operator.operatorName,
            rankId: Number(rank?.rankId ?? 0),
            dbPath: rank?.dbPath ?? '',
            timestamp: msToNs(operator.startTime),
            duration: msToNs(operator.duration),
        };
        setDropDownVisible(true);
    };
    state.dataZoomHandler = (): void => scheduleWebGLRender(chartDom);
    chart.on('datazoom', state.dataZoomHandler);
    chart.getZr().on('mousemove', state.mouseMoveHandler);
    chart.getZr().on('globalout', state.mouseOutHandler);
    chart.getZr().on('contextmenu', state.contextMenuHandler);
    return state;
};

/**
 * 初始化图表
 * @param chartDom 图表实例
 * @param setDropDownVisible 设置下拉菜单可见性的函数
 * @return 初始化的ECharts实例或 null
 */
function initChartInstance(chartDom: HTMLElement, setDropDownVisible: (_: boolean) => void): echarts.ECharts | null {
    if (chartInstanceMap.has(chartDom)) {
        return chartInstanceMap.get(chartDom)?.chart ?? null;
    }
    const chart = getAdaptiveEchart(chartDom, option);
    if (!chart) {
        return null;
    }
    // 绑定尺寸监听
    let resizeObserverTimer: number | null = null;
    const resizeObserver = new ResizeObserver((): void => {
        if (resizeObserverTimer) {
            clearTimeout(resizeObserverTimer);
        }
        resizeObserverTimer = window.setTimeout((): void => {
            if (!chart.isDisposed() && chartDom.offsetHeight > 0 && chartDom.offsetWidth > 0) {
                chart.resize();
                scheduleWebGLRender(chartDom);
            }
            resizeObserverTimer = null;
        }, 100);
    });
    resizeObserver.observe(chartDom);

    // 绑定 contextmenu 事件
    // 通过闭包引用最新 dataSource 更新的 _currentRankMap
    const contextMenuHandler = (e: echarts.ECElementEvent): void => {
        setDropDownVisible(true);
        const [rankId, timestamp, , duration, , operatorName] = e.value as RectItemValues;
        // 从实例属性动态读取最新映射
        const rankMap = (chart as any)._currentRankMap || new Map();
        selectedOpDetail = {
            name: operatorName,
            rankId,
            dbPath: rankMap.get(rankId.toString()) || '',
            timestamp: msToNs(timestamp),
            duration: msToNs(duration),
        };
    };
    chart.on('contextmenu', { element: 'op' }, contextMenuHandler);

    // 注册清理函数
    const cleanup = (): void => {
        resizeObserver.disconnect();
        chart.off('contextmenu', contextMenuHandler);
        const state = chartInstanceMap.get(chartDom)?.webgl;
        if (state) {
            disposeWebGLState(chart, state);
        }
        delete chartDom.dataset.renderer;
        disposeAdaptiveEchart(chartDom);
        chartInstanceMap.delete(chartDom);
    };

    const instance: ChartInstance = { chart, resizeObserver, cleanup };
    chartInstanceMap.set(chartDom, instance);
    instance.webgl = initWebGLState(chartDom, chart, setDropDownVisible);
    return chart;
}

/**
 * 更新图表数据，轻量级，高频调用
 * @param chart 图表实例
 * @param dataSource 数据源
 * @param session 会话
 */
function updateChartData(
    chartDom: HTMLElement,
    dataSource: AnalysisChartData,
    session: Session,
    communicationChartZoomData?: ChartZoomData,
): boolean {
    const instance = chartInstanceMap.get(chartDom);
    if (!instance || instance.chart.isDisposed()) {
        return false;
    }
    const { chart } = instance;

    // 更新动态数据映射（供 contextmenu 事件使用）
    const rankDbPathMap = new Map<string, string>();
    dataSource?.data?.forEach(item => rankDbPathMap.set(item.rankId, item.dbPath));
    (chart as any)._currentRankMap = rankDbPathMap; // 通过实例属性存储当前映射，供事件处理器访问

    // 更新图表数据
    if (dataSource !== undefined) {
        if (instance.webgl) {
            instance.webgl.renderer.setSelectedOperator(null);
            instance.webgl.index = buildCommunicationWebGLIndex(dataSource, session.isCompare);
            instance.webgl.dataSource = dataSource;
            instance.webgl.session = session;
            instance.webgl.isCompare = session.isCompare;
            chart.setOption(getWebGLChartOption(
                dataSource,
                communicationChartZoomData,
            ), { notMerge: true });
            scheduleWebGLRender(chartDom);
        } else {
            chart.setOption(wrapData(dataSource, session.isCompare, communicationChartZoomData), { notMerge: true });
        }
    }
    return true;
}

/**
 * 清理资源，组件卸载时调用
 * @param chartDom 图表实例
 */
function disposeChartInstance(chartDom: HTMLElement): void {
    const instance = chartInstanceMap.get(chartDom);
    if (instance) {
        instance.cleanup();
    }
}

/**
 * 获取图表高度
 * @param dataSource - 分析图表数据
 * @returns 图表高度
 */
function calculateDataHeight(dataSource: AnalysisChartData): number {
    let calculateHeight: number;
    if (dataSource?.data?.length !== undefined) {
        calculateHeight = Math.max(dataSource.data.length * MIN_CHART_ITEM_HEIGHT, DEFAULT_INNER_CHART_HEIGHT);
    } else {
        calculateHeight = DEFAULT_INNER_CHART_HEIGHT;
    }
    return Math.min(MAX_CHART_HEIGHT, calculateHeight);
}
function getChartHeight(dataSource: AnalysisChartData): number {
    return calculateDataHeight(dataSource) + DEFAULT_CHART_HEIGHT - DEFAULT_INNER_CHART_HEIGHT;
}

/**
 * 重定向到时间线
 * @param setDropDownVisible - 设置下拉菜单可见性的函数
 * @returns 无返回值
 */
async function redirectToTimeline(setDropDownVisible: (_: boolean) => void): Promise<void> {
    if (selectedOpDetail === null) {
        return;
    }
    const { name, rankId, dbPath, duration } = selectedOpDetail;
    const params = {
        name,
        rankId: rankId.toString(),
        dbPath,
    };
    try {
        const res = await queryTimelineUnitKernelDetail(params);
        setDropDownVisible(false);
        const resObj = res ?? {};
        connector.send({
            event: 'switchModule',
            body: {
                switchTo: 'timeline',
                toModuleEvent: 'locateUnit',
                params: {
                    ...resObj,
                    ...params,
                    processId: resObj.pid,
                    startTime: resObj.startTime,
                    rankId: resObj.rankId,
                    duration,
                    showSelectedData: true,
                },
            },
        });
    } catch (e) {
        setDropDownVisible(false);
    }
}

/**
 * 更新时间线加载状态
 * @param isLoading - 是否正在加载
 */
const findInTimelineLoad = (isLoading: boolean): void => {
    const element = document?.getElementById('findInTimeline');
    if (!element) {
        return;
    }

    if (isLoading) {
        element.classList.add('find-in-time-line-load');
    } else {
        element.classList.remove('find-in-time-line-load');
    }
};

/**
 * 获取图表缩放数据
 * @param chartInstance - 图表实例
 * @returns 缩放数据
 */
const getZoomData = (chartInstance: ECharts | null): ChartZoomData => {
    const currentOption = chartInstance?.getOption();
    const { start = 0, end = 100 } = (currentOption?.dataZoom as InsideDataZoomComponentOption[])?.[0] || [];
    return {
        start,
        end,
    };
};

export const consumeCommunicationChartZoomData = (
    session: Pick<Session, 'communicationChartZoomData'>,
    update: (zoomData?: ChartZoomData) => void,
): void => {
    update(session.communicationChartZoomData);
    session.communicationChartZoomData = undefined;
};

/**
 * 生成菜单项
 * @param session - 会话对象
 * @param setDropDownVisible - 设置下拉菜单可见性的函数
 * @param chartInstance - 图表实例
 * @returns 菜单项数组
 */
const useMenuItems = (session: Session, setDropDownVisible: (_: boolean) => void, chartInstance: ECharts | null): MenuProps['items'] => {
    const { t } = useTranslation('communication');
    const findInTimeline = {
        label: t('Find in Timeline'),
        key: 'findInTimeline',
        id: 'findInTimeline',
        disabled: false,
        onClick: () => {
            findInTimelineLoad(true);
            setTimeout(() => {
                redirectToTimeline(setDropDownVisible);
            });
        },
    };
    const alignOperator = {
        label: t('Align according to selected operator'),
        key: 'alignAccordingToSelectedOperator',
        disabled: false,
        onClick: (): void => {
            setDropDownVisible(false);
            if (selectedOpDetail === null) {
                return;
            }
            session.communicationChartZoomData = getZoomData(chartInstance);
            session.targetOperator = selectedOpDetail as ClickOperatorItem;
        },
    };
    const restoredefault = {
        label: t('Restore default state'),
        key: 'restoreDefaultState',
        disabled: false,
        onClick: (): void => {
            setDropDownVisible(false);
            session.communicationChartZoomData = getZoomData(chartInstance);
            session.targetOperator = undefined;
        },
    };
    if (session.unitcount === 0) {
        findInTimeline.disabled = true;
    }
    if (session.targetOperator === undefined) {
        restoredefault.disabled = true;
    }
    return [
        findInTimeline,
        alignOperator,
        restoredefault,
    ];
};

/**
 * Y轴的接口定义。
 *
 * @interface YAxis
 * @property {string[]} data - Y轴的数据，表示为字符串数组。
 * @property {string} [name] - Y轴的名称，可选属性。
 * @property {boolean} [show] - 是否显示Y轴，可选属性。
 */
export interface YAxis {
    data: string[];
    name?: string;
    show?: boolean;
}

export const getRankDataZoomRange = (
    rankIds: string[],
    rankId: string | number,
): { start: number; end: number } | { startValue: string; endValue: string } => {
    if (rankIds.length < START_POSITION_AXIS_Y) {
        return { start: 0, end: 100 };
    }
    const rankIndex = rankIds.findIndex(item => item === `${rankId}`);
    if (rankIndex < 0) {
        return { start: 0, end: 100 };
    }
    const startIndex = Math.max(0, rankIndex - 10);
    const endIndex = Math.min(rankIds.length - 1, rankIndex + 10);
    return {
        startValue: rankIds[startIndex],
        endValue: rankIds[endIndex],
    };
};

/**
 * CommunicationTimeAnalysisChart组件，用于展示通信时间分析图表。
 * @param dataSource - 分析图表的数据源。
 * @param session - 当前会话对象。
 * @param loading - 是否正在加载数据。
 * @returns 返回一个React组件，用于展示通信时间分析图表。
 */
const CommunicationTimeAnalysisChart = observer(({ dataSource, session, loading }: { dataSource: AnalysisChartData; session: Session; loading: boolean }) => {
    const durationFileCompleted = session.durationFileCompleted;
    // 设置图表高度的state
    const [chartHeight, setChartHeight] = useState(DEFAULT_CHART_HEIGHT);
    // 控制下拉菜单可见性的state
    const [dropDownVisible, setDropDownVisible] = useState(false);
    // 图表容器的引用
    const chartRef = useRef<HTMLDivElement>(null);
    // 图表实例的引用
    const chartInst = useRef<echarts.ECharts | null>(null);
    // 获取菜单项
    const menuItems = useMenuItems(session, setDropDownVisible, chartInst.current);

    /**
     * 同步滚动事件处理函数，用于处理鼠标滚轮缩放时的页面滚动问题。
     * @param e - 鼠标滚轮事件对象。
     * @returns 无返回值。
     */
    const syncScroll = (e: WheelEvent): void => {
        // 如果目标元素不是CANVAS，则直接返回
        if ((e.target as HTMLElement).tagName !== 'CANVAS') {
            return;
        }

        // 如果没有按下Ctrl或Shift键，则滚动页面
        if (!e.ctrlKey && !e.shiftKey) {
            // 页面内容滚动容器
            const scrollContainer = document.querySelector('.mi-page-content');
            scrollContainer?.scrollBy(0, e.deltaY);
        }
    };

    const updateData = React.useCallback((dataSource: AnalysisChartData): void => {
        const chartDom = chartRef.current;
        if (chartDom && chartInst.current && dataSource) {
            runInAction(() => {
                consumeCommunicationChartZoomData(session, (zoomData): void => {
                    updateChartData(chartDom, dataSource, session, zoomData);
                });
            });
            setTimeout(() => {
                // 设置图表高度
                setChartHeight(getChartHeight(dataSource));
            });
        }
    }, [session]);

    /**
     * 使用useEffect更新图表数据
     * @param
     * @returns void
     */
    useEffect(() => {
        updateData(dataSource);
    }, [updateData, dataSource]);

    /**
     * 使用useEffect初始化图表
     * @param
     * @returns void
     */
    useEffect(() => {
        if (!durationFileCompleted) {
            return;
        }
        const dom = chartRef.current;
        if (!dom) {
            return;
        }
        const firstInitChart = (): void => {
            chartInst.current = initChartInstance(dom, setDropDownVisible);
            // 添加滚轮事件监听
            chartRef.current?.addEventListener('wheel', syncScroll, true);
            updateData(dataSource);
        };

        // 创建初始化 EChart 监听器，等待 DOM 元素准备就绪
        let resizeObserverTimer: number | null = null;
        const resizeObserver = new ResizeObserver((): void => {
            if (resizeObserverTimer) {
                clearTimeout(resizeObserverTimer);
            }
            resizeObserverTimer = window.setTimeout((): void => {
                // 仅当尺寸有效时“尝试”初始化
                if (dom.clientHeight > 0 && dom.clientWidth > 0) {
                    // 尺寸有效后停止监听
                    resizeObserver.disconnect();
                    firstInitChart();
                }
                resizeObserverTimer = null;
            }, 20); // 设置 delay 防止 Error: ResizeObserver loop completed with undelivered notifications.
        });
        resizeObserver.observe(dom);

        // 首次尝试
        if (dom.clientHeight > 0 && dom.clientWidth > 0) {
            // 尺寸有效后停止监听
            resizeObserver.disconnect();
            firstInitChart();
        }

        // 清理函数，移除滚轮事件监听
        return (): void => {
            resizeObserver.disconnect();
            disposeChartInstance(dom); // 清理全局实例记录
            chartRef.current?.removeEventListener('wheel', syncScroll, true);
        };
    }, [durationFileCompleted, updateData]);

    /**
     * 监听并处理慢算子点击事件的函数。
     * @param res - 包含慢算子事件参数的对象，包括起始值、结束值、算子名称和算子ID。
     * @returns 无返回值。
     */
    useEventBus('onClickSlowRankOp', (res): void => {
        // 解构慢算子事件参数
        const { startValue, endValue, name, rankId } = res as OnClickSlowRankOpCallbackParams;
        const dataSourceAxisY = (chartInst.current?.getOption()?.yAxis ?? []) as YAxis[];
        const rankZoomRange = getRankDataZoomRange(dataSourceAxisY?.[0]?.data ?? [], rankId);
        // 根据接收到的起始值和结束值调整图表横轴范围
        chartInst.current?.dispatchAction({
            type: 'dataZoom',
            dataZoomIndex: 0,
            startValue,
            endValue,
        });
        // 执行纵轴数据缩放
        chartInst.current?.dispatchAction({
            type: 'dataZoom',
            dataZoomIndex: 1, // 指定纵轴
            ...rankZoomRange,
        });

        const webglState = chartRef.current ? chartInstanceMap.get(chartRef.current)?.webgl : undefined;
        const webglOperator = webglState?.index
            ? findCommunicationOperator(webglState.index, rankId, name, startValue)
            : null;
        if (webglState) {
            scheduleWebGLRender(chartRef.current as HTMLElement);
            webglState.renderer.setSelectedOperator(webglOperator?.id ?? null);
        } else {
            // 先取消所有高亮
            chartInst.current?.dispatchAction({
                type: 'downplay',
                seriesIndex: 0,
            });

            // 再高亮具体算子
            chartInst.current?.dispatchAction({
                type: 'highlight',
                seriesIndex: 0,
                name: `${rankId}-${name}`,
            });
        }
        // 延迟取消高亮
        setTimeout(() => {
            if (webglState) {
                webglState.renderer.setSelectedOperator(null);
            } else {
                chartInst.current?.dispatchAction({
                    type: 'downplay',
                    seriesIndex: 0,
                    name: `${rankId}-${name}`,
                });
            }
        }, 5000);

        // ★ 提示用户可以右键按尾部对齐（仅 allReduce/allToAll/allGather）
        if (shouldShowTailAlignTip(name)) {
            const key = `align-tip-${rankId}-${name}-${startValue}-${endValue}`;
            notification.open({
                key,
                className: 'tail-align-notification',
                message: i18n.t('alignment.tipTitle', { ns: 'communication' }),
                description: i18n.t('alignment.tipDescription', { ns: 'communication' }),
                btn: (
                    <Button size="small" onClick={() => notification.close(key)}>
                        {i18n.t('alignment.dismiss', { ns: 'communication' })}
                    </Button>
                ),
                duration: 5,
                placement: 'top',
            });
        }

        // 滚动到视图中心
        chartRef.current?.scrollIntoView({
            block: 'center',
            behavior: 'smooth',
        });
    });

    return durationFileCompleted
        ? <Dropdown
            menu={{
                items: menuItems,
                onBlur: (e: React.FocusEvent<HTMLUListElement, Element>): void => {
                    const hasItem = menuItems?.findIndex(item =>
                        (e.relatedTarget as HTMLElement)?.dataset?.menuId?.includes(item?.key as string)) !== -1;
                    if (!hasItem) {
                        setDropDownVisible(false);
                    }
                },
            }}
            trigger={['contextMenu']}
            open={dropDownVisible}
            autoFocus
        >
            <Spin spinning={loading} delay={400}>
                <div ref={chartRef} id={'hccl'} style={{ width: 'calc(100vw - 80px)', height: chartHeight }}></div>
            </Spin>
        </Dropdown>
        : <div style={{ height: '400px' }}><Loading style={{ margin: '200px auto 0' }}/></div>;
});

export default CommunicationTimeAnalysisChart;

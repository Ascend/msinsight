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
import * as d3 from 'd3';
import { observer } from 'mobx-react';
import React, { useRef } from 'react';
import type { ChartProps, Scale, ScaleType } from '../../entity/chart';
import { Canvas, CanvasContainer } from './common';
import { useBatchedRender, useData, useHoverPosX, useRangeAndDomain } from './hooks';
import { TooltipComponent, type TooltipProps } from './TooltipComp';
import { useTheme } from '@emotion/react';
import type { Session } from '../../entity/session';

type ScaleTypeDefinition = Record<ScaleType, d3.ScaleContinuousNumeric<number, number>>;

const getScale: ScaleTypeDefinition = {
    linear: d3.scaleLinear(),
    symLog: d3.scaleSymlog(),
};

/**
 * This function can iterate data and return sum of data's height.
 * @param data should dispose dataset
 * @param limited the limited iterate times, generally used for ignoring out of palette's boundary data.
 * @param callback execute process when iterate dataset
 * @returns [number] totalHeight
 */
const getTotalHeight = (data: Data, limited: number, callback?: (accHeight: number, curHeight: number, index: number, all: number[]) => void): number => {
    const { values: heights } = data;
    return heights.slice(0, limited + 1).reduce((accumulativeHeight, curHeight, index, all) => {
        const tempHeight = accumulativeHeight + curHeight;
        callback?.(tempHeight, curHeight, index, all);
        return tempHeight;
    }, 0);
};

const drawAuxiliaryLine = (context: CanvasRenderingContext2D,
    yScale: Scale,
    auxiliaryValue: number, width: number): void => {
    context.beginPath();
    context.setLineDash([10, 10]);
    context.moveTo(0, yScale(auxiliaryValue));
    context.lineTo(width, yScale(auxiliaryValue));
    context.strokeStyle = 'white';
    context.globalAlpha = 0.2;
    context.stroke();
};

interface DrawAreaArgs {
    ctx: CanvasRenderingContext2D;
    dataList: Data[];
    minHeight: number;
    radius: number;
    xScale: Scale;
    yScale: Scale;
    domainStart: number;
    barWidthStamp: number;
    barWidthPix?: number;
    palette: StackedBarChartProps['palette'];
};

const getThisDrawWidth = (index: number, data: Data[], xScale: (x: number) => number, domainStart: number): number => {
    // find bar width pixel
    if (index > 0 && index < data.length - 1) {
        // find min distance
        return xScale(Math.min(data[index + 1].timestamp - data[index].timestamp, data[index].timestamp - (data[index - 1].timestamp / 2)) + domainStart);
    }
    if (data.length === 1) {
        return 24;
    }
    if (index === 0) {
        return xScale((data[index + 1].timestamp - (data[index].timestamp / 2)) + domainStart);
    }
    return xScale((data[index].timestamp - (data[index - 1].timestamp / 2)) + domainStart);
};

const drawArea = ({
    ctx, dataList, minHeight, radius, xScale, palette, yScale, domainStart, barWidthPix, barWidthStamp,
}: DrawAreaArgs): void => {
    if (palette === undefined) {
        return;
    }
    dataList.forEach((data, index) => {
        if (data.timestamp + (barWidthStamp / 2) < domainStart) {
            return;
        }
        let thisDrawWidth: number = barWidthPix as number;
        if (barWidthPix === undefined) {
            thisDrawWidth = getThisDrawWidth(index, dataList, xScale, domainStart);
        }
        const drawRect = (accumulativeHeight: number, curHeight: number, idx: number, allHeights: number[]): void => {
            ctx.fillStyle = palette[idx];
            // setting chart transparency 1 -> totally nontransparent
            ctx.globalAlpha = 1;
            const thisDrawHeight = yScale(minHeight) - yScale(curHeight);
            const startDrawWidth = xScale(data.timestamp) - (thisDrawWidth / 2);
            const startDrawHeight = thisDrawHeight > 1 ? yScale(accumulativeHeight) : ctx.canvas.height - 1;
            // when draw height < 1px, draw 1px.
            ctx.fillRect(
                startDrawWidth,
                startDrawHeight,
                thisDrawWidth,
                thisDrawHeight > 1 ? thisDrawHeight : 1,
            );
        };
        getTotalHeight(data, palette.length - 1, drawRect);
    });
};
interface DrawArgs {
    ctx: CanvasRenderingContext2D | null;
    data: Data[];
    palette: StackedBarChartProps['palette'];
    height: number;
    yScaleType: ScaleType;
    rangeAndDomain: Array<[ number, number ]>;
    radius: number;
    valueRange?: [number, number];
    auxiliaryValue?: number;
    barWidth?: number | string;
    barWidthStamp: number;
};
const draw = ({
    ctx, data, height, yScaleType, rangeAndDomain,
    radius, valueRange, auxiliaryValue, barWidth, palette, barWidthStamp,
}: DrawArgs): void => {
    if (palette === undefined) {
        return;
    }
    if (!ctx) { return; }
    const xScale = d3.scaleLinear().range(rangeAndDomain[0]).domain(rangeAndDomain[1]).clamp(false) as Scale;
    let barWidthPix: number | undefined;
    const domainStart = rangeAndDomain[1][0];
    if (barWidth !== undefined) {
        if (typeof barWidth === 'number') {
            barWidthPix = xScale(barWidth + domainStart);
        } else {
            barWidthPix = Number(barWidth.replace('px', ''));
        }
    }
    let maxHeight = 0;
    let minHeight = 0;
    const findHeights = (dataList: Data[]): void => {
        dataList.forEach((data) => {
            const curHeight = getTotalHeight(data, palette.length - 1);
            maxHeight = Math.max(maxHeight, curHeight);
            minHeight = Math.min(minHeight, curHeight);
        });
    };

    if (valueRange) {
        [minHeight, maxHeight] = valueRange;
    } else {
        findHeights(data);
        maxHeight *= 2;
    }
    const yScale = getScale[yScaleType].range([height, 0]).domain([minHeight, maxHeight]) as Scale;
    if (auxiliaryValue !== undefined && auxiliaryValue !== 0) { drawAuxiliaryLine(ctx, yScale, auxiliaryValue, rangeAndDomain[0][1]); }
    // draw line and area
    drawArea({ ctx, dataList: data, minHeight, radius, xScale, yScale, domainStart, barWidthStamp, barWidthPix, palette });
};

type ToolTipData = [ Data, number ];
const findDataByX = (mousePosX: number | undefined, data: Data[], rangeAndDomain: Array<[number, number]>): ToolTipData | undefined => {
    if (rangeAndDomain.length === 0 || data.length === 0 || mousePosX === undefined) { return undefined; }
    const reverseXScale = d3.scaleLinear().range([rangeAndDomain[1][0], rangeAndDomain[1][1]])
        .domain([rangeAndDomain[0][0], rangeAndDomain[0][1]]).clamp(false) as Scale;
    const xScale = d3.scaleLinear().domain([rangeAndDomain[1][0], rangeAndDomain[1][1]])
        .range([rangeAndDomain[0][0], rangeAndDomain[0][1]]).clamp(false) as Scale;
    const mouseTimestamp = reverseXScale(mousePosX);
    let minDistance = Number.MAX_VALUE;
    let selectedData = null;
    for (let i = 0; i < data.length; i++) {
        const distanceNow = Math.abs(data[i].timestamp - mouseTimestamp);
        if (distanceNow < minDistance) {
            minDistance = distanceNow;
            selectedData = data[i];
        }
    }
    if (!selectedData) { return undefined; }
    return [selectedData, xScale(selectedData.timestamp)];
};

const isTooltipXInDomain = (data: ToolTipData, session: Session): boolean => {
    const domainRange = session.domainRange;
    return data[0].timestamp > domainRange.domainStart && data[0].timestamp < domainRange.domainEnd;
};

const getBarWidthStamp = (barWidth: number | string, rangeAndDomain: Array<[number, number]>): number => {
    const xReverseScale = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false) as Scale;
    const domainStart = rangeAndDomain[1][0];
    if (typeof barWidth === 'number') {
        return barWidth;
    }
    return xReverseScale(Number(barWidth.replace('px', ''))) - domainStart;
};

const isHoverPosOnBar = (data: ToolTipData, barWidthStamp: number, rangeAndDomain: Array<[number, number]>, mousePosX: number): boolean => {
    const drawStartStamp = data[0].timestamp - (barWidthStamp / 2);
    const drawEndStamp = data[0].timestamp + (barWidthStamp / 2);
    const reverseXScale = d3.scaleLinear().range([rangeAndDomain[1][0], rangeAndDomain[1][1]])
        .domain([rangeAndDomain[0][0], rangeAndDomain[0][1]]).clamp(false) as Scale;
    const mouseTimestamp = reverseXScale(mousePosX);
    return mouseTimestamp >= drawStartStamp && mouseTimestamp <= drawEndStamp;
};

interface Data {
    timestamp: number;
    values: number[];
};
type StackedBarChartProps = ChartProps<'stackedBar'>;
export const StackedBarChart = observer(({
    margin, session, mapFunc, palette, unit,
    valueRange, barWidth, radius, yScaleType, auxiliaryValue,
    renderTooltip, width, height, metadata,
}: StackedBarChartProps) => {
    const canvasContainer = useRef<HTMLDivElement>(null);
    const canvas = useRef<HTMLCanvasElement>(null);
    const rangeAndDomain = useRangeAndDomain(session, width, margin);
    const barWidthStamp = getBarWidthStamp(barWidth, rangeAndDomain);
    const data = useData({
        session,
        mapFunc,
        unit,
        metadata,
        width,
        processor: (data) => data.filter(item => {
            return item.timestamp + (barWidthStamp / 2) >= rangeAndDomain[1][0] && item.timestamp - (barWidthStamp / 2) <= rangeAndDomain[1][1];
        }),
    });
    const mousePosX = useHoverPosX(canvasContainer);
    const theme = useTheme();
    const defaultPalette = ['#4183a2', '#549251', '#b09239', '#bb5f43', theme.colorPalette.otherColor];
    const hoveredData = React.useMemo(() => findDataByX(mousePosX, data, rangeAndDomain), [mousePosX, data, rangeAndDomain]);
    useBatchedRender(() => {
        const isCanvasInvalid = canvasContainer.current === null || canvas.current === null || rangeAndDomain.length === 0 ||
            canvas.current.width === 0 || canvas.current.height === 0;
        if (isCanvasInvalid) { return; }
        const ctx = canvas.current.getContext('2d');
        ctx?.clearRect(0, 0, width, height);
        const drawPalette = palette ?? defaultPalette;
        draw({ ctx, data, height, yScaleType, rangeAndDomain, radius, valueRange, auxiliaryValue, barWidth, palette: drawPalette, barWidthStamp });
    }, [data, rangeAndDomain, valueRange]);

    const tooltipProp: TooltipProps<ToolTipData, Data[]> = {
        data: (hoveredData !== undefined && !isTooltipXInDomain(hoveredData, session) &&
         !isHoverPosOnBar(hoveredData, barWidthStamp, rangeAndDomain, mousePosX ?? 0))
            ? undefined
            : hoveredData,
        x: (data) => isTooltipXInDomain(data, session) ? data[1] : mousePosX ?? 0,
        mouseX: mousePosX ?? null,
        session,
        dataset: data,
        calcHeight: () => height / 2,
        dom: canvasContainer,
        renderContent: (data) => renderTooltip?.(data[0], metadata),
    };

    return <CanvasContainer ref={canvasContainer} className={'canvasContainer'} width={width} height={height}>
        {height === null ? null : <TooltipComponent {...tooltipProp} />}
        <Canvas className={'drawCanvas'} ref={canvas} width={width} height={height}/>
    </CanvasContainer>;
});

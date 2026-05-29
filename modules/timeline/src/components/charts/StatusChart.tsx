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
import type { Theme } from '@emotion/react';
import { useTheme } from '@emotion/react';
import * as d3 from 'd3';
import { Skeleton } from 'antd';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import React, { useEffect, useMemo, useRef } from 'react';
import type { ChartProps, Scale, StatusData } from '../../entity/chart';
import { Canvas, SkeletonWrapper, CanvasContainer, zipStatusData } from './common';
import { useBatchedRender, useClick, useData, useHoverPosX, useRangeAndDomain } from './hooks';
import { TooltipComponent, type TooltipProps } from './TooltipComp';
import type { ThreadMetaData } from '../../entity/data';
import type { InsightUnit } from '../../entity/insight';

type StatusChartProps = ChartProps<'status'>;
interface DrawParams {
    ctx: CanvasRenderingContext2D | null;
    data: StatusData[];
    xScale: Scale;
    yScale: Scale;
    theme: Theme;
    startY: number;
}

const getMaxText = (text: string, maxWidth: number, ctx: CanvasRenderingContext2D): [string, number] => {
    if (ctx.measureText(text).width <= maxWidth) { return [text, ctx.measureText(text).width]; }
    let left = 0;
    let right = text.length;
    let mid = 0;
    while (left < right) {
        mid = Math.floor((left + right) / 2);
        if (ctx.measureText(`${text.slice(0, mid)}...`).width > maxWidth) {
            right = mid;
        } else {
            if (left === mid) { break; }
            left = mid;
        }
    }
    return [`${text.slice(0, mid)}...`, ctx.measureText(`${text.slice(0, mid)}...`).width];
};

const draw = ({ ctx, data, xScale, yScale, theme, startY }: DrawParams): void => {
    if (!ctx) { return; }
    ctx.textAlign = 'start';
    ctx.textBaseline = 'top';
    const minTextWidth = ctx.measureText('...').width + 8;
    data.forEach(data => {
        ctx.fillStyle = theme.summaryChartBgColor;
        let width = xScale(data.startTime + data.duration) - xScale(data.startTime);
        width = Math.max(1, Math.floor(width));
        const height = yScale(1) - yScale(0);
        ctx.fillRect(Math.floor(xScale(data.startTime)), startY, width, height - 2);
        if (width < minTextWidth) { return; }
        ctx.fillStyle = theme.fontColor;
        const xOffset = (textWidth: number): number => {
            return xScale(data.startTime) + ((xScale(data.startTime + data.duration) - xScale(data.startTime) - textWidth) / 2);
        };
        if (data.subName !== undefined) {
            const [subNameText, subNameWidth] = getMaxText(data.subName, width - 8, ctx);
            ctx.font = '12px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
            ctx.fillText(subNameText, xOffset(subNameWidth), height / 2);
            ctx.font = 'bold 12px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
            const [nameText, nameWidth] = getMaxText(data.name, width - 8, ctx);
            ctx.fillText(nameText, xOffset(nameWidth), (height - 12) / 4);
        } else {
            ctx.font = '12px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
            const [nameText, nameWidth] = getMaxText(data.name, width - 8, ctx);
            ctx.fillText(nameText, xOffset(nameWidth), (height - 12) / 2);
        }
    });
};

const findDataByX = (mousePosX: number | undefined, data: StatusData[],
    rangeAndDomain: Array<[number, number]>): StatusData | undefined => {
    if (mousePosX === undefined || data.length === 0 || rangeAndDomain.length === 0) {
        return undefined;
    }
    // mouse point is 1px in width, which in fact does not represent a timestamp, but a time range
    const pxToTime = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false);
    const mouseTimeStart = pxToTime(mousePosX);
    const mouseTimeEnd = pxToTime(mousePosX + 1);
    if (data[0].startTime > mouseTimeEnd || (data[data.length - 1].startTime + data[data.length - 1].duration < mouseTimeStart)) {
        return undefined;
    }
    let lo = 0;
    let hi = data.length;
    while (lo < hi) {
        const mid = Math.floor((lo + hi) / 2);
        const elem = data[mid];
        if (mouseTimeEnd < elem.startTime) {
            hi = mid;
        } else if (mouseTimeStart > elem.startTime + elem.duration) {
            lo = mid + 1;
        } else {
            return elem;
        }
    }
    return undefined;
};

const shouldHidePreview = (unit: InsightUnit): boolean => {
    if ((unit.metadata as ThreadMetaData).cardId?.endsWith('.db')) {
        // Host侧的CANN层泳道(Label类型)未展开时，不隐藏预览图，否则感知不到CANN层下有无数据
        const labelUnit = unit.children?.find(childUnit => childUnit.name === 'Label');
        return unit.isExpanded && Boolean(labelUnit?.isExpanded);
    }
    return unit.isExpanded;
};

export const StatusChart = observer(({
    session, margin, mapFunc, unit, metadata, renderTooltip, height, onHover, onClick, decorator, width, rowHeight,
}: StatusChartProps) => {
    const theme = useTheme();
    const canvasContainer = useRef<HTMLDivElement>(null);
    const canvas = useRef<HTMLCanvasElement>(null);
    const { action: drawExt = (): void => { }, triggers = [] } = decorator?.(session, metadata) ?? {};

    const dataState = useData({ session, mapFunc, unit, metadata, width, processor: zipStatusData });
    const rangeAndDomain = useRangeAndDomain(session, width, margin);

    const mousePosX = useHoverPosX(canvasContainer);
    const hoveredData = useMemo(() => findDataByX(mousePosX, dataState, rangeAndDomain), [mousePosX, dataState, rangeAndDomain]);
    const handleMouseUp = (e: MouseEvent): void => {
        const clickedData = findDataByX(e.offsetX, dataState, rangeAndDomain);
        runInAction(() => {
            session.selectedData = clickedData
                ? { ...clickedData, threadId: (metadata as ThreadMetaData).threadId ?? '', processId: (metadata as ThreadMetaData).processId ?? '' }
                : undefined;
            if (!session.selectedData) {
                session.drawLineMode = 'all';
            }
            onClick?.(clickedData, session, metadata);
        });
    };
    useEffect(() => onHover?.(hoveredData, session, metadata), [hoveredData, metadata]);
    useClick({ canvasContainer, dataState, rangeAndDomain, session, metadata, handleMouseUp });
    useBatchedRender(() => {
        const isCanvasInvalid = canvasContainer.current === null || canvas.current === null || rangeAndDomain.length === 0 ||
            canvas.current.width === 0 || canvas.current.height === 0;
        if (isCanvasInvalid) { return; }
        const ctx = canvas.current.getContext('2d');
        const xScale = d3.scaleLinear().range(rangeAndDomain[0]).domain(rangeAndDomain[1]).clamp(false);
        const yScale = (n: number): number => n * rowHeight;
        const startY = ((height - rowHeight) / 2) + 1;
        ctx?.setTransform(1, 0, 0, 1, 0, 0);
        ctx?.scale(devicePixelRatio, devicePixelRatio);
        ctx?.clearRect(0, 0, width, height);
        if (shouldHidePreview(unit)) { return; }
        draw({ ctx, data: dataState, xScale, yScale, theme, startY });
        drawExt({
            context: ctx,
            draw: (data, scaleX, scaleY) => draw({ ctx, data: data, xScale: scaleX, yScale: scaleY, theme, startY }),
            findAll: (condition) => dataState.filter(condition),
        }, xScale, yScale, theme);
    }, [dataState, rangeAndDomain, ...triggers, theme, unit.isExpanded]);

    const tooltipProp: TooltipProps<StatusData, StatusData[]> = {
        data: hoveredData,
        mouseX: mousePosX ?? null,
        session,
        dataset: dataState,
        calcHeight: () => height / 2,
        dom: canvasContainer,
        renderContent: (data) => renderTooltip?.(data),
    };

    return <CanvasContainer ref={canvasContainer} className={'canvasContainer'} width={width} height={height}>
        <SkeletonWrapper>
            <Skeleton loading={unit.isSummaryLoading} active paragraph={false}>
                <TooltipComponent {...tooltipProp} />
                <Canvas className={'drawCanvas'} ref={canvas} width={width * devicePixelRatio} height={height * devicePixelRatio} />
            </Skeleton>
        </SkeletonWrapper>
    </CanvasContainer>;
});

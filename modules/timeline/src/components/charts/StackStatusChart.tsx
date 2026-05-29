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
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import React, { useEffect, useMemo, useRef } from 'react';
import type { ChartProps, ChartReaction, Scale, StackStatusData, TextConfig } from '../../entity/chart';
import { UnitHeight } from '../../entity/insight';
import type { Session } from '../../entity/session';
import { Canvas, CanvasContainer, zipStatusData } from './common';
import { useBatchedRender, useClick, useData, useHoverPos, useRangeAndDomain } from './hooks';
import { TooltipComponent, type TooltipProps } from './TooltipComp';
import type { ThreadMetaData } from '../../entity/data';
import { Spin } from 'antd';

type StackStatusChartProps = ChartProps<'stackStatus'>;
type OverflowType = 'hidden' | 'ellipsis';
type DrawTextType = Array<StackStatusData & { width: number }>;

const FONT_SIZE = 12;
const DFT_PADDING = 8;
const CHEVRON_WIDTH = 12;

const fontMetricsCache = new Map();

const getYOffset = (ctx: CanvasRenderingContext2D): number => {
    const font = ctx.font;
    if (fontMetricsCache.has(font)) {
        return fontMetricsCache.get(font);
    }

    // 使用代表性的大写字母进行精准度量，以获取在当前操作系统/字体下的真实视觉高度中心
    const m = ctx.measureText('M');
    // 注意：这里我们故意只用 actualBoundingBox，因为它反映的是像素渲染级的真实轮廓
    // 不用 fontBoundingBox，因为 fontBoundingBox 在 Windows 的 Segoe UI 下通常偏高
    const ascent = m.actualBoundingBoxAscent;
    const descent = m.actualBoundingBoxDescent;
    const offset = (ascent - descent) / 2;

    fontMetricsCache.set(font, offset);
    return offset;
};

const getMaxText = (text: string, maxWidth: number, ctx: CanvasRenderingContext2D, overflow: OverflowType): string => {
    if (ctx.measureText(text).width <= maxWidth) { return text; }
    if (overflow === 'hidden') { return ''; }
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
    return `${text.slice(0, mid)}...`;
};

/**
 * 画人字形型箭头
 * @param ctx
 * @param x 箭头顶部 x 坐标
 * @param y 箭头顶部 y 坐标
 * @param h 箭头高
 */
export const drawChevron = (
    ctx: CanvasRenderingContext2D,
    x: number,
    y: number,
    h: number,
): void => {
    const HALF_CHEVRON_WIDTH = CHEVRON_WIDTH / 2;
    ctx.beginPath();
    ctx.moveTo(x, y);
    ctx.lineTo(x + HALF_CHEVRON_WIDTH, y + h);
    ctx.lineTo(x, y + h - HALF_CHEVRON_WIDTH);
    ctx.lineTo(x - HALF_CHEVRON_WIDTH, y + h);
    ctx.lineTo(x, y);
    ctx.closePath();
    ctx.fill();
};

// 计算当前节点宽度及圆角半径大小，同时将需要写的文字提前放入数组保存
const drawRect = (ctx: CanvasRenderingContext2D, dataObj: { data: StackStatusData; textToDraw: DrawTextType },
    config: { height: number; right: number; xScale: Scale; yScale: Scale; overflow: OverflowType; minTextWidth: number; order?: number },
    isSimulation: boolean): void => {
    const { data, textToDraw } = dataObj;
    const { height, right, xScale, yScale, overflow, minTextWidth, order } = config;
    let startTime = xScale(data.startTime);
    let width = Math.max(1, xScale(data.duration < 0 ? right : (data.startTime + data.duration)) - startTime);
    const minWidth = overflow === 'ellipsis' ? minTextWidth : ctx.measureText(data.type).width + DFT_PADDING;
    if (width >= minWidth) {
        textToDraw.push({ ...data, width });
    }
    if (order !== undefined && data.color instanceof Array) {
        // 第一个圆角矩形块加0.5是为了解决canvas绘图毛边的问题和红色块会在分割线前一点点开始绘制的问题
        width = order === 0 ? Math.abs(xScale(data.color[0][0]) - startTime) + 0.5 : Math.abs(xScale(data.color[1][0]) - xScale(data.color[0][0]));
        startTime = order === 0 ? startTime : xScale(data.color[0][0]) + 0.5;
    }
    // 算子仿真图的缩略图体现并行度
    if (isSimulation && height < UnitHeight.STANDARD - 1) {
        ctx.fillRect(startTime, 1, width, UnitHeight.COLL - 2);
        return;
    }
    if (data.duration === 0) {
        const startHeight = yScale(data.depth) + 1;
        drawChevron(ctx, startTime, startHeight, height - 2);
        return;
    }
    ctx.fillRect(startTime, yScale(data.depth) + 1, width, height - 2);
};

function dealDataColor(theme: Theme, dataColor: Map<keyof Theme['colorPalette'], StackStatusData[]>,
    dataMultiColor: StackStatusData[], dataList: StackStatusData[][]): void {
    Object.keys(theme.colorPalette).forEach(key => {
        dataColor.set(key as keyof Theme['colorPalette'], []);
    });
    dataList.forEach(it => it.forEach(data => {
        // 目前只支持最多2种背景颜色的情况，下面绘制多色背景时for循环两次也是这样原因
        if (data.color instanceof Array) {
            if (data.color.length === 2) {
                dataMultiColor.push(data);
            }
        } else {
            dataColor.get(data.color)?.push(data);
        }
    }));
}

interface DrawParams {
    ctx: CanvasRenderingContext2D | null;
    dataList: StackStatusData[][];
    xScale: Scale;
    yScale: Scale;
    theme: Theme;
    right: number;
    isSimulation: boolean;
    textConfig?: TextConfig;
}

const draw = ({ ctx, dataList, xScale, yScale, theme, right, isSimulation, textConfig }: DrawParams): void => {
    if (!ctx) {
        return;
    }
    const { overflow, textAlign } = textConfig ?? { overflow: 'ellipsis', textAlign: 'start' };
    ctx.font = `${FONT_SIZE}px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif`;
    ctx.textAlign = textAlign;
    ctx.textBaseline = 'top';
    const minTextWidth = ctx.measureText('...').width + DFT_PADDING;
    // draw by color order
    // change fillstyle as less as possible
    const dataColor = new Map<keyof Theme['colorPalette'], StackStatusData[]>();
    const dataMultiColor: StackStatusData[] = [];
    dealDataColor(theme, dataColor, dataMultiColor, dataList);
    const height = yScale(1) - yScale(0);
    const textToDraw: DrawTextType = [];
    const func = (arr: StackStatusData[], key: keyof Theme['colorPalette']): void => {
        ctx.fillStyle = theme.colorPalette[key];
        arr.forEach(data => {
            drawRect(ctx, { data, textToDraw }, { height, right, xScale, yScale, overflow, minTextWidth }, isSimulation);
        });
    };
    // 绘制一个背景的节点
    dataColor.forEach(func);
    // 绘制有多个背景色的节点
    if (dataMultiColor.length > 0) {
        for (let i = 0; i < 2; i++) {
            ctx.fillStyle = theme.colorPalette[dataMultiColor[0].color[i][1] as keyof Theme['colorPalette']];
            dataMultiColor.forEach(data => {
                drawRect(ctx, { data, textToDraw }, { height, right, xScale, yScale, overflow, minTextWidth, order: i }, isSimulation);
            });
        }
    }
    // 绘制节点上的文字
    ctx.fillStyle = '#FFFFFFE6';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'alphabetic';
    if (height > UnitHeight.STANDARD - 1) {
        textToDraw.forEach(data => {
            const text = getMaxText(data.type, data.width - DFT_PADDING, ctx, overflow);
            const yOffset = getYOffset(ctx);
            const x = xScale(data.startTime) + data.width / 2;
            const y = yScale(data.depth) + height / 2 + yOffset;

            ctx.fillText(text, x, y);
        });
    }
};

const findDataByXY = (mousePos: { x: number; y: number } | undefined, dataList: StackStatusData[][],
    rangeAndDomain: Array<[number, number]>, depthHeight: number, endTime: number): StackStatusData | undefined => {
    if (mousePos === undefined || dataList.length === 0 || rangeAndDomain.length < 2) {
        return undefined;
    }
    const mouseTime = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false)(mousePos.x);
    const range = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false)(CHEVRON_WIDTH / 2);
    const depth = Math.floor(mousePos.y / depthHeight);
    const data = dataList[depth];
    if (data === undefined || data.length === 0) {
        return undefined;
    }
    let rangeTime = data[0].duration === 0 ? range - rangeAndDomain[1][0] : 0;
    if (data[0].startTime - rangeTime > mouseTime || endTime + rangeTime < mouseTime ||
        (data[data.length - 1].startTime + data[data.length - 1].duration + rangeTime < mouseTime)) {
        return undefined;
    }
    let lo = 0;
    let hi = data.length;
    while (lo < hi) {
        const mid = Math.floor((lo + hi) / 2);
        const elem = data[mid];
        rangeTime = elem.duration === 0 ? range - rangeAndDomain[1][0] : 0;
        if (mouseTime < elem.startTime - rangeTime) {
            hi = mid;
        } else if ((elem.duration >= 0 && mouseTime > elem.startTime + elem.duration + rangeTime)) {
            lo = mid + 1;
        } else {
            return elem;
        }
    }
    return undefined;
};

const findDataByXXRange = ([downX, upX]: number[], dataList: StackStatusData[][],
    rangeAndDomain: Array<[number, number]>): StackStatusData[] | undefined => {
    const isUndefinedOrEmpty = downX === undefined || upX === undefined || dataList.length === 0 || rangeAndDomain.length === 0;
    if (isUndefinedOrEmpty) {
        return undefined;
    }
    const sX = Math.min(downX, upX);
    const eX = Math.max(downX, upX);
    const mouseSTime = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false)(sX);
    const mouseETime = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false)(eX);

    const result = [] as StackStatusData[];
    dataList.forEach((data) => {
        data.forEach((elem) => {
            if (elem.startTime < mouseETime && elem.startTime + elem.duration > mouseSTime) {
                result.push(elem);
            }
        });
    });
    return result.length > 0 ? result : undefined;
};

interface MouseUpFuncParams {
    e: MouseEvent;
    datasState: StackStatusData[][];
    rangeAndDomain: Array<[number, number]>;
    rowHeight: UnitHeight; session: Session;
    metadata: unknown;
    onClick?: ChartReaction<'stackStatus'>;
}
const mouseUpFunc = ({ e, datasState, rangeAndDomain, rowHeight, session, metadata, onClick }: MouseUpFuncParams): void => {
    const clickedData = findDataByXY({ x: e.offsetX, y: e.offsetY }, datasState, rangeAndDomain, rowHeight, session.endTimeAll ?? 0);
    if (clickedData !== undefined) {
        clickedData.showSelectedData = true; // 强制设置优先显示“选中详情”
    }
    runInAction(() => {
        session.selectedData = clickedData
            ? {
                ...clickedData,
                threadId: (metadata as ThreadMetaData).threadId ?? clickedData.threadId ?? '',
                processId: (metadata as ThreadMetaData).processId ?? '',
                timestamp: clickedData.originalStartTime as number,
                metaType: (metadata as ThreadMetaData).metaType ?? '',
            }
            : undefined;
        if (!session.selectedData) {
            session.drawLineMode = 'all';
        }
        onClick?.(clickedData, session, metadata);
        session.selectedRangeData = undefined;
    });
};

const mouseMoveUpFunc = ([downX, upX]: number[], datasState: StackStatusData[][], rangeAndDomain: Array<[number, number]>,
    session: Session, metadata: unknown): void => {
    const selectedRangeData = findDataByXXRange([downX, upX], datasState, rangeAndDomain);
    const threadMetaData = metadata as ThreadMetaData;
    runInAction(() => {
        session.selectedRangeData = selectedRangeData?.map(item => ({
            ...item,
            threadId: threadMetaData.threadId ?? item.threadId,
            processId: threadMetaData.processId ?? '',
            cardId: threadMetaData.cardId,
            dbPath: threadMetaData.dbPath,
            metaType: threadMetaData.metaType ?? '',
        }));
    });
};

// eslint-disable-next-line max-lines-per-function
export const StackStatusChart = observer(({ // 绘制 slice 的画布
    session, unit, margin, mapFunc, metadata, renderTooltip, height, onHover, onClick, decorator,
    rowHeight, width, textConfig, isNeedClamp, isCollapse, maxDepth,
}: StackStatusChartProps) => {
    const theme = useTheme();
    const canvasContainer = useRef<HTMLDivElement>(null);
    const canvas = useRef<HTMLCanvasElement>(null);
    const { action: drawExt = (): void => { }, triggers = [] } = decorator?.(session, metadata) ?? {};
    const datasState = useData({
        session,
        mapFunc,
        unit,
        metadata,
        width,
        processor: (data, processedWidth, start, end) => data.map(row => zipStatusData(row, processedWidth, start, end)),
    });
    const rangeAndDomain = useRangeAndDomain(session, width, margin); const mousePos = useHoverPos(canvasContainer);
    const hoveredData = useMemo(
        () => findDataByXY(mousePos, datasState, rangeAndDomain, rowHeight, session.endTimeAll ?? 0),
        [mousePos, datasState, rangeAndDomain],
    );
    const handleMouseUp = (e: MouseEvent): void => {
        mouseUpFunc({
            e,
            datasState,
            rangeAndDomain,
            rowHeight,
            session,
            metadata,
            onClick,
        });
    };
    const handleMouseMoveUp = ([downX, upX]: number[]): void => { mouseMoveUpFunc([downX, upX], datasState, rangeAndDomain, session, metadata); };
    useEffect(() => onHover?.(hoveredData, session, metadata), [hoveredData, metadata]);
    useClick({ canvasContainer, dataState: datasState, rangeAndDomain, session, metadata, handleMouseUp, handleMouseMoveUp });
    const yScale = isCollapse ? d3.scaleLinear().range([0, height]).domain([0, maxDepth as number]) : (depth: number): number => depth * rowHeight;
    useBatchedRender(() => {
        const noRender = canvasContainer.current === null || canvas.current === null || rangeAndDomain.length === 0 ||
            canvas.current.width === 0 || canvas.current.height === 0;
        if (noRender) {
            return;
        }
        const ctx = canvas.current.getContext('2d');
        const xScale = d3.scaleLinear().range(rangeAndDomain[0]).domain(rangeAndDomain[1]).clamp(isNeedClamp ?? true);
        ctx?.resetTransform();
        ctx?.scale(devicePixelRatio, devicePixelRatio);
        ctx?.clearRect(0, 0, width, height);
        draw({ ctx, dataList: datasState, xScale, yScale, theme, right: session.endTimeAll ?? 0, isSimulation: session.isSimulation, textConfig });
        drawExt({
            context: ctx,
            draw: (data, xScaleExt, yScaleExt) => draw({
                ctx,
                dataList: data,
                xScale: xScaleExt,
                yScale: yScaleExt,
                theme,
                right: session.endTimeAll ?? 0,
                isSimulation: session.isSimulation,
                textConfig,
            }),
            findAll: (condition) => datasState.map(it => it.filter(condition)),
        }, xScale, yScale, theme);
    }, [datasState, rangeAndDomain, ...triggers, theme, isCollapse]);

    const tooltipProp: TooltipProps<StackStatusData, StackStatusData[][]> = {
        data: hoveredData,
        mouseX: mousePos?.x ?? null,
        session,
        dataset: datasState,
        calcHeight: (data) => (data.depth * rowHeight) + (rowHeight / 2),
        dom: canvasContainer,
        renderContent: (data) => renderTooltip?.(data),
    };

    return <CanvasContainer ref={canvasContainer} className={'canvasContainer'} width={width} height={height} style={{ pointerEvents: `${isCollapse ? 'none' : 'auto'}` }}>
        { !isCollapse && <TooltipComponent {...tooltipProp} /> }
        <Spin spinning={unit.isTraceLoading} delay={2500} size="small">
            <Canvas className={'drawCanvas'} ref={canvas} width={width * devicePixelRatio} height={height * devicePixelRatio}/>
        </Spin>
    </CanvasContainer>;
});

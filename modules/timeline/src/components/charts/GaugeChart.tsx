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
import { observer } from 'mobx-react';
import React, { useMemo, useRef } from 'react';
import type { Session } from '../../entity/session';
import { logger } from '../../utils/Logger';
import { useAsyncEffect } from '../../utils/useEffectHooks';
import styled from '@emotion/styled';

export interface ArcData {
    startAngle: number;
    endAngle: number;
    innerRadius: number;
    outerRadius: number;
};

interface GaugeChartProps {
    session: Session;
    valueFormat: (value: number) => string;
    mapFunc: () => Promise<Array<[number, string]>>;
    palette: Array<keyof Theme['colorPalette']>;
    totalFormat?: [ () => Promise<string>, string? ];
};

const margin = 30;

const CanvasChart = styled.canvas`
    height: 100%;
    width: 100%;
    padding: 0;
    margin: 0;
    display: block;
`;

interface DrawLeftParams {
    dataList: Array<[ number, string ]>;
    palette: string[];
    canvasHeight: number;
};
const drawLeft = (ctx: CanvasRenderingContext2D, {
    dataList,
    palette,
    canvasHeight,
}: DrawLeftParams): (undefined | {startAngle: number; endAngle: number; innerRadius: number; outerRadius: number}) => {
    const step = 78;
    const totalValue = dataList.reduce((previousValue, item) => previousValue + item[0], 0);
    if (totalValue === 0) {
        ctx.beginPath();
        ctx.arc(margin + (step / 2), canvasHeight / 2, step / 3, 0, 2 * Math.PI);
        ctx.arc(margin + (step / 2), canvasHeight / 2, step / 2, 0, 2 * Math.PI, true);
        ctx.fillStyle = 'grey';
        ctx.fill();
        ctx.closePath();
        return;
    }
    let nowRadStart = 0;
    const arcData: ArcData[] = dataList.map((item, index) => {
        const value = {
            startAngle: nowRadStart,
            endAngle: nowRadStart + (2 * Math.PI * item[0] / totalValue),
            innerRadius: step / 3,
            outerRadius: step / 2,
        };
        nowRadStart = value.endAngle;
        if (index === dataList.length - 1) {
            value.endAngle = 2 * Math.PI;
        }
        return value;
    });
    arcData.forEach((item, index) => {
        ctx.beginPath();
        ctx.arc(margin + (step / 2), canvasHeight / 2, item.innerRadius, item.startAngle, item.endAngle);
        ctx.arc(margin + (step / 2), canvasHeight / 2, item.outerRadius, item.endAngle, item.startAngle, true);
        ctx.fillStyle = palette[index];
        ctx.fill();
        ctx.closePath();
    });
};

const drawTotalText = (ctx: CanvasRenderingContext2D, height: number, totalFormat: [ string, string? ], theme: Theme): void => {
    const step = 78;
    ctx.fillStyle = theme.fontColor;
    ctx.font = 'Bold 10px MicrosoftYaHei';
    const totalLength = ctx.measureText('Total').width;
    const valueLength = ctx.measureText(totalFormat[0]).width;
    if (totalFormat[1] === undefined) {
        ctx.fillText('Total', margin + (step / 2) - (totalLength / 2), (height / 2) - 3);
        ctx.fillText(totalFormat[0], margin + (step / 2) - (valueLength / 2), (height / 2) + 13);
        return;
    }
    ctx.fillText('Total', margin + (step / 2) - (totalLength / 2), (height / 2) - 10);
    ctx.fillText(totalFormat[0], margin + (step / 2) - (valueLength / 2), (height / 2) + 6);
    ctx.fillStyle = theme.tableHeadFontColor;
    ctx.font = 'Regular 10px PingFangSC';
    const unitLength = ctx.measureText(totalFormat[1]).width;
    ctx.fillText(totalFormat[1], margin + (step / 2) - (unitLength / 2), (height / 2) + 20);
};

interface DrawRightParams {
    range: [ number, number ];
    dataList: Array<[ number, string ]>;
    valueFormat: (value: number) => string;
    theme: Theme;
    palette: string[];
    canvasHeight: number;
};
const drawRight = (ctx: CanvasRenderingContext2D, {
    range,
    dataList,
    valueFormat,
    theme,
    palette,
    canvasHeight,
}: DrawRightParams): void => {
    if (dataList.length === 0) { return; }
    const xScale = d3.scaleLinear().range(range).domain([0, 10]);
    const ratio = 0.15;
    const yOffset = canvasHeight / dataList.length;
    const rectSizePara = canvasHeight * ratio / 5;
    const paddingTop = 10;
    const radius = canvasHeight * ratio / 6;
    dataList.forEach((item, index) => {
        const positionY = (index * yOffset) + paddingTop;
        ctx.fillStyle = palette[index];
        ctx.beginPath();
        ctx.arc(xScale(0), positionY, radius, Math.PI, Math.PI * 1.5);
        ctx.arc(xScale(0) + rectSizePara, positionY, radius, Math.PI * 1.5, 0);
        ctx.arc(xScale(0) + rectSizePara, positionY + rectSizePara, radius, 0, Math.PI * 0.5);
        ctx.arc(xScale(0), positionY + rectSizePara, radius, Math.PI * 0.5, Math.PI);
        ctx.closePath();
        ctx.fillStyle = palette[index];
        ctx.fill();
        ctx.fillStyle = theme.fontColor;
        ctx.font = '12px sans-serif';
        ctx.fillText(item[1], xScale(1), positionY + rectSizePara + radius);
        ctx.fillText(valueFormat(item[0]), xScale(8), positionY + rectSizePara + radius);
    });
};

export const GaugeChart = observer(({ session, valueFormat, mapFunc, totalFormat, palette }: GaugeChartProps) => {
    const canvas = useRef<HTMLCanvasElement>(null);
    const theme = useTheme();
    const topHeight = 11;
    const colorPalette = useMemo(() => palette.map(d => theme.colorPalette[d]), [palette, theme]);
    const draw = async (isCanceled: () => boolean): Promise<void> => {
        let dataList: Array<[number, string]> = [];
        try {
            dataList = await mapFunc();
        } catch {
            logger('GaugeChart', 'mapFunc occurred an exception.');
        }
        if (isCanceled() || !canvas.current) { return; }
        const height = canvas.current.clientHeight;
        const width = canvas.current.clientWidth;
        const rangeRight: [ number, number ] = [160, 350];
        canvas.current.width = width;
        canvas.current.height = height;
        const ctx = canvas.current.getContext('2d');
        if (!ctx) { return; }
        drawLeft(ctx, { dataList, canvasHeight: height - topHeight, palette: colorPalette });
        drawRight(ctx, { range: rangeRight, dataList, valueFormat, theme, palette: colorPalette, canvasHeight: 90 });
        if (totalFormat !== undefined) {
            const format: [ string, string | undefined ] = [(await totalFormat[0]()).toString(), totalFormat[1]];
            drawTotalText(ctx, height - topHeight, format, theme);
        }
    };
    useAsyncEffect(draw, [session.endTimeAll, theme]);
    return <CanvasChart ref={canvas} width={400} height={90} />;
});

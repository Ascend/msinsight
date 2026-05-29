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
/** @jsxImportSource @emotion/react */
import { observer } from 'mobx-react';
import React, { useRef } from 'react';
import type { Session } from '../../entity/session';
import styled from '@emotion/styled';
import { useAsyncEffect } from '../../utils/useEffectHooks';
import { useTheme } from '@emotion/react';
import type { Theme } from '@emotion/react';
import { logger } from '../../utils/Logger';

interface GaugeChartHalfRoundProps {
    session: Session;
    splits: Array<[number, string]>;
    mapFunc: () => Promise<number>;
    type: string;
    getDisplayNumber: Formatter;
};

const CanvasChart = styled.canvas`
    padding: 0;
    margin: 0;
    display: block;
`;

const ratio = 0.65;
const canvasHeight = 90;
const thickness = canvasHeight * ratio / 4.588;
const radius = canvasHeight * ratio;
const paddingInPie = 0.05;
const handleThickness = canvasHeight * ratio / 3;
const handleRadius = 0.03;
const needleHeight = canvasHeight * ratio / 1.696;
const needleThickness = canvasHeight * ratio / 15.6;
const paddingTop = canvasHeight * ratio / 0.9;

const drawNeedle = (theme: Theme, context: CanvasRenderingContext2D, width: number, maxNumber: number, data: number): void => {
    if (maxNumber === 0) {
        return;
    }
    context.save();
    context.translate(width / 2, paddingTop);
    context.rotate((Math.PI / 2) + (Math.PI * data / maxNumber));
    context.beginPath();
    context.moveTo(-needleThickness / 2, (radius + (thickness / 2)) - needleHeight);
    context.lineTo(needleThickness / 2, (radius + (thickness / 2)) - needleHeight);
    context.lineTo(0, radius + (thickness / 2));
    context.closePath();
    context.fillStyle = theme.fpsPointersColor;
    context.fill();
    context.restore();
};
interface Iparams {
    type: string;
    theme: Theme;
    context: CanvasRenderingContext2D;
    width: number;
    data: number;
    getDisplayNumber: Formatter;
}
const drawText = (params: Iparams): void => {
    const { type, theme, context, width, data, getDisplayNumber } = params;
    context.textAlign = 'center';
    context.textBaseline = 'bottom';
    context.font = '22px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
    context.fillStyle = theme.fpsTextColor;
    context.fillText(getDisplayNumber(data), width / 2, paddingTop);
    context.textBaseline = 'top';
    context.font = '12px -apple-system, BlinkMacSystemFont, "Segoe UI", "Roboto", "Oxygen", "Ubuntu", "Cantarell", "Fira Sans", "Droid Sans", "Helvetica Neue", sans-serif';
    context.fillStyle = theme.fpsColor;
    context.fillText(type, width / 2, paddingTop);
};

interface DrawLengendParams {
    width: number;
    maxNumber: number;
    splits: Array<[number, string]>;
    canvasHeight: number;
};
const drawLegend = (context: CanvasRenderingContext2D, {
    width,
    maxNumber,
    splits,
}: DrawLengendParams): void => {
    if (maxNumber === 0) {
        return;
    }
    let lastStartAngle = Math.PI;
    splits.forEach((d, index) => {
        context.beginPath();
        const endAngle = Math.PI + (Math.PI * d[0] / maxNumber);
        if (index === splits.length - 1) {
            // last one don't need padding
            context.arc(width / 2, paddingTop, radius, lastStartAngle, endAngle);
        } else {
            context.arc(width / 2, paddingTop, radius, lastStartAngle, endAngle - paddingInPie);
        }
        context.strokeStyle = d[1];
        context.lineWidth = thickness;
        context.stroke();
        context.beginPath();
        context.arc(width / 2, paddingTop, radius - ((handleThickness - thickness) / 2), lastStartAngle, lastStartAngle + handleRadius);
        context.lineWidth = handleThickness;
        context.stroke();
        lastStartAngle = endAngle;
    });
};

export const GaugeChartHalfRound = observer(({ session, splits, mapFunc, type, getDisplayNumber }: GaugeChartHalfRoundProps) => {
    const canvas = useRef<HTMLCanvasElement>(null);
    const theme = useTheme();
    const draw = async (isCanceled: () => boolean): Promise<void> => {
        let data: number = 0;
        try {
            data = await mapFunc();
        } catch {
            logger('GaugeChartHalfRound', 'mapFunc occurred an exception.');
        }
        if (isCanceled() || !canvas.current) { return; }
        const height = canvas.current.clientHeight;
        const width = canvas.current.clientWidth;
        canvas.current.width = width;
        canvas.current.height = height;
        const context = canvas.current.getContext('2d');
        if (!context) { return; }
        // find max number
        let maxNumber = 0;
        splits.forEach(d => { maxNumber = Math.max(maxNumber, d[0]); });
        // draw legend
        drawLegend(context, { width, maxNumber, splits, canvasHeight });
        // draw needle
        drawNeedle(theme, context, width, maxNumber, data);
        // draw text
        drawText({ type, theme, context, width, data, getDisplayNumber });
    };
    useAsyncEffect(draw, [session.endTimeAll, theme]);
    return <CanvasChart ref={canvas} width={400} height={canvasHeight} />;
});

type Formatter = (data: number) => string;

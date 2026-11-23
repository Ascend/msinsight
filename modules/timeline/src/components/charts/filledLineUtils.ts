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

export type FilledLineSeriesMode = 'stacked' | 'overlay';

/** Explicit intervals are half open; an absent interval must never inherit a neighbouring value. */
export const findFilledLineInterval = (
    dataList: number[][], intervalEnds: number[], timestamp: number,
): number[] | undefined => {
    if (!Number.isFinite(timestamp)) { return undefined; }
    let left = 0;
    let right = dataList.length;
    while (left < right) {
        const middle = Math.floor((left + right) / 2);
        if (dataList[middle][0] <= timestamp) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    const index = left - 1;
    const row = dataList[index];
    if (row === undefined || row.length < 2 || !row.every(Number.isFinite) ||
        !Number.isFinite(intervalEnds[index]) || timestamp >= intervalEnds[index]) {
        return undefined;
    }
    return row;
};

interface FilledLineIntervalDrawParams {
    context: CanvasRenderingContext2D;
    dataList: number[][];
    intervalEnds: number[];
    xScale: (value: number) => number;
    yScale: (value: number) => number;
    minHeight: number;
    palette: string[];
    seriesMode: FilledLineSeriesMode;
    isIE: boolean;
}

type AreaParams = Pick<FilledLineIntervalDrawParams, 'context' | 'dataList' | 'xScale' | 'yScale' | 'minHeight' | 'palette' | 'isIE'>;

/** Sample-and-hold series end one pixel after their last point. Measured runs supply their actual end. */
export const drawFilledLineArea = ({
    context, dataList, xScale, yScale, minHeight, palette, isIE,
}: AreaParams, endTime?: number): void => {
    if (dataList.length === 0) { return; }
    const x = dataList.map(row => xScale(row[0]));
    x.push(endTime === undefined ? x[x.length - 1] + 1 : xScale(endTime));
    const baseline = yScale(minHeight);
    const columns = dataList[0].length;
    for (let series = 1; series < columns; series++) {
        context.beginPath();
        context.moveTo(x[0], baseline);
        dataList.forEach((row, index) => {
            const value = row.slice(series, columns).reduce((sum, item) => sum + item, 0);
            const top = yScale(value) - (columns - series);
            context.lineTo(x[index], top);
            context.lineTo(x[index + 1], top);
        });
        context.lineTo(x[dataList.length], baseline);
        if (isIE) {
            context.strokeStyle = palette[series - 1];
            context.stroke();
        } else {
            context.closePath();
            context.fillStyle = palette[series - 1];
            context.fill();
        }
    }
};

export const drawFilledLineIntervals = (params: FilledLineIntervalDrawParams): void => {
    const { context, dataList, intervalEnds, xScale, yScale, palette, seriesMode, isIE } = params;
    context.setLineDash([]);
    context.globalAlpha = 1;
    context.lineWidth = 1.5;
    const isDrawable = (index: number): boolean => {
        const row = dataList[index];
        const end = intervalEnds[index];
        return row.length >= 2 && row.every(Number.isFinite) && Number.isFinite(end) && end > row[0];
    };
    for (let index = 0; index < dataList.length;) {
        const start = index++;
        const row = dataList[start];
        if (!isDrawable(start)) { continue; }
        if (seriesMode === 'stacked' && !isIE) {
            // Join in time coordinates so even a subpixel gap still breaks the staircase.
            while (index < dataList.length && dataList[index][0] === intervalEnds[index - 1] &&
                dataList[index].length === row.length && isDrawable(index)) {
                index++;
            }
            drawFilledLineArea({ ...params, dataList: dataList.slice(start, index) }, intervalEnds[index - 1]);
            continue;
        }
        for (let seriesIndex = 1; seriesIndex < row.length; seriesIndex++) {
            const value = seriesMode === 'overlay' ? row[seriesIndex] : row.slice(seriesIndex).reduce((sum, item) => sum + item, 0);
            const top = yScale(value) - (seriesMode === 'overlay' ? 0 : row.length - seriesIndex);
            context.beginPath();
            context.moveTo(xScale(row[0]), top);
            context.lineTo(xScale(intervalEnds[start]), top);
            context.strokeStyle = palette[seriesIndex - 1];
            context.stroke();
        }
    }
};

export const findFilledLineValueRange = (
    dataList: number[][], seriesMode: FilledLineSeriesMode = 'stacked',
): [number, number] => {
    if (dataList.length === 0) { return [0, 0]; }
    let minHeight = 0;
    let maxHeight = 0;
    dataList.forEach(data => {
        if (seriesMode === 'overlay') {
            data.slice(1).forEach(value => {
                const height = isNaN(Number(value)) ? 0 : Number(value);
                minHeight = Math.min(minHeight, height);
                maxHeight = Math.max(maxHeight, height);
            });
            return;
        }
        let height = 0;
        data.forEach((value, index) => {
            if (index === 0) { return; }
            height += isNaN(Number(value)) ? 0 : Number(value);
        });
        minHeight = Math.min(minHeight, height);
        maxHeight = Math.max(maxHeight, height);
    });
    return [minHeight, maxHeight * 1.2];
};

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
import { useTheme } from '@emotion/react';
import styled from '@emotion/styled';
import * as d3 from 'd3';
import { observer } from 'mobx-react';
import React, { useMemo, useRef, useState } from 'react';
import type { ChartProps, Scale } from '../../entity/chart';
import type { Readable } from '../../utils/humanReadable';
import { Canvas, CanvasContainer, LegendArea, search, zipTimeSeriesData } from './common';
import { useBatchedRender, useData, useHoverPosX, useRangeAndDomain } from './hooks';
import { TooltipComponent, type TooltipProps } from './TooltipComp';

type FilledLineChartProps = ChartProps<'filledLine'>;

const findHeights = (dataList: number[][]): [number, number] => {
    if (dataList.length === 0) { return [0, 0]; }
    let minHeight = 0;
    let maxHeight = 0;
    dataList.forEach(data => {
        let height = 0;
        data.forEach((val, index) => {
            if (index === 0) { return; }
            height += isNaN(Number(val)) ? 0 : Number(val);
        });
        minHeight = Math.min(minHeight, height);
        maxHeight = Math.max(maxHeight, height);
    });
    return [minHeight, maxHeight * 1.2];
};

const drawAuxiliaryLine = (context: CanvasRenderingContext2D, yScale: Scale,
    auxiliaryValue: number, width: number): void => {
    context.beginPath();
    context.setLineDash([10, 10]);
    context.moveTo(0, yScale(auxiliaryValue));
    context.lineTo(width, yScale(auxiliaryValue));
    context.strokeStyle = 'white';
    context.globalAlpha = 0.2;
    context.stroke();
};

interface DrawAreaParams {
    context: CanvasRenderingContext2D;
    dataList: number[][];
    minHeight: number;
    maxHeight: number;
    xScale: (n: number) => number;
    yScale: (n: number) => number;
    palette: string[];
    width: number;
    isIE: boolean;
}

const drawArea = ({ context, dataList, minHeight, maxHeight, xScale, yScale, palette, width, isIE }: DrawAreaParams): (undefined | number[]) => {
    if (dataList.length === 0) { return; }

    const x = [...dataList.map(it => xScale(it[0])), 1 + xScale(dataList[dataList.length - 1][0])];
    const y0 = yScale(minHeight);
    for (let i = 1; i < dataList[0].length; i++) {
        context.beginPath();
        context.moveTo(x[0], y0);
        for (let j = 0; j < dataList.length; j++) {
            const y = yScale(dataList[j].slice(i, dataList[0].length).reduce((prev, cur) => prev + cur, 0)) - (dataList[0].length - i);
            context.lineTo(x[j], y);
            context.lineTo(x[j + 1], y);
        }
        if (!isIE) {
            context.lineTo(x[dataList.length], y0);
            context.closePath();
            context.fillStyle = palette[i - 1];
            context.fill();
        } else {
            context.lineTo(x[dataList.length], y0);
            context.strokeStyle = palette[i - 1];
            context.stroke();
        }
    };
};

interface DrawParams {
    ctx: CanvasRenderingContext2D | null;
    dataList: number[][];
    width: number;
    height: number;
    palette: string[];
    rangeAndDomain: Array<[ number, number ]>;
    hideLayer: number[];
    valueRange?: [ number, number ];
    auxiliaryValue?: number;
    legend?: string[];
    valueFormat?: Readable;
    isIE: boolean;
}

const draw = ({ ctx, dataList, width, height, palette, rangeAndDomain, hideLayer, valueRange, auxiliaryValue, legend, valueFormat, isIE }: DrawParams):
(HTMLCanvasElement | undefined) => {
    if (!ctx) { return; }
    if (dataList.length === 0 || dataList[0].length > palette.length + 1) { return; }
    const [drawDataList, newPalette] = removeHideLayerDatas(dataList, palette, hideLayer);
    let minHeight = 0;
    let maxHeight = 0;
    [minHeight, maxHeight] = valueRange ?? findHeights(drawDataList);
    maxHeight = maxHeight === 0 ? 1 : maxHeight;
    const xScale = d3.scaleLinear().range(rangeAndDomain[0]).domain(rangeAndDomain[1]).clamp(false);
    const yScale = d3.scaleLinear().range([height, 0]).domain([minHeight, maxHeight]);
    if (auxiliaryValue !== undefined) { drawAuxiliaryLine(ctx, yScale, auxiliaryValue, width); }
    // draw line and area
    drawArea({ context: ctx, dataList: drawDataList, minHeight, maxHeight, xScale, yScale, palette: newPalette, width, isIE });
};

const findDataByX = (mousePosX: number | undefined, dataListState: number[][],
    rangeAndDomain: Array<[number, number]>): undefined | number[] => {
    if (rangeAndDomain.length === 0 || dataListState.length === 0 || mousePosX === undefined) {
        return undefined;
    }
    const reverseXScale = d3.scaleLinear().range(rangeAndDomain[1]).domain(rangeAndDomain[0]).clamp(false);
    const mouseTimestamp = reverseXScale(mousePosX);
    // 如果鼠标位置在所有关键点范围之外，不显示tooltip
    if (dataListState[0][0] > mouseTimestamp || dataListState[dataListState.length - 1][0] < mouseTimestamp) {
        return undefined;
    }
    return dataListState[search(dataListState, mouseTimestamp, it => it[0])];
};

const flipLayerBit = (flipBit: number, hideLayer: number[],
    setHideLayer: React.Dispatch<React.SetStateAction<number[]>>): void => {
    let bitInLayer = false;
    hideLayer.forEach((value, index, array) => {
        if (value === flipBit) {
            array.splice(index, 1);
            bitInLayer = true;
        }
    });
    if (!bitInLayer) {
        hideLayer.push(flipBit);
    }
    const ret = [...hideLayer];
    setHideLayer(ret);
};

interface LegendProps {
    legend?: string[];
    palette: string[];
    hideLayer: number[];
    setHideLayer: React.Dispatch<React.SetStateAction<number[]>>;
};

interface ColorBlockProps {
    bgColor: string;
    isHiding: boolean;
};

const ColorBlock = styled.div<ColorBlockProps>`
    background-color: ${(props): string => (props.isHiding ? 'none' : props.bgColor)};
    height: 12px;
    width: 12px;
    margin-right: 8px;
    border-width: 1px;
    display: inline-block;
    padding: 0;
    border-radius: 4px;
    border-style: ${(props): string => (props.isHiding ? 'dotted' : 'solid')};
    border-color: ${(props): string => props.bgColor};
    pointer-events: none;
`;

const LegendJSX = ({ legend, palette, hideLayer, setHideLayer }: LegendProps): JSX.Element => {
    if (legend === undefined) { return <></>; }
    const table: JSX.Element[] = [];
    legend.forEach((name, index): void => {
        table.push(<div style={{ marginRight: '16px', padding: 0, height: '12px' }} key={`legend_${index}`} onClick={(): void => {
            flipLayerBit(index, hideLayer, setHideLayer);
        }} className={'clickable'}>
            <ColorBlock isHiding={hideLayer.includes(index)} bgColor={palette[index]} key={`rect_${index}`}/>
            <div style={{ font: '12px "Microsoft YaHei", "PingFang SC", "Hiragino Sans GB", "Arial", "Narrow", "sans-serif"', verticalAlign: 'center', display: 'inline-block', pointerEvents: 'none' }} key={`text_${index}`}>{name}</div>
        </div>);
    });
    return <LegendArea id={'colorBox'}>{table.reverse()}</LegendArea>;
};

const removeHideLayerDatas = (dataList: number[][], palette: string[], hideLayer: number[]): [number[][], string[]] => {
    if (hideLayer.length === 0 || dataList.length === 0) { return [dataList, palette]; }
    const ret: number[][] = [];
    let newPalette: string[] = [];
    dataList.forEach(row => {
        const newRow = [...row];
        ret.push(newRow);
    });
    const sortedHideLayer = hideLayer.sort((x, y) => x - y);
    // for each row delete indexs in hideLayer
    // the 1st column is timestamp
    ret.forEach((row, index, array) => { array[index] = row.filter((elem, elemIndex) => !sortedHideLayer.includes(elemIndex - 1)); });
    newPalette = palette.filter((elem, index) => !sortedHideLayer.includes(index));
    return [ret, newPalette];
};

// eslint-disable-next-line max-lines-per-function
export const FilledLineChart = observer(({
    margin, session, mapFunc, valueFormat, valueRange, legend, auxiliaryValue, palette, renderTooltip, metadata, height, width, unit,
}: FilledLineChartProps) => {
    const theme = useTheme();
    const canvasContainer = useRef<HTMLDivElement>(null);
    const canvas = useRef<HTMLCanvasElement>(null);

    const dataListState = useData({ session, mapFunc, unit, metadata, width, processor: zipTimeSeriesData });
    const rangeAndDomain = useRangeAndDomain(session, width, margin);

    const [hideLayer, setHideLayer] = useState<number[]>([]);
    const mousePosX = useHoverPosX(canvasContainer);
    const hoveredData = useMemo(() => findDataByX(mousePosX, dataListState, rangeAndDomain), [mousePosX, dataListState, rangeAndDomain]);
    const colorPalette = useMemo(() => palette.map(d => theme.colorPalette[d]), [palette, theme]);

    const isCanvasOrContainerInvalid = canvasContainer.current === null || canvas.current === null;
    const isDataOrRangeEmpty = dataListState.length === 0 || rangeAndDomain.length === 0;
    const isCanvasSizeZero = canvas.current && (canvas.current.width === 0 || canvas.current.height === 0);

    useBatchedRender(() => {
        if (isCanvasOrContainerInvalid || isDataOrRangeEmpty || isCanvasSizeZero) {
            return;
        }
        const ctx = canvas.current.getContext('2d');
        ctx?.setTransform(1, 0, 0, 1, 0, 0);
        ctx?.scale(devicePixelRatio, devicePixelRatio);
        ctx?.clearRect(0, 0, width, height);
        const param: DrawParams = {
            ctx,
            dataList: dataListState,
            width,
            height,
            palette: colorPalette,
            rangeAndDomain,
            hideLayer,
            valueRange,
            auxiliaryValue,
            legend,
            valueFormat,
            isIE: session.isIE,
        };
        draw(param);
    }, [dataListState, rangeAndDomain, theme, valueRange, hideLayer, colorPalette]);

    const tooltipProp: TooltipProps<number[], number[][]> = {
        data: hoveredData,
        mouseX: mousePosX ?? null,
        session,
        dataset: dataListState,
        calcHeight: () => height / 2,
        dom: canvasContainer,
        renderContent: (data) => renderTooltip ? renderTooltip(data, metadata) : undefined,
    };
    return <CanvasContainer ref={canvasContainer} className={'canvasContainer'} width={width} height={height}>
        <TooltipComponent {...tooltipProp} />
        <Canvas className={'drawCanvas'} ref={canvas} width={width * devicePixelRatio} height={height * devicePixelRatio}/>
        <LegendJSX legend={legend} palette={colorPalette} hideLayer={hideLayer} setHideLayer={setHideLayer}/>
    </CanvasContainer>;
});

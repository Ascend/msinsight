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
import type { ReactNode } from 'react';
import type { Readable } from '../utils/humanReadable';
import type { AtomicObjectElementType } from './common';
import type { Session } from './session';
import type { InsightUnit, UnitHeight } from './insight';

export type SizePx = number;

interface ChartDataType<T, U> {
    config: T; // type of chart configuration data, used when creating the chart component, such as color mapping
    data: U; // type of chart data, used for drawing the chart
};

interface ChartDataDefinition {
    filledLine: ChartDataType<FilledLineConfig, number[][]>;
    stackedBar: ChartDataType<StackBarConfig, StackedBarData[]>;
    keyEvent: ChartDataType<Record<string, unknown>, EventData[]>;
    status: ChartDataType<StatusConfig, StatusData[]>;
    stackStatus: ChartDataType<StackStatusConfig, StackStatusData[][]>; // data is grouped by depth
    // to be extended...
};

export type ChartType = keyof ChartDataDefinition;

export type ChartData<T extends ChartType> = ChartDataDefinition[T]['data'];

export type ChartConfig<T extends ChartType> = ChartDataDefinition[T]['config'];

export type GetChartConfig<T extends ChartType> = (session: Session, metadata: unknown) => ChartDataDefinition[T]['config'];

export type Scale = (num: number) => number;

/**
 * A handle for defining custom drawing logic for charts.
 */
export interface ChartHandle<T extends ChartType> {
    /**
     * Canvas context
     */
    context: CanvasRenderingContext2D | null;

    /**
     * Draws some given data on the canvas.
     *
     * @param data data to be drawn
     * @param xScale a mapper for converting timestamp to x coordinate
     * @param yScale a mapper for y coordinate conversion
     */
    draw: (data: ChartData<T>, xScale: Scale, yScale: Scale) => void;

    /**
     * Finds all data in the current chart matching {@param predicate}
     */
    findAll: (predicate: (elem: AtomicObjectElementType<ChartData<T>>) => boolean) => ChartData<T>;
}

export type ChartReaction<T extends ChartType> = (data: AtomicObjectElementType<ChartData<T>> | undefined, session: Session, metadata: unknown) => void;

export type CustomChartDraw<T extends ChartType> = (handle: ChartHandle<T>, xScale: Scale, yScale: Scale, theme: Theme) => void;

export interface TextConfig { overflow: 'hidden' | 'ellipsis'; textMarginLeft: number | ((num: number) => number); textAlign: 'start' | 'center' };

export type ChartDecorator<T extends ChartType> = (session: Session, metadata: unknown) => {
    action: CustomChartDraw<T>;
    triggers: unknown[];
};

export type ChartProps<T extends ChartType> = ChartConfig<T> & {
    session: Session;
    unit: InsightUnit;
    margin: SizePx;
    width: number;
    height: number;
    mapFunc: MapFunc<T>;
    renderTooltip?: RenderTooltip<T>;
    title: string;
    metadata: unknown;
    onHover?: ChartReaction<T>;
    onClick?: ChartReaction<T>;
    decorator?: ChartDecorator<T>;
};

export interface MapFunc<T extends ChartType> {
    (session: Session, metadata?: unknown, unit?: InsightUnit, theme?: Theme): Promise<ChartData<T>>;
}

export interface RenderTooltip<T extends ChartType> {
    (data: AtomicObjectElementType<ChartData<T>>, metadata?: unknown): Map<string, string> | ReactNode;
}

// #region chart-specific data types
export interface BarEvent {
    yVal: number;
    event: string;
};

export interface EventData {
    name: string;
    duration: number;
    happenTime: number;
    endTime: number;
};

export interface StatusData {
    [x: string]: string | number | undefined | object;
    startTime: number;
    duration: number;
    name: string;
    type: string;
    color?: keyof Theme['colorPalette'] | Array<[ number, keyof Theme['colorPalette'] ]>;
    subName?: string;
};

export interface StackStatusData {
    [x: string]: unknown;
    startTime: number;
    duration: number;
    name: string;
    type: string;
    color: keyof Theme['colorPalette'] | Array<[ number, keyof Theme['colorPalette'] ]>;
    depth: number;
    cname: string;
    id?: string;
    threadId?: string;
};

export interface StackedBarData {
    timestamp: number;
    values: number[];
}

export interface FilledLineConfig {
    legend?: string[];
    valueRange?: [ number, number ];
    valueFormat?: Readable;
    auxiliaryValue?: number;
    palette: Array<keyof Theme['colorPalette']>;
    seriesMode?: 'stacked' | 'overlay';
};

export interface StackBarConfig {
    radius: number;
    auxiliaryValue?: number;
    yScaleType: ScaleType;
    // number - bar width stand for ms
    // string - '100px' fixed bar width
    barWidth: number | string;
    barTimestampPosition?: 'center' | 'start';
    valueRange?: [ number, number ] ;
    palette?: string[];
    autoScaleHeadroom?: number;
};

export interface StatusConfig {
    rowHeight: UnitHeight;
};

export interface StackStatusConfig {
    rowHeight: UnitHeight;
    textConfig?: TextConfig;
    isNeedClamp?: boolean;
    isCollapse: boolean;
    maxDepth?: number;
};

export type ScaleType = 'linear' | 'symLog';
// #endregion

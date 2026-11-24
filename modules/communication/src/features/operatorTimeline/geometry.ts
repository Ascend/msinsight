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
import type { ECharts } from 'echarts';
import type { CommunicationWebGLIndex, CommunicationWebGLOperator } from './dataIndex';
import { hitTestCommunicationOperator } from './dataIndex';
import { getCommunicationWebGLRankGeometry, type CommunicationWebGLLayout } from './WebGLRenderer';

export const getWebGLLayout = (
    chartDom: HTMLElement,
    chart: ECharts,
    index: CommunicationWebGLIndex,
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
    };
};

export const getWebGLViewport = (
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

export function hitTest(index: CommunicationWebGLIndex, layout: CommunicationWebGLLayout | null, x: number, y: number): CommunicationWebGLOperator | null {
    if (!layout || x < layout.gridLeft || x > layout.gridLeft + layout.gridWidth ||
        y < layout.gridTop || y > layout.gridTop + layout.gridHeight) { return null; }
    const rankIndex = Math.round((y - layout.rankY0) / layout.rankStep);
    const rankCenter = layout.rankY0 + rankIndex * layout.rankStep;
    if (!Number.isFinite(rankIndex) || Math.abs(y - rankCenter) > layout.rowHeight * 0.3) { return null; }
    const time = layout.xMin + (x - layout.gridLeft) / layout.gridWidth * (layout.xMax - layout.xMin);
    return hitTestCommunicationOperator(index, rankIndex, time);
}

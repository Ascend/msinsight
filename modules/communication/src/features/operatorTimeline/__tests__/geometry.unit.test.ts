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

import {
    getCommunicationWebGLOperatorRect,
    getCommunicationWebGLRankGeometry,
    type CommunicationWebGLLayout,
} from '../WebGLRenderer';
import {
    type CommunicationWebGLOperator,
} from '../dataIndex';

const layout: CommunicationWebGLLayout = {
    canvasWidth: 400,
    canvasHeight: 240,
    gridLeft: 100,
    gridTop: 20,
    gridWidth: 200,
    gridHeight: 180,
    xMin: 10,
    xMax: 30,
    rankY0: 50,
    rankStep: 30,
    rowHeight: 30,
};

const createOperator = (
): CommunicationWebGLOperator => ({
    id: 1,
    rankIndex: 1,
    operatorName: 'AllReduce',
    startTime: 12,
    endTime: 16,
    duration: 4,
    original: { operatorName: 'AllReduce', startTime: 12000000, duration: 4000000 },
    colorIndex: 0,
});

it('uses the same rectangle for WebGL rendering and the highlight overlay', () => {
    expect(getCommunicationWebGLOperatorRect(createOperator(), layout)).toEqual({
        x: 120,
        y: 71,
        width: 40,
        height: 18,
        coverage: 1,
    });
});

it('keeps a non-zero rank step when the visible Y axis contains only one rank', () => {
    const geometry = getCommunicationWebGLRankGeometry({
        rankCount: 100,
        visibleExtent: [42, 42],
        axisExtent: [180, 0],
        bandWidth: 180,
        getRankY: () => 110,
    });

    expect(geometry).toEqual({
        rankY0: 7670,
        rankStep: -180,
        rowHeight: 180,
    });
    expect((geometry?.rankY0 ?? 0) + 42 * (geometry?.rankStep ?? 0)).toBe(110);
});

it('derives the rank step from adjacent visible ranks when the Y axis has a range', () => {
    const geometry = getCommunicationWebGLRankGeometry({
        rankCount: 100,
        visibleExtent: [10, 12],
        axisExtent: [180, 0],
        bandWidth: 60,
        getRankY: rankIndex => 200 - (rankIndex - 10) * 60,
    });

    expect(geometry).toEqual({
        rankY0: 800,
        rankStep: -60,
        rowHeight: 60,
    });
});

it('uses coverage alpha for operators smaller than one physical pixel', () => {
    const subpixelLayout = {
        ...layout,
        rowHeight: 0.5,
    };
    const subpixelOperator = {
        ...createOperator(),
        endTime: 12.04,
    };

    const rect = getCommunicationWebGLOperatorRect(subpixelOperator, subpixelLayout, 1);

    expect(rect.width).toBe(1);
    expect(rect.height).toBe(1);
    expect(rect.coverage).toBeCloseTo(0.12);
});

it('calculates subpixel coverage in physical pixels for high-DPI displays', () => {
    const subpixelLayout = {
        ...layout,
        rowHeight: 0.5,
    };
    const subpixelOperator = {
        ...createOperator(),
        endTime: 12.04,
    };

    const rect = getCommunicationWebGLOperatorRect(subpixelOperator, subpixelLayout, 2);

    expect(rect.width).toBe(0.5);
    expect(rect.height).toBe(0.5);
    expect(rect.coverage).toBeCloseTo(0.48);
});

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

interface RenderOptions {
    transform: {
        x: number;
        y: number;
        scale: number;
        scaleX?: number;
        scaleY?: number;
    };
    viewport: {
        width: number;
        height: number;
    };
    zoom: {
        x: number;
        y: number;
        offset: number;
    };
}

interface Block {
    id: number;
    addr: string;
    _startTimestamp: number;
    _endTimestamp: number;
    size: number;
    path: Array<[number, number]>;
}

interface RenderData {
    maxTimestamp: number;
    minTimestamp: number;
    maxSize: number;
    minSize: number;
    blocks: Block[];
}

interface SpatialGrid {
    [key: string]: number[]; // 存储区块索引的数组
}

interface Shader {
    vertexShader: string;
    fragmentShader: string;
}

interface Segment {
    address: string;
    stream: number;
    size: number;
    blocks: StateBlock[];
    offsetX: number;
    offsetY: number;
    allocOrMapEventId: number;
}

interface StateBlock {
    offset: number;
    id: number;
    size: number;
    colorIndex?: number;
}

type Theme = 'light' | 'dark';

interface StateDataHoverResult {
    type: 'segment' | 'block';
    data: Segment;
}

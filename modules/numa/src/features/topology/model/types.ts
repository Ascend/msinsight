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

import type { SocketNode } from '@/entities/numa/types';

export interface Point {
    x: number;
    y: number;
}

export interface ConnectionMetricLabelLayout {
    key: string;
    text: string;
    position: Point;
    rotation: number;
    textAnchor: 'start' | 'middle' | 'end';
    maxLength: number;
    progress: number;
}

export interface NodeBox {
    center: Point;
    width: number;
    height: number;
}

export interface SocketLayout {
    socket: SocketNode;
    column: number;
    x: number;
    y: number;
    width: number;
    height: number;
    center: Point;
    numaPoints: Map<number, Point>;
}

export interface DiagramLayout {
    layouts: SocketLayout[];
    nodeBoxes: Map<string, NodeBox>;
    width: number;
    height: number;
    viewBox: string;
}

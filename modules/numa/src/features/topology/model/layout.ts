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

import type { Connection, SocketNode } from '@/entities/numa/types';
import {
    DEFAULT_SOCKET_COLUMNS,
    MEMORY_HEIGHT,
    MEMORY_WIDTH,
    MIN_DIAGRAM_WIDTH,
    NUMA_HEIGHT,
    NUMA_TOP_OFFSET,
    NUMA_VERTICAL_SPAN,
    NUMA_WIDTH,
    SOCKET_COLUMN_STEP,
    SOCKET_HEIGHT,
    SOCKET_ROW_GAP,
    SOCKET_START_X,
    SOCKET_WIDTH,
} from './constants';
import type { DiagramLayout, NodeBox, Point, SocketLayout } from './types';

function boundaryPoint(box: NodeBox, target: Point): Point {
    const dx = target.x - box.center.x;
    const dy = target.y - box.center.y;
    const scale = Math.min(
        dx === 0 ? Number.POSITIVE_INFINITY : Math.abs((box.width / 2) / dx),
        dy === 0 ? Number.POSITIVE_INFINITY : Math.abs((box.height / 2) / dy),
    );
    return {
        x: box.center.x + dx * scale,
        y: box.center.y + dy * scale,
    };
}

function boxEndpoints(source: NodeBox, target: NodeBox): [Point, Point] {
    return [boundaryPoint(source, target.center), boundaryPoint(target, source.center)];
}

export function normalizeSocketColumns(socketColumns: number | undefined, socketCount: number): number {
    const requestedColumns = Number.isFinite(socketColumns)
        ? Math.max(1, Math.floor(socketColumns as number))
        : DEFAULT_SOCKET_COLUMNS;
    const normalizedSocketCount = Number.isFinite(socketCount)
        ? Math.max(1, Math.floor(socketCount))
        : 1;
    return Math.min(requestedColumns, normalizedSocketCount);
}

export function buildDiagramLayout(sockets: SocketNode[], columns: number): DiagramLayout {
    const normalizedColumns = normalizeSocketColumns(columns, sockets.length);
    const layouts: SocketLayout[] = sockets.map((socket, index) => {
        const column = index % normalizedColumns;
        const row = Math.floor(index / normalizedColumns);
        const x = SOCKET_START_X + column * SOCKET_COLUMN_STEP;
        const y = 50 + row * (SOCKET_HEIGHT + SOCKET_ROW_GAP);
        const numaPoints = new Map<number, Point>();
        const spacing = socket.numas.length > 1 ? NUMA_VERTICAL_SPAN / (socket.numas.length - 1) : 0;
        socket.numas.forEach((numa, numaIndex) => {
            numaPoints.set(numa.id, {
                x: x + SOCKET_WIDTH / 2,
                y: y + NUMA_TOP_OFFSET + spacing * numaIndex,
            });
        });
        return {
            socket,
            column,
            x,
            y,
            width: SOCKET_WIDTH,
            height: SOCKET_HEIGHT,
            center: { x: x + SOCKET_WIDTH / 2, y: y + SOCKET_HEIGHT / 2 },
            numaPoints,
        };
    });

    const nodeBoxes = new Map<string, NodeBox>();
    layouts.forEach((layout) => {
        nodeBoxes.set(`socket-${layout.socket.id}`, {
            center: layout.center,
            width: layout.width,
            height: layout.height,
        });
        const memoryOnLeft = layout.column % 2 === 0;
        layout.socket.numas.forEach((numa) => {
            const point = layout.numaPoints.get(numa.id);
            if (!point) return;
            nodeBoxes.set(`numa-${numa.id}`, {
                center: point,
                width: NUMA_WIDTH,
                height: NUMA_HEIGHT,
            });
            nodeBoxes.set(`memory-${numa.id}`, {
                center: {
                    x: memoryOnLeft ? layout.x - 62 : layout.x + layout.width + 62,
                    y: point.y,
                },
                width: MEMORY_WIDTH,
                height: MEMORY_HEIGHT,
            });
        });
    });

    const rows = Math.max(1, Math.ceil(layouts.length / normalizedColumns));
    const width = Math.max(
        MIN_DIAGRAM_WIDTH,
        SOCKET_START_X * 2 + SOCKET_WIDTH + (normalizedColumns - 1) * SOCKET_COLUMN_STEP,
    );
    const height = rows * (SOCKET_HEIGHT + SOCKET_ROW_GAP) + 20;

    return {
        layouts,
        nodeBoxes,
        width,
        height,
        viewBox: `0 0 ${width} ${height}`,
    };
}

export function connectionEndpoints(connection: Connection, nodeBoxes: Map<string, NodeBox>): [Point, Point] | null {
    const nodeBoxForKey = connection.type === 'socket'
        ? (key: string): NodeBox | undefined => key.startsWith('socket-') ? nodeBoxes.get(key) : undefined
        : (key: string): NodeBox | undefined => key.startsWith('numa-') || key.startsWith('memory-')
            ? nodeBoxes.get(key)
            : undefined;
    const source = nodeBoxForKey(connection.source);
    const target = nodeBoxForKey(connection.target);
    return source && target ? boxEndpoints(source, target) : null;
}

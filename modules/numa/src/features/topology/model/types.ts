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

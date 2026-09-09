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

import { formatMetricValue } from '@/entities/numa/formatters';
import type { Connection, ConnectionType, Metric } from '@/entities/numa/types';
import type { ConnectionMetricLabelLayout, Point } from './types';

export const METRIC_LABEL_OFFSET = 14;

type Segment = [Point, Point];

const METRIC_KEYS: Record<ConnectionType, string[]> = {
    memory: ['dramRead', 'dramWrite'],
    numa: ['crossScclRead'],
    socket: ['sourceToTarget', 'targetToSource'],
};

const EPSILON = 1e-6;

function cross(left: Point, right: Point): number {
    return left.x * right.y - left.y * right.x;
}

function subtract(left: Point, right: Point): Point {
    return { x: left.x - right.x, y: left.y - right.y };
}

function metricValue(metric: Metric | undefined): string {
    return metric === undefined || metric.hasValue === false || !Number.isFinite(metric.value)
        ? '--'
        : formatMetricValue(metric.value, metric.unit);
}

function usesForwardTextOrder(tangent: Point): boolean {
    const rawRotation = Math.atan2(tangent.y, tangent.x) * 180 / Math.PI;
    return rawRotation >= -90 && rawRotation <= 90;
}

function labelRotation(tangent: Point): number {
    if (Math.abs(tangent.x) < 0.15 || Math.abs(tangent.y) < 0.15) return 0;
    const rawRotation = Math.atan2(tangent.y, tangent.x) * 180 / Math.PI;
    if (rawRotation > 90) return rawRotation - 180;
    if (rawRotation < -90) return rawRotation + 180;
    return rawRotation;
}

function labelAnchor(tangent: Point, position: Point, linePoint: Point): ConnectionMetricLabelLayout['textAnchor'] {
    if (Math.abs(tangent.x) >= 0.15) return 'middle';
    return position.x < linePoint.x ? 'end' : 'start';
}

function directionalText(value: string, arrow: string): string {
    return arrow === '→' || arrow === '↓' ? `${value} ${arrow}` : `${arrow} ${value}`;
}

function labelPosition(source: Point, tangent: Point, normal: Point, progress: number, offset: number): Point {
    return {
        x: source.x + tangent.x * progress + normal.x * offset,
        y: source.y + tangent.y * progress + normal.y * offset,
    };
}

function directionArrow(vector: Point): string {
    const angle = Math.atan2(vector.y, vector.x) * 180 / Math.PI;
    if (angle >= -22.5 && angle < 22.5) return '→';
    if (angle >= 22.5 && angle < 67.5) return '↘';
    if (angle >= 67.5 && angle < 112.5) return '↓';
    if (angle >= 112.5 && angle < 157.5) return '↙';
    if (angle >= 157.5 || angle < -157.5) return '←';
    if (angle >= -157.5 && angle < -112.5) return '↖';
    if (angle >= -112.5 && angle < -67.5) return '↑';
    return '↗';
}

export function segmentsCross([firstStart, firstEnd]: Segment, [secondStart, secondEnd]: Segment): boolean {
    const firstVector = subtract(firstEnd, firstStart);
    const secondVector = subtract(secondEnd, secondStart);
    const denominator = cross(firstVector, secondVector);
    if (Math.abs(denominator) < EPSILON) return false;

    const betweenStarts = subtract(secondStart, firstStart);
    const firstProgress = cross(betweenStarts, secondVector) / denominator;
    const secondProgress = cross(betweenStarts, firstVector) / denominator;
    return firstProgress > EPSILON && firstProgress < 1 - EPSILON &&
        secondProgress > EPSILON && secondProgress < 1 - EPSILON;
}

export function buildConnectionMetricLabels(
    connection: Connection,
    source: Point,
    target: Point,
    crossing: boolean,
): ConnectionMetricLabelLayout[] {
    const vector = subtract(target, source);
    const length = Math.hypot(vector.x, vector.y);
    if (length < EPSILON) return [];

    const tangent = { x: vector.x / length, y: vector.y / length };
    const normal = { x: -tangent.y, y: tangent.x };
    const forward = usesForwardTextOrder(tangent);
    const rotation = labelRotation(tangent);
    const metrics = new Map(connection.metrics.map((metric) => [metric.key, metric]));
    const keys = METRIC_KEYS[connection.type];
    const maxLength = length * (crossing ? 0.28 : 0.45);

    if (connection.type === 'numa') {
        const key = keys[0];
        const progress = 0.5;
        const linePoint = labelPosition(source, tangent, normal, length * progress, 0);
        const position = labelPosition(source, tangent, normal, length * progress, -METRIC_LABEL_OFFSET);
        return [{
            key,
            text: metricValue(metrics.get(key)),
            position,
            rotation: 0,
            textAnchor: labelAnchor(tangent, position, linePoint),
            maxLength,
            progress,
        }];
    }

    return keys.map((key, index) => {
        const towardTarget = connection.type === 'memory' ? index === 1 : index === 0;
        const progress = crossing ? towardTarget ? 0.68 : 0.32 : 0.5;
        const direction = towardTarget ? tangent : { x: -tangent.x, y: -tangent.y };
        const arrow = rotation === 0
            ? directionArrow(direction)
            : towardTarget === forward ? '→' : '←';
        const linePoint = labelPosition(source, tangent, normal, length * progress, 0);
        const position = labelPosition(
            source,
            tangent,
            normal,
            length * progress,
            index === 0 ? METRIC_LABEL_OFFSET : -METRIC_LABEL_OFFSET,
        );
        return {
            key,
            text: directionalText(metricValue(metrics.get(key)), arrow),
            position,
            rotation,
            textAnchor: labelAnchor(tangent, position, linePoint),
            maxLength,
            progress,
        };
    });
}

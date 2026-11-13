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
export const MIN_FRAME_SIZE = 6;
export const ICICLE_CELL_GAP = 0.8;

export function allocateMinimumSizes(parts: Array<{ value: number; minimum: number }>, extent: number): number[] {
    const sizes = parts.map(() => 0);
    const minimum = parts.reduce((sum, part) => sum + part.minimum, 0);
    let available = Math.max(extent, minimum);
    let weight = parts.reduce((sum, part) => sum + part.value, 0);
    if (weight === 0) return parts.map(part => part.minimum + (available - minimum) / parts.length);
    // Water filling fixes undersized parts first, then divides the remainder proportionally.
    const order = parts.map((_, index) => index).sort((a, b) => parts[a].value / parts[a].minimum - parts[b].value / parts[b].minimum);
    let fixed = 0;
    while (fixed < order.length) {
        const index = order[fixed];
        const part = parts[index];
        if (weight > 0 && available * part.value / weight >= part.minimum) break;
        sizes[index] = part.minimum;
        available -= part.minimum;
        weight -= part.value;
        fixed++;
    }
    for (let cursor = fixed; cursor < order.length; cursor++) {
        const index = order[cursor];
        sizes[index] = Math.max(parts[index].minimum, weight > 0 ? available * parts[index].value / weight : 0);
    }
    return sizes;
}

export interface MinimumRectangle {
    value: number;
    minWidth: number;
    minHeight: number;
}
export interface RectanglePlan extends MinimumRectangle {
    index?: number;
    horizontal: boolean;
    children?: [RectanglePlan, RectanglePlan];
}

export function planMinimumRectangles(parts: MinimumRectangle[], width: number, height: number): RectanglePlan {
    const sums = [0];
    for (const part of parts) sums.push(sums[sums.length - 1] + part.value);
    const make = (start: number, end: number): RectanglePlan => ({
        value: Math.max(0, sums[end] - sums[start]), minWidth: MIN_FRAME_SIZE, minHeight: MIN_FRAME_SIZE, horizontal: true,
    });
    const root = make(0, parts.length);
    const pending = [{ node: root, start: 0, end: parts.length, width, height }];
    const visited: RectanglePlan[] = [];
    while (pending.length) {
        const entry = pending.pop();
        if (!entry) break;
        const { node, start, end } = entry;
        visited.push(node);
        if (end - start === 1) {
            Object.assign(node, parts[start], { index: start });
            continue;
        }
        let split = Math.floor((start + end) / 2);
        if (node.value > 0) {
            const half = sums[start] + node.value / 2;
            let low = start + 1;
            let high = end - 1;
            while (low < high) {
                const middle = Math.floor((low + high) / 2);
                if (sums[middle] < half) low = middle + 1;
                else high = middle;
            }
            split = low > start + 1 && half - sums[low - 1] < sums[low] - half ? low - 1 : low;
        }
        node.horizontal = entry.width >= entry.height;
        const first = make(start, split);
        const second = make(split, end);
        node.children = [first, second];
        const ratio = node.value > 0 ? first.value / node.value : (split - start) / (end - start);
        for (const [child, from, to, fraction] of [
            [first, start, split, ratio], [second, split, end, 1 - ratio],
        ] as const) {
            pending.push({
                node: child,
                start: from,
                end: to,
                width: node.horizontal ? entry.width * fraction : entry.width,
                height: node.horizontal ? entry.height : entry.height * fraction,
            });
        }
    }
    for (let index = visited.length - 1; index >= 0; index--) {
        const node = visited[index];
        if (!node.children) continue;
        const [first, second] = node.children;
        node.minWidth = node.horizontal ? first.minWidth + second.minWidth : Math.max(first.minWidth, second.minWidth);
        node.minHeight = node.horizontal ? Math.max(first.minHeight, second.minHeight) : first.minHeight + second.minHeight;
    }
    return root;
}

export function placeMinimumRectangles(plan: RectanglePlan, width: number, height: number): Array<{
    index: number; x: number; y: number; width: number; height: number;
}> {
    const placed = [];
    const pending = [{ node: plan, x: 0, y: 0, width, height }];
    while (pending.length) {
        const entry = pending.pop();
        if (!entry) break;
        const { node, x, y } = entry;
        if (!node.children) {
            if (node.index !== undefined) placed.push({ index: node.index, x, y, width: entry.width, height: entry.height });
            continue;
        }
        const [first, second] = node.children;
        const extent = node.horizontal ? entry.width : entry.height;
        const sizes = allocateMinimumSizes(node.children.map(child => ({
            value: child.value, minimum: node.horizontal ? child.minWidth : child.minHeight,
        })), extent);
        pending.push({
            node: second,
            x: x + (node.horizontal ? sizes[0] : 0),
            y: y + (node.horizontal ? 0 : sizes[0]),
            width: node.horizontal ? sizes[1] : entry.width,
            height: node.horizontal ? entry.height : sizes[1],
        });
        pending.push({
            node: first, x, y, width: node.horizontal ? sizes[0] : entry.width, height: node.horizontal ? entry.height : sizes[0],
        });
    }
    return placed;
}

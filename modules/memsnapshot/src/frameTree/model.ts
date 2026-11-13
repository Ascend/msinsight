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
import { METRIC_KEYS } from './types';
import * as d3 from 'd3';
import { MIN_FRAME_SIZE, placeMinimumRectangles, planMinimumRectangles } from './minimumSize';
import type { RectanglePlan } from './minimumSize';
import type { FrameTree, FrameTreeCell, FrameTreeNode, FrameTreeNodeDTO, NodeKind } from './types';

const KINDS: Record<NodeKind, string> = {
    root: 'All memory 全部内存',
    frame: '',
    static: 'Static memory 静态内存',
    preexisting: 'Preexisting memory 预存内存',
    emptyStack: 'Empty stack 空调用栈',
    missingFrame: 'Missing frames 缺失调用帧',
};
const safeNatural = (value: unknown): value is number => Number.isSafeInteger(value) && (value as number) >= 0;
const compareIds = (a: string, b: string): number => a < b ? -1 : a > b ? 1 : 0;
const invalid = (id = ''): never => { throw new Error(`Invalid or incomplete memory frame tree${id ? `: ${id}` : ''}`); };

export const nodeAt = (tree: FrameTree, id: string): FrameTreeNode => tree.nodes.get(id) ?? invalid(id);
export const childIds = (tree: FrameTree, id: string): string[] => tree.children.get(id) ?? invalid(id);

export const compareNodes = (tree: FrameTree, a: string, b: string): number => {
    const left = nodeAt(tree, a).metrics.sizeBytes;
    const right = nodeAt(tree, b).metrics.sizeBytes;
    return left === right ? compareIds(a, b) : left > right ? -1 : 1;
};

export function validateFrameTree(input: unknown, total: number): FrameTree {
    if (!safeNatural(total) || total < 1 || !Array.isArray(input) || input.length !== total) invalid();
    const tree: FrameTree = { rootId: '', nodes: new Map(), children: new Map() };
    let roots = 0;
    for (const raw of input as FrameTreeNodeDTO[]) {
        if (raw === null || typeof raw !== 'object' || typeof raw.nodeId !== 'string' || raw.nodeId === '' ||
            tree.nodes.has(raw.nodeId) || typeof raw.kind !== 'string' || !Object.prototype.hasOwnProperty.call(KINDS, raw.kind) ||
            !safeNatural(raw.depth) || !safeNatural(raw.childCount) ||
            (raw.parentId !== null && typeof raw.parentId !== 'string')) invalid();
        for (const key of METRIC_KEYS) {
            if (typeof raw[key] !== 'string' || !/^(0|[1-9]\d*)$/.test(raw[key])) invalid(raw.nodeId);
        }
        if (raw.kind === 'frame') {
            if (!raw.frame || typeof raw.frame.filename !== 'string' || typeof raw.frame.name !== 'string' ||
                (raw.frame.line !== null && !Number.isSafeInteger(raw.frame.line))) invalid(raw.nodeId);
        } else if (raw.frame !== null || (raw.kind !== 'root' && raw.childCount !== 0)) invalid(raw.nodeId);
        const metrics = Object.fromEntries(METRIC_KEYS.map(key => [key, BigInt(raw[key])])) as FrameTreeNode['metrics'];
        const node = {
            dto: raw,
            metrics,
            searchText: [
                raw.nodeId, KINDS[raw.kind], raw.frame?.name, raw.frame?.filename, raw.frame?.line,
            ].filter(value => value !== null && value !== undefined).join(' ').toLowerCase(),
        };
        tree.nodes.set(raw.nodeId, node);
        tree.children.set(raw.nodeId, []);
        if (raw.kind === 'root') {
            roots++;
            tree.rootId = raw.nodeId;
            if (raw.parentId !== null || raw.depth !== 0 || metrics.selfBytes !== 0n ||
                metrics.selfRequestedBytes !== 0n || metrics.selfBlockCount !== 0n) invalid(raw.nodeId);
        } else if (raw.parentId === null || raw.depth === 0) invalid(raw.nodeId);
    }
    if (roots !== 1) invalid();
    for (const [id, { dto }] of tree.nodes) {
        if (id === tree.rootId) continue;
        const parentId = dto.parentId ?? invalid(id);
        const parent = tree.nodes.get(parentId);
        if (!parent || parent.dto.depth + 1 !== dto.depth) invalid(id);
        childIds(tree, parentId).push(id);
    }
    const visited = new Set<string>();
    const pending = [tree.rootId];
    while (pending.length) {
        const id = pending.pop() ?? invalid();
        if (visited.has(id)) invalid(id);
        visited.add(id);
        const node = nodeAt(tree, id);
        const children = childIds(tree, id);
        if (children.length !== node.dto.childCount) invalid(id);
        const sums = [node.metrics.selfBytes, node.metrics.selfRequestedBytes, node.metrics.selfBlockCount];
        for (const child of children) {
            const metrics = nodeAt(tree, child).metrics;
            sums[0] += metrics.sizeBytes;
            sums[1] += metrics.requestedBytes;
            sums[2] += metrics.blockCount;
            pending.push(child);
        }
        if (sums[0] !== node.metrics.sizeBytes || sums[1] !== node.metrics.requestedBytes ||
            sums[2] !== node.metrics.blockCount) invalid(id);
        // Keep static memory at the end of each sibling group without changing metric-based search ranking.
        children.sort((a, b) => Number(nodeAt(tree, a).dto.kind === 'static') - Number(nodeAt(tree, b).dto.kind === 'static') ||
            compareNodes(tree, a, b));
    }
    if (visited.size !== total) invalid();
    return tree;
}

export function groupUnknownFrames(source: FrameTree, includeUnknown: boolean): FrameTree {
    const view: FrameTree = { rootId: source.rootId, nodes: new Map(), children: new Map() };
    const unknown = Object.fromEntries(METRIC_KEYS.map(key => [key, 0n])) as FrameTreeNode['metrics'];
    let hasUnknown = false;
    for (const [id, node] of source.nodes) {
        if (node.dto.kind === 'root' || node.dto.kind === 'frame') {
            view.nodes.set(id, { ...node, dto: { ...node.dto }, metrics: { ...node.metrics } });
        } else {
            hasUnknown = true;
            for (const key of METRIC_KEYS) unknown[key] += node.metrics[key];
        }
    }
    for (const id of view.nodes.keys()) view.children.set(id, childIds(source, id).filter(child => view.nodes.has(child)));
    const ordered = [view.rootId];
    for (let index = 0; index < ordered.length; index++) {
        for (const child of childIds(view, ordered[index])) ordered.push(child);
    }
    // Recompute caller totals without unknown descendants before moving that memory to the root bucket.
    for (let index = ordered.length - 1; index >= 0; index--) {
        const node = nodeAt(view, ordered[index]);
        for (const [total, self] of [['sizeBytes', 'selfBytes'], ['requestedBytes', 'selfRequestedBytes'], ['blockCount', 'selfBlockCount']] as const) {
            node.metrics[total] = childIds(view, ordered[index])
                .reduce((sum, child) => sum + nodeAt(view, child).metrics[total], node.metrics[self]);
        }
    }
    if (includeUnknown && hasUnknown) {
        let id = 'unknown';
        while (source.nodes.has(id)) id = `_${id}`;
        const root = nodeAt(view, view.rootId);
        view.nodes.set(id, {
            dto: { ...root.dto, nodeId: id, parentId: view.rootId, depth: 1, kind: 'unknown', frame: null, childCount: 0 },
            metrics: unknown,
            searchText: 'unknown \u672a\u77e5',
        });
        view.children.set(id, []);
        childIds(view, view.rootId).push(id);
        for (const key of ['sizeBytes', 'requestedBytes', 'blockCount'] as const) root.metrics[key] += unknown[key];
    }
    for (const [id, node] of view.nodes) {
        const children = childIds(view, id);
        children.sort((a, b) => Number(nodeAt(view, a).dto.kind === 'unknown') - Number(nodeAt(view, b).dto.kind === 'unknown') ||
            compareNodes(view, a, b));
        node.dto.childCount = children.length;
        for (const key of METRIC_KEYS) node.dto[key] = node.metrics[key].toString();
    }
    return view;
}

export function pathToNode(tree: FrameTree, id: string): string[] {
    const path: string[] = [];
    let current: string | null = id;
    while (current !== null) {
        const node: FrameTreeNode | undefined = tree.nodes.get(current);
        if (!node) return [];
        path.push(current);
        current = node.dto.parentId;
    }
    return path.reverse();
}

export function searchFrameTree(tree: FrameTree, query: string): string[] {
    const text = query.trim().toLowerCase();
    if (!text) return [];
    return [...tree.nodes].filter(([, node]) => node.searchText.includes(text))
        .map(([id]) => id).sort((a, b) => compareNodes(tree, a, b));
}

export const exactRatio = (numerator: bigint, denominator: bigint): number | null =>
    denominator === 0n ? null : Number(numerator * 1000000000n / denominator) / 1000000000;

interface TreemapDatum {
    nodeId: string;
    self: boolean;
    value: number;
    children: TreemapDatum[];
}

export function layoutTreemap(tree: FrameTree, focus: string, limit: number, size: { width: number; height: number }): {
    width: number; height: number; cells: FrameTreeCell[];
} {
    const total = nodeAt(tree, focus).metrics.sizeBytes;
    if (total === 0n || !Number.isFinite(size.width) || !Number.isFinite(size.height) || size.width <= 0 || size.height <= 0) {
        return { ...size, cells: [] };
    }
    const data: TreemapDatum = { nodeId: focus, self: false, value: 0, children: [] };
    const pending = [{ datum: data, level: 0 }];
    while (pending.length) {
        const next = pending.pop();
        if (!next) break;
        const { datum, level } = next;
        const node = nodeAt(tree, datum.nodeId);
        const children = childIds(tree, datum.nodeId);
        if (level === limit || children.length === 0) {
            datum.value = Number(node.metrics.sizeBytes) / Number(total);
            continue;
        }
        for (const id of children) {
            if (nodeAt(tree, id).metrics.sizeBytes === 0n) continue;
            const child: TreemapDatum = { nodeId: id, self: false, value: 0, children: [] };
            datum.children.push(child);
            pending.push({ datum: child, level: level + 1 });
        }
        if (node.metrics.selfBytes > 0n) {
            const self = {
                nodeId: datum.nodeId, self: true, value: Number(node.metrics.selfBytes) / Number(total), children: [],
            };
            const tailIndex = datum.children.findIndex(child => ['static', 'unknown'].includes(nodeAt(tree, child.nodeId).dto.kind));
            datum.children.splice(tailIndex < 0 ? datum.children.length : tailIndex, 0, self);
        }
    }
    // Geometry uses relative numbers; the business metrics remain exact BigInts.
    const root = d3.hierarchy(data, datum => datum.children).sum(datum => datum.value);
    const baseline = d3.treemap<TreemapDatum>().size([size.width, size.height]).paddingInner(0)
        .paddingTop(node => (node.children?.length ?? 0) > 1 ? Math.min(18, (node.y1 - node.y0) * 0.12) : 0)(root);
    type Node = d3.HierarchyRectangularNode<TreemapDatum>;
    const plans = new Map<Node, { width: number; height: number; children?: RectanglePlan }>();
    baseline.eachAfter(node => {
        if (!node.children?.length) {
            plans.set(node, { width: MIN_FRAME_SIZE, height: MIN_FRAME_SIZE });
            return;
        }
        const header = node.children.length > 1 ? 18 : 0;
        const plan = planMinimumRectangles(node.children.map(child => ({
            value: child.value ?? 0,
            minWidth: plans.get(child)?.width ?? MIN_FRAME_SIZE,
            minHeight: plans.get(child)?.height ?? MIN_FRAME_SIZE,
        })), Math.max(MIN_FRAME_SIZE, node.x1 - node.x0), Math.max(MIN_FRAME_SIZE, node.y1 - node.y0 - header));
        plans.set(node, { width: plan.minWidth, height: header + plan.minHeight, children: plan });
    });
    const width = Math.max(size.width, plans.get(baseline)?.width ?? MIN_FRAME_SIZE);
    const height = Math.max(size.height, plans.get(baseline)?.height ?? MIN_FRAME_SIZE);
    const cells: FrameTreeCell[] = [];
    const rectangles = [{ node: baseline, x: 0, y: 0, width, height }];
    while (rectangles.length) {
        const rectangle = rectangles.pop();
        if (!rectangle) break;
        const { node, x, y } = rectangle;
        const businessNode = nodeAt(tree, node.data.nodeId);
        cells.push({
            nodeId: node.data.nodeId,
            x: x / width,
            y: y / height,
            width: rectangle.width / width,
            height: rectangle.height / height,
            level: node.depth,
            self: node.data.self,
            truncated: !node.data.self && node.depth === limit && businessNode.dto.childCount > 0,
        });
        const plan = plans.get(node)?.children;
        if (!plan || !node.children) continue;
        const header = node.children.length > 1 ? 18 : 0;
        const placed = placeMinimumRectangles(plan, rectangle.width, rectangle.height - header);
        for (let index = placed.length - 1; index >= 0; index--) {
            const child = placed[index];
            rectangles.push({
                node: node.children[child.index],
                x: x + child.x,
                y: y + header + child.y,
                width: child.width,
                height: child.height,
            });
        }
    }
    return { width, height, cells };
}

export function formatExactBytes(value: bigint): string {
    const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB'];
    let unit = 0;
    let scale = 1n;
    while (unit < units.length - 1 && value >= scale * 1024n) { scale *= 1024n; unit++; }
    if (unit === 0) return `${value.toString()} B`;
    const hundredths = value * 100n / scale;
    return `${hundredths / 100n}.${(hundredths % 100n).toString().padStart(2, '0')} ${units[unit]}`;
}

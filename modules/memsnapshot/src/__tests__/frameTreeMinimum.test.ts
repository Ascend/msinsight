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
import { layoutTreemap, validateFrameTree } from '../frameTree/model';
import { layoutReferenceIcicle } from '../frameTree/referenceIcicle';
import type { FrameTreeNodeDTO } from '../frameTree/types';
import { allocateMinimumSizes } from '../frameTree/minimumSize';
import { frameTreeLeaf } from './fixtures/frameTreeMock';

it('fills a minimum-size partition even if tiny relative weights round to zero', () => {
    expect(allocateMinimumSizes([{ value: 0, minimum: 6 }, { value: 0, minimum: 6 }], 100)).toEqual([50, 50]);
    expect(allocateMinimumSizes([{ value: 0, minimum: 6 }, { value: 1, minimum: 6 }], 100)).toEqual([6, 94]);
    expect(allocateMinimumSizes([{ value: 1, minimum: 6 }, { value: 1, minimum: 6 }], 8)).toEqual([6, 6]);
});

const leaf = (id: string, bytes: bigint, parent = 'root', depth = 1): FrameTreeNodeDTO =>
    frameTreeLeaf(id, parent, bytes, depth);
const fixture = (count: number): FrameTreeNodeDTO[] => [
    {
        ...leaf('root', 10000000000000000n + BigInt(count)),
        parentId: null,
        depth: 0,
        kind: 'root',
        frame: null,
        childCount: count + 1,
        selfBytes: '0',
        selfRequestedBytes: '0',
        selfBlockCount: '0',
        blockCount: String(count + 1),
    },
    leaf('large', 10000000000000000n),
    ...Array.from({ length: count }, (_, index) => leaf(`tiny-${index}`, 1n)),
];

it.each([1, 10, 200])('makes all %s tiny icicle allocations visible without overlapping their siblings', count => {
    const nodes = fixture(count);
    const tree = validateFrameTree(nodes, nodes.length);
    const layout = layoutReferenceIcicle(tree, 'root', 560);
    const children = layout.cells.filter(cell => cell.row === 1).sort((a, b) => a.x - b.x);
    expect(children).toHaveLength(count + 1);
    for (const [index, cell] of children.entries()) {
        expect(cell.width - 0.8).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(cell.x + cell.width).toBeLessThanOrEqual(layout.width + 1e-8);
        if (index) expect(cell.x).toBeGreaterThanOrEqual(children[index - 1].x + children[index - 1].width - 1e-8);
    }
    expect(tree.nodes.get('tiny-0')?.metrics.sizeBytes).toBe(1n);
});

it('keeps the entire tiny call chain instead of stopping at its first subpixel ancestor', () => {
    const nodes = fixture(1);
    Object.assign(nodes[2], { selfBytes: '0', selfRequestedBytes: '0', selfBlockCount: '0', childCount: 1 });
    for (let depth = 2; depth <= 1200; depth++) {
        nodes.push({
            ...leaf(`deep-${depth}`, 1n, depth === 2 ? 'tiny-0' : `deep-${depth - 1}`, depth),
            ...(depth < 1200 ? { selfBytes: '0', selfRequestedBytes: '0', selfBlockCount: '0', childCount: 1 } : {}),
        });
    }
    const tree = validateFrameTree(nodes, nodes.length);
    const layout = layoutReferenceIcicle(tree, 'root', 560);
    expect(layout.cells).toHaveLength(1202);
    expect(layout.cells.find(cell => cell.nodeId === 'deep-1200')?.width).toBeGreaterThanOrEqual(6.8 - 1e-8);
});

it.each([1, 10, 200])('gives all %s tiny treemap allocations at least 6x6px without overlaps or missing bytes', count => {
    const nodes = fixture(count);
    const tree = validateFrameTree(nodes, nodes.length);
    const layout = layoutTreemap(tree, 'root', Infinity, { width: 120, height: 80 });
    const children = layout.cells.filter(cell => cell.level === 1);
    expect(children).toHaveLength(count + 1);
    for (const cell of children) {
        expect(cell.width * layout.width).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(cell.height * layout.height).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(cell.x).toBeGreaterThanOrEqual(0);
        expect(cell.y).toBeGreaterThanOrEqual(0);
        expect(cell.x + cell.width).toBeLessThanOrEqual(1 + 1e-8);
        expect(cell.y + cell.height).toBeLessThanOrEqual(1 + 1e-8);
    }
    for (let a = 0; a < children.length; a++) {
        for (let b = a + 1; b < children.length; b++) {
            const left = children[a];
            const right = children[b];
            const overlapWidth = Math.min(left.x + left.width, right.x + right.width) - Math.max(left.x, right.x);
            const overlapHeight = Math.min(left.y + left.height, right.y + right.height) - Math.max(left.y, right.y);
            expect(overlapWidth <= 1e-8 || overlapHeight <= 1e-8).toBe(true);
        }
    }
    expect(tree.nodes.get('root')?.metrics.sizeBytes).toBe(10000000000000000n + BigInt(count));
});

it('reserves room for nested tiny branches and self allocations within their parent rectangles', () => {
    const nodes = fixture(1);
    Object.assign(nodes[2], {
        sizeBytes: '3', requestedBytes: '3', blockCount: '3', childCount: 2,
    });
    nodes.push(leaf('a', 1n, 'tiny-0', 2), leaf('b', 1n, 'tiny-0', 2));
    Object.assign(nodes[0], { sizeBytes: '10000000000000003', requestedBytes: '10000000000000003', blockCount: '4' });
    const tree = validateFrameTree(nodes, nodes.length);
    const layout = layoutTreemap(tree, 'root', Infinity, { width: 120, height: 80 });
    const parent = layout.cells.find(cell => cell.nodeId === 'tiny-0' && !cell.self);
    expect(parent).toBeDefined();
    const children = layout.cells.filter(cell => cell.level === 2);
    expect(children).toHaveLength(3);
    for (const cell of children) {
        expect(cell.width * layout.width).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(cell.height * layout.height).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(cell.x).toBeGreaterThanOrEqual(parent?.x ?? 0);
        expect(cell.y).toBeGreaterThanOrEqual(parent?.y ?? 0);
        expect(cell.x + cell.width).toBeLessThanOrEqual((parent?.x ?? 0) + (parent?.width ?? 0) + 1e-8);
        expect(cell.y + cell.height).toBeLessThanOrEqual((parent?.y ?? 0) + (parent?.height ?? 0) + 1e-8);
    }
});

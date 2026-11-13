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
import { ICICLE_STYLE, fitIcicleLabel, icicleColors, layoutReferenceIcicle, icicleTicks } from '../frameTree/referenceIcicle';
import { layoutTreemap, searchFrameTree, validateFrameTree } from '../frameTree/model';
import { mergeSingleChildChains } from '../frameTree/mergeChains';
import type { FrameTreeNodeDTO } from '../frameTree/types';
import { frameTreeLeaf } from './fixtures/frameTreeMock';

const leaf = (id: string, parent: string | null, bytes: string, depth = 1): FrameTreeNodeDTO => ({
    ...frameTreeLeaf(id, parent, bytes, depth),
    frame: { filename: 'f.py', line: 1, name: id },
});
const fixture = (): FrameTreeNodeDTO[] => [{
    ...leaf('r', null, '100', 0),
    kind: 'root',
    frame: null,
    childCount: 2,
    selfBytes: '0',
    selfRequestedBytes: '0',
    selfBlockCount: '0',
    blockCount: '2',
}, leaf('a', 'r', '70'), leaf('b', 'r', '30')];

it.each([
    ['70', '20', '10'],
    ['10', '60', '30'],
])('keeps static memory of %s bytes on the right regardless of its size', (staticBytes, frameBytes, emptyBytes) => {
    const nodes: FrameTreeNodeDTO[] = [
        { ...fixture()[0], childCount: 3, blockCount: '3' },
        { ...leaf('static', 'r', staticBytes), kind: 'static', frame: null },
        { ...leaf('empty', 'r', emptyBytes), kind: 'emptyStack', frame: null },
        leaf('call', 'r', frameBytes),
    ];
    const tree = validateFrameTree(nodes, nodes.length);
    expect(tree.children.get('r')).toEqual(['call', 'empty', 'static']);
    expect(searchFrameTree(tree, 'a')).toEqual(staticBytes === '70' ? ['r', 'static', 'call', 'empty'] : ['r', 'call', 'empty', 'static']);
    for (const viewTree of [tree, mergeSingleChildChains(tree).tree]) {
        const icicle = layoutReferenceIcicle(viewTree, 'r', 1000);
        const staticCell = icicle.cells.find(cell => cell.nodeId === 'static');
        expect(staticCell?.x).toBeCloseTo(42 + 958 * (1 - Number(staticBytes) / 100));
        expect(staticCell?.width).toBeCloseTo(958 * Number(staticBytes) / 100);
        const treemap = layoutTreemap(viewTree, 'r', Infinity, { width: 1000, height: 360 }).cells;
        const staticRect = treemap.find(cell => cell.nodeId === 'static');
        expect((staticRect?.x ?? 0) + (staticRect?.width ?? 0)).toBeCloseTo(1);
        expect(tree.nodes.get('static')?.metrics.sizeBytes).toBe(BigInt(staticBytes));
    }
    expect(nodes[1].nodeId).toBe('static');
});

it('retains full-stack geometry while taking its palette from the host', () => {
    const tree = validateFrameTree(fixture(), 3);
    const layout = layoutReferenceIcicle(tree, 'r', 1000);
    expect(ICICLE_STYLE).toMatchObject({
        rowHeight: 21,
        gutter: 42,
        top: 26,
        bottom: 24,
    });
    expect(layout.height).toBe(92);
    expect(layout.cells[0]).toMatchObject({ nodeId: 'r', x: 42, y: 26, width: 958, height: 19 });
    expect(layout.cells[1]).toMatchObject({ nodeId: 'a', x: 42, y: 47, height: 19 });
    expect(layout.cells[1].width).toBeCloseTo(670.6);
    expect(layout.cells[2].x).toBeCloseTo(712.6);
    const colors = icicleColors(tree, { root: '#2a2f37', branches: ['#317af7', '#5aada0'], synthetic: '#343a43' });
    expect(colors.get('r')).toBe('#2a2f37');
    expect(colors.get('a')).toBe('#317af7');
    expect(colors.get('b')).toBe('#5aada0');
});

it('keeps every actual depth and lays out a 1200-frame chain iteratively', () => {
    const nodes = Array.from({ length: 1201 }, (_, index) => ({
        ...leaf(`n${index}`, index === 0 ? null : `n${index - 1}`, '1', index),
        selfBytes: index === 1200 ? '1' : '0',
        selfRequestedBytes: index === 1200 ? '1' : '0',
        selfBlockCount: index === 1200 ? '1' : '0',
        childCount: index === 1200 ? 0 : 1,
    }));
    Object.assign(nodes[0], { kind: 'root', frame: null });
    const tree = validateFrameTree(nodes, nodes.length);
    const full = layoutReferenceIcicle(tree, 'n0', 1000);
    expect(full.cells).toHaveLength(1201);
    expect(full.cells[1200].y).toBe(25226);
    const focused = layoutReferenceIcicle(tree, 'n1199', 1000);
    expect(focused.cells).toHaveLength(2);
    expect(focused.cells[0].depth).toBe(1199);
    expect(focused.cells[1].depth).toBe(1200);
});

it('accounts for a non-leaf self allocation without fabricating a DTO node', () => {
    const nodes = fixture();
    Object.assign(nodes[1], { sizeBytes: '100', requestedBytes: '100', selfBytes: '70', selfRequestedBytes: '70', blockCount: '2', childCount: 1 });
    Object.assign(nodes[2], { parentId: 'a', depth: 2 });
    nodes[0].childCount = 1;
    const tree = validateFrameTree(nodes, 3);
    const layout = layoutReferenceIcicle(tree, 'r', 1000);
    const self = layout.cells.find(cell => cell.self);
    expect(self?.nodeId).toBe('a');
    expect(self?.width).toBeCloseTo(670.6);
    expect(tree.nodes.size).toBe(3);
});

it('expands a tiny call chain to the minimum width and retains every positive descendant', () => {
    const nodes = fixture();
    Object.assign(nodes[0], { sizeBytes: '1000000000001', requestedBytes: '1000000000001' });
    Object.assign(nodes[1], {
        sizeBytes: '1000000000000',
        selfBytes: '1000000000000',
        requestedBytes: '1000000000000',
        selfRequestedBytes: '1000000000000',
    });
    Object.assign(nodes[2], { sizeBytes: '1', requestedBytes: '1', selfBytes: '0', selfRequestedBytes: '0', selfBlockCount: '0', childCount: 1 });
    for (let depth = 2; depth <= 150; depth++) {
        nodes.push({
            ...leaf(`tiny-${depth}`, depth === 2 ? 'b' : `tiny-${depth - 1}`, '1', depth),
            selfBytes: depth === 150 ? '1' : '0',
            selfRequestedBytes: depth === 150 ? '1' : '0',
            selfBlockCount: depth === 150 ? '1' : '0',
            childCount: depth === 150 ? 0 : 1,
        });
    }
    const tree = validateFrameTree(nodes, nodes.length);
    const layout = layoutReferenceIcicle(tree, 'r', 1000);
    expect(layout.levels).toBe(151);
    expect(layout.cells).toHaveLength(152);
    expect(layout.cells.find(cell => cell.nodeId === 'tiny-150')?.width).toBeGreaterThanOrEqual(6.8 - 1e-8);
    expect(tree.nodes.has('tiny-150')).toBe(true);
    expect(layoutReferenceIcicle(tree, 'b', 1000).levels).toBe(150);
});

it('uses the prototype ellipsis fitting and integer-derived memory axis', () => {
    expect(fitIcicleLabel('short', 10)).toBe('short');
    expect(fitIcicleLabel('abcdefghij', 4)).toBe('abcde\u2026');
    const ticks = icicleTicks(28587332812n);
    expect(ticks.map(tick => tick.label)).toEqual(['0 GiB', '5 GiB', '10 GiB', '15 GiB', '20 GiB', '25 GiB']);
    expect(ticks[0].ratio).toBe(0);
    expect(ticks[ticks.length - 1].ratio).toBeLessThan(1);
});

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
import { childIds, groupUnknownFrames, layoutTreemap, nodeAt, searchFrameTree, validateFrameTree } from '../frameTree/model';
import { filterPythonFrames } from '../frameTree/filterPython';
import { layoutReferenceIcicle } from '../frameTree/referenceIcicle';
import { mergeSingleChildChains } from '../frameTree/mergeChains';
import { METRIC_KEYS } from '../frameTree/types';
import type { FrameTree, FrameTreeNodeDTO, NodeKind } from '../frameTree/types';

const node = (id: string, parent: string | null, depth: number, kind: NodeKind,
    size: number, self: number, requested: number, selfRequested: number, blocks: number, selfBlocks: number,
    children = 0): FrameTreeNodeDTO => ({
    nodeId: id,
    parentId: parent,
    depth,
    kind,
    frame: kind === 'frame' ? { name: id, filename: `${id}.py`, line: 1 } : null,
    childCount: children,
    sizeBytes: String(size),
    selfBytes: String(self),
    requestedBytes: String(requested),
    selfRequestedBytes: String(selfRequested),
    blockCount: String(blocks),
    selfBlockCount: String(selfBlocks),
});
const fixture = (): FrameTreeNodeDTO[] => [
    node('root', null, 0, 'root', 100, 0, 80, 0, 8, 0, 4),
    node('a', 'root', 1, 'frame', 60, 30, 50, 25, 4, 2, 2),
    node('b', 'a', 2, 'frame', 20, 20, 15, 15, 1, 1),
    node('missing', 'a', 2, 'missingFrame', 10, 10, 10, 10, 1, 1),
    node('static', 'root', 1, 'static', 15, 15, 10, 10, 1, 1),
    node('preexisting', 'root', 1, 'preexisting', 10, 10, 5, 5, 1, 1),
    node('empty', 'root', 1, 'emptyStack', 15, 15, 15, 15, 2, 2),
];
const assertConserving = (tree: FrameTree): void => {
    for (const [id, current] of tree.nodes) {
        for (const [total, self] of [['sizeBytes', 'selfBytes'], ['requestedBytes', 'selfRequestedBytes'], ['blockCount', 'selfBlockCount']] as const) {
            expect(childIds(tree, id).reduce((sum, child) => sum + nodeAt(tree, child).metrics[total], current.metrics[self]))
                .toBe(current.metrics[total]);
        }
        for (const key of METRIC_KEYS) expect(current.dto[key]).toBe(current.metrics[key].toString());
        expect(current.dto.childCount).toBe(childIds(tree, id).length);
    }
};

it('groups every non-frame source into one unknown root bucket without double counting or mutating API data', () => {
    const nodes = fixture();
    const source = validateFrameTree(nodes, nodes.length);
    const before = JSON.stringify(nodes);
    const tree = groupUnknownFrames(source, true);
    const unknown = [...tree.nodes.values()].filter(current => current.dto.kind === 'unknown');
    expect(unknown).toHaveLength(1);
    expect(unknown[0].dto).toMatchObject({ parentId: 'root', depth: 1, frame: null, childCount: 0 });
    expect(unknown[0].metrics).toEqual({
        sizeBytes: 50n, selfBytes: 50n, requestedBytes: 40n, selfRequestedBytes: 40n, blockCount: 5n, selfBlockCount: 5n,
    });
    expect(nodeAt(tree, 'root').metrics).toEqual(nodeAt(source, 'root').metrics);
    expect(nodeAt(tree, 'a').metrics.sizeBytes).toBe(50n);
    expect(nodeAt(tree, 'b').metrics.sizeBytes).toBe(20n);
    expect(childIds(tree, 'root').at(-1)).toBe(unknown[0].dto.nodeId);
    expect(tree.nodes.size).toBe(4);
    expect(searchFrameTree(tree, '\u672a\u77e5')).toEqual([unknown[0].dto.nodeId]);
    expect(searchFrameTree(tree, 'unknown')).toEqual([unknown[0].dto.nodeId]);
    assertConserving(tree);
    expect(JSON.stringify(nodes)).toBe(before);
    expect(nodeAt(source, 'a').metrics.sizeBytes).toBe(60n);
});

it('excludes all unknown categories when disabled, including missing frames nested beneath real callers', () => {
    const nodes = fixture();
    const tree = groupUnknownFrames(validateFrameTree(nodes, nodes.length), false);
    expect([...tree.nodes.keys()].sort()).toEqual(['a', 'b', 'root']);
    expect(nodeAt(tree, 'root').metrics).toMatchObject({ sizeBytes: 50n, requestedBytes: 40n, blockCount: 3n });
    assertConserving(tree);
});

it('keeps unknown at the right edge after Python filtering and chain merging', () => {
    const nodes = fixture();
    const tree = groupUnknownFrames(validateFrameTree(nodes, nodes.length), true);
    const projected = mergeSingleChildChains(filterPythonFrames(tree)).tree;
    const unknown = [...projected.nodes.values()].find(current => current.dto.kind === 'unknown');
    expect(unknown).toBeDefined();
    expect(childIds(projected, 'root').at(-1)).toBe(unknown?.dto.nodeId);
    const icicle = layoutReferenceIcicle(projected, 'root', 1000);
    const block = icicle.cells.find(cell => cell.nodeId === unknown?.dto.nodeId);
    expect((block?.x ?? 0) + (block?.width ?? 0)).toBeCloseTo(icicle.width);
    const treemap = layoutTreemap(projected, 'root', Infinity, { width: 1000, height: 360 });
    const tile = treemap.cells.find(cell => cell.nodeId === unknown?.dto.nodeId);
    expect((tile?.x ?? 0) + (tile?.width ?? 0)).toBeCloseTo(1);
    assertConserving(projected);
});

it('handles unknown-only trees, zero-byte blocks and collisions with real node IDs', () => {
    const nodes = [
        node('root', null, 0, 'root', 1, 0, 1, 0, 2, 0, 2),
        node('unknown', 'root', 1, 'frame', 1, 1, 1, 1, 1, 1),
        node('static', 'root', 1, 'static', 0, 0, 0, 0, 1, 1),
    ];
    const source = validateFrameTree(nodes, nodes.length);
    const tree = groupUnknownFrames(source, true);
    expect(nodeAt(tree, 'unknown').dto.kind).toBe('frame');
    const unknown = [...tree.nodes.values()].find(current => current.dto.kind === 'unknown');
    expect(unknown?.dto.nodeId).not.toBe('unknown');
    expect(unknown?.metrics).toMatchObject({ sizeBytes: 0n, blockCount: 1n });
    assertConserving(tree);
    const only = [node('root', null, 0, 'root', 1, 0, 1, 0, 1, 0, 1), node('empty', 'root', 1, 'emptyStack', 1, 1, 1, 1, 1, 1)];
    const hidden = groupUnknownFrames(validateFrameTree(only, 2), false);
    expect(hidden.nodes.size).toBe(1);
    expect(nodeAt(hidden, 'root').metrics.sizeBytes).toBe(0n);
    expect(nodeAt(hidden, 'root').metrics.blockCount).toBe(0n);
});

it('retains exact unknown totals above 2^53', () => {
    const nodes = fixture();
    for (const key of ['sizeBytes', 'selfBytes', 'requestedBytes', 'selfRequestedBytes'] as const) nodes[4][key] = '9007199254740993';
    nodes[0].sizeBytes = '9007199254741078';
    nodes[0].requestedBytes = '9007199254741063';
    const tree = groupUnknownFrames(validateFrameTree(nodes, nodes.length), true);
    const unknown = [...tree.nodes.values()].find(current => current.dto.kind === 'unknown');
    expect(unknown?.metrics.sizeBytes).toBe(9007199254741028n);
    expect(unknown?.metrics.requestedBytes).toBe(9007199254741023n);
    assertConserving(tree);
});

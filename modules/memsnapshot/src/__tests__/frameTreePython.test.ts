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
import { filterPythonFrames } from '../frameTree/filterPython';
import { childIds, nodeAt, pathToNode, validateFrameTree } from '../frameTree/model';
import type { FrameTreeNodeDTO } from '../frameTree/types';
import { frameTreeLeaf } from './fixtures/frameTreeMock';

const frame = (id: string, parent: string | null, depth: number, bytes: number, self: number, children: number,
    filename = `${id}.py`): FrameTreeNodeDTO => ({
    ...frameTreeLeaf(id, parent, String(bytes), depth),
    childCount: children,
    kind: parent === null ? 'root' : 'frame',
    frame: parent === null ? null : { name: id, filename, line: 1 },
    selfBytes: String(self),
    selfRequestedBytes: String(self),
    blockCount: String(bytes),
    selfBlockCount: String(self),
});
const fixture = (): FrameTreeNodeDTO[] => [
    frame('root', null, 0, 100, 0, 3),
    frame('native', 'root', 1, 60, 10, 2, 'native.cpp'),
    frame('nested', 'native', 2, 40, 40, 0),
    frame('native-leaf', 'native', 2, 10, 10, 0, ''),
    frame('python', 'root', 1, 30, 30, 0),
    { ...frame('static', 'root', 1, 10, 10, 0), kind: 'static', frame: null },
];

it('promotes Python descendants, retains static memory on the right and conserves every metric', () => {
    const nodes = fixture();
    const original = validateFrameTree(nodes, nodes.length);
    const before = JSON.stringify(nodes);
    const view = filterPythonFrames(original);
    expect([...view.nodes.keys()].sort()).toEqual(['nested', 'python', 'root', 'static']);
    expect(childIds(view, 'root')).toEqual(['nested', 'python', 'static']);
    expect(pathToNode(view, 'nested')).toEqual(['root', 'nested']);
    expect(nodeAt(view, 'nested').dto.depth).toBe(1);
    expect(nodeAt(view, 'root').metrics.selfBytes).toBe(20n);
    for (const [id, node] of view.nodes) {
        for (const [total, self] of [
            ['sizeBytes', 'selfBytes'], ['requestedBytes', 'selfRequestedBytes'], ['blockCount', 'selfBlockCount'],
        ] as const) {
            const sum = childIds(view, id).reduce((value, child) => value + nodeAt(view, child).metrics[total], node.metrics[self]);
            expect(sum).toBe(node.metrics[total]);
            expect(node.dto[self]).toBe(node.metrics[self].toString());
        }
    }
    expect(JSON.stringify(nodes)).toBe(before);
    expect(childIds(original, 'root')).toEqual(['native', 'python', 'static']);
    expect(nodeAt(original, 'root').metrics.selfBytes).toBe(0n);
});

it.each(['file.py', 'C:\\model\\file.py', '/model/native.cpp/file.py'])('recognizes the .py filename suffix: %s', filename => {
    const nodes = [frame('root', null, 0, 1, 0, 1), frame('leaf', 'root', 1, 1, 1, 0, filename)];
    expect(filterPythonFrames(validateFrameTree(nodes, 2)).nodes.has('leaf')).toBe(true);
});

it.each(['', 'file.pyc', 'file.pyi', 'file.cpp', 'file.PY', 'file.py/native'])('does not treat %s as a Python filename', filename => {
    const nodes = [frame('root', null, 0, 1, 0, 1), frame('leaf', 'root', 1, 1, 1, 0, filename)];
    const view = filterPythonFrames(validateFrameTree(nodes, 2));
    expect(view.nodes.size).toBe(1);
    expect(nodeAt(view, 'root').metrics).toMatchObject({ sizeBytes: 1n, selfBytes: 1n });
});

it('attributes a hidden frame self allocation to its nearest Python ancestor, not the root', () => {
    const nodes = [
        frame('root', null, 0, 100, 0, 1),
        frame('python', 'root', 1, 100, 10, 1),
        frame('native', 'python', 2, 90, 20, 1, 'module.cc'),
        frame('leaf', 'native', 3, 70, 70, 0),
    ];
    const view = filterPythonFrames(validateFrameTree(nodes, nodes.length));
    expect(nodeAt(view, 'python').metrics.selfBytes).toBe(30n);
    expect(nodeAt(view, 'root').metrics.selfBytes).toBe(0n);
    expect(pathToNode(view, 'leaf')).toEqual(['root', 'python', 'leaf']);
});

it('retains non-frame source categories and handles a 1200-frame hidden chain iteratively', () => {
    const nodes = Array.from({ length: 1201 }, (_, depth) =>
        frame(`n${depth}`, depth === 0 ? null : `n${depth - 1}`, depth, 1, depth === 1200 ? 1 : 0,
            depth === 1200 ? 0 : 1, depth === 1200 ? 'leaf.py' : 'native.cpp'));
    const view = filterPythonFrames(validateFrameTree(nodes, nodes.length));
    expect(pathToNode(view, 'n1200')).toEqual(['n0', 'n1200']);
    for (const kind of ['static', 'preexisting', 'emptyStack', 'missingFrame'] as const) {
        const sources = [frame('root', null, 0, 1, 0, 1), { ...frame('source', 'root', 1, 1, 1, 0), kind, frame: null }];
        expect(filterPythonFrames(validateFrameTree(sources, 2)).nodes.has('source')).toBe(true);
    }
});

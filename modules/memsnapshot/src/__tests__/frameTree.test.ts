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
import { createMockFrameTreePage, frameTreeLeaf } from './fixtures/frameTreeMock';
import { validateFrameTree, pathToNode, searchFrameTree, exactRatio, layoutTreemap } from '../frameTree/model';
import { loadFrameTree } from '../frameTree/loader';
import type { FrameTreeNodeDTO, FrameTreeQuery } from '../frameTree/types';

const query: FrameTreeQuery = { deviceId: '0', eventId: 204412, includeStatic: true, pageSize: 1000 };
const leaf = (nodeId: string, parentId: string, bytes: string, depth = 1): FrameTreeNodeDTO => ({
    ...frameTreeLeaf(nodeId, parentId, bytes, depth),
    frame: { filename: `${nodeId}.py`, line: 7, name: nodeId },
});
const fixture = (): FrameTreeNodeDTO[] => [{
    ...leaf('actual-root', '', '9007199254741000', 0),
    parentId: null,
    kind: 'root',
    frame: null,
    selfBytes: '0',
    selfRequestedBytes: '0',
    blockCount: '2',
    selfBlockCount: '0',
    childCount: 2,
}, leaf('large', 'actual-root', '9007199254740993'), leaf('small', 'actual-root', '7')];

describe('complete memory frame tree', () => {
    it('retains exact values above 2^53 and discovers the actual root', () => {
        const tree = validateFrameTree(fixture(), 3);
        expect(tree.rootId).toBe('actual-root');
        expect(tree.nodes.get('large')?.metrics.sizeBytes).toBe(9007199254740993n);
        expect(tree.children.get('actual-root')).toEqual(['large', 'small']);
        expect(exactRatio(1n, 0n)).toBeNull();
        expect(exactRatio(9007199254740993n, 9007199254741000n)).toBeGreaterThan(0.999);
    });

    it.each([
        ['duplicate', (nodes: FrameTreeNodeDTO[]) => { nodes[2].nodeId = 'large'; }],
        ['missing parent', (nodes: FrameTreeNodeDTO[]) => { nodes[2].parentId = 'absent'; }],
        ['depth', (nodes: FrameTreeNodeDTO[]) => { nodes[2].depth = 3; }],
        ['count', (nodes: FrameTreeNodeDTO[]) => { nodes[0].childCount = 1; }],
        ['conservation', (nodes: FrameTreeNodeDTO[]) => { nodes[0].sizeBytes = '1'; }],
        ['integer format', (nodes: FrameTreeNodeDTO[]) => { nodes[1].selfBytes = '1e9'; }],
        ['unsafe frame line', (nodes: FrameTreeNodeDTO[]) => { Object.assign(nodes[1].frame ?? {}, { line: Number.MAX_SAFE_INTEGER + 1 }); }],
        ['multiple roots', (nodes: FrameTreeNodeDTO[]) => { nodes[1].kind = 'root'; }],
    ])('rejects %s rather than repairing broken data', (_, mutate) => {
        const nodes = fixture();
        mutate(nodes);
        expect(() => validateFrameTree(nodes, 3)).toThrow();
    });

    it('finds nodes outside the visible subtree and preserves the complete path', () => {
        const tree = validateFrameTree(fixture(), 3);
        expect(searchFrameTree(tree, '  SMALL.PY ')).toEqual(['small']);
        expect(searchFrameTree(tree, '7')).toEqual(['large', 'small']);
        expect(pathToNode(tree, 'small')).toEqual(['actual-root', 'small']);
    });

    it('handles a 1200-frame path without recursive traversal', () => {
        const nodes: FrameTreeNodeDTO[] = Array.from({ length: 1201 }, (_, index) => ({
            ...leaf(`n${index}`, `n${index - 1}`, '1', index),
            selfBytes: index === 1200 ? '1' : '0',
            selfRequestedBytes: index === 1200 ? '1' : '0',
            selfBlockCount: index === 1200 ? '1' : '0',
            childCount: index === 1200 ? 0 : 1,
        }));
        Object.assign(nodes[0], { kind: 'root', parentId: null, frame: null });
        const tree = validateFrameTree(nodes, 1201);
        expect(pathToNode(tree, 'n1200')).toHaveLength(1201);
        const treemap = layoutTreemap(tree, tree.rootId, Infinity, { width: 1000, height: 360 }).cells;
        expect(treemap).toHaveLength(1201);
        expect(treemap[1200]).toMatchObject({ nodeId: 'n1200', truncated: false });
        expect(treemap[1200].width * treemap[1200].height).toBeGreaterThan(0.9);
    });

    it('shows a non-leaf self allocation without adding a business node', () => {
        const nodes = [leaf('parent', 'actual-root', '10'), leaf('child', 'parent', '7', 2)];
        Object.assign(nodes[0], { selfBytes: '3', selfRequestedBytes: '3', childCount: 1, blockCount: '2' });
        const root = { ...fixture()[0], sizeBytes: '10', requestedBytes: '10', childCount: 1 };
        const tree = validateFrameTree([root, ...nodes], 3);
        const cells = layoutTreemap(tree, 'parent', Infinity, { width: 1000, height: 360 }).cells;
        const self = cells.find(cell => cell.self);
        expect(self?.nodeId).toBe('parent');
        expect((self?.width ?? 0) * (self?.height ?? 0)).toBeGreaterThan(0);
        expect(tree.nodes.size).toBe(3);
    });

    it('uses balanced rectangles rather than one-pixel strips for a wide treemap', () => {
        const nodes = Array.from({ length: 100 }, (_, index) => leaf(`wide-${index}`, 'actual-root', '1'));
        const root = { ...fixture()[0], sizeBytes: '100', requestedBytes: '100', blockCount: '100', childCount: 100 };
        const tree = validateFrameTree([root, ...nodes], 101);
        for (const width of [1000, 120]) {
            const cells = layoutTreemap(tree, tree.rootId, 1, { width, height: 360 }).cells.filter(cell => cell.level === 1);
            expect(cells).toHaveLength(100);
            for (const cell of cells) {
                const aspect = cell.width * width / (cell.height * 360);
                expect(Math.max(aspect, 1 / aspect)).toBeLessThan(5);
            }
        }
    });
});

describe('frame tree full-page assembly', () => {
    it('rejects page sizes above 1000 in both the loader and test fixture', async () => {
        await expect(loadFrameTree({ ...query, pageSize: 1001 }, createMockFrameTreePage,
            new AbortController().signal)).rejects.toThrow('Invalid frame tree query');
        await expect(createMockFrameTreePage({ ...query, pageSize: 1001, currentPage: 1 }))
            .rejects.toThrow('Invalid frame tree mock request');
    });

    it('assembles fixture pages into one conserving tree and filters both static source kinds', async () => {
        const tree = await loadFrameTree({ ...query, pageSize: 100 }, createMockFrameTreePage, new AbortController().signal);
        expect(tree.nodes.size).toBeGreaterThan(600);
        expect([...tree.nodes.values()].some(node => node.dto.kind === 'missingFrame')).toBe(true);
        const filtered = await loadFrameTree({ ...query, includeStatic: false }, createMockFrameTreePage, new AbortController().signal);
        expect([...filtered.nodes.values()].some(node => ['static', 'preexisting'].includes(node.dto.kind))).toBe(false);
        expect(filtered.nodes.get(filtered.rootId)?.metrics.sizeBytes)
            .toBeLessThan(tree.nodes.get(tree.rootId)?.metrics.sizeBytes ?? 0n);
    });

    it('loads 1001 nodes in exact pages before publishing a result', async () => {
        const nodes = Array.from({ length: 1000 }, (_, index) => leaf(`l${index}`, 'actual-root', '1'));
        const root = { ...fixture()[0], sizeBytes: '1000', requestedBytes: '1000', blockCount: '1000', childCount: 1000 };
        const all = [root, ...nodes];
        const pages: number[] = [];
        const tree = await loadFrameTree(query, async params => {
            pages.push(params.currentPage);
            const start = (params.currentPage - 1) * params.pageSize;
            return { total: 1001, nodes: all.slice(start, start + params.pageSize) };
        }, new AbortController().signal);
        expect(pages).toEqual([1, 2]);
        expect(tree.nodes.size).toBe(1001);
    });

    it('loads all six 1000-node transport pages for a 5193-node tree', async () => {
        const children = Array.from({ length: 5192 }, (_, index) => leaf(`n-${index}`, 'actual-root', '1'));
        const root = { ...fixture()[0], sizeBytes: '5192', requestedBytes: '5192', blockCount: '5192', childCount: 5192 };
        const all = [root, ...children];
        const sizes: number[] = [];
        const tree = await loadFrameTree(query, async params => {
            expect(params).toEqual({ ...query, currentPage: sizes.length + 1 });
            const offset = (params.currentPage - 1) * params.pageSize;
            const page = all.slice(offset, offset + params.pageSize);
            sizes.push(page.length);
            return { total: 5193, nodes: page };
        }, new AbortController().signal);
        expect(sizes).toEqual([1000, 1000, 1000, 1000, 1000, 193]);
        expect(tree.nodes.size).toBe(5193);
    });

    it('rejects an exceeded display budget without publishing a truncated tree', async () => {
        await expect(loadFrameTree(query, async () => ({ total: 100001, nodes: [] }),
            new AbortController().signal)).rejects.toThrow('display limit');
    });

    it('retries a temporary transport failure once but never retries a business error', async () => {
        jest.useFakeTimers();
        try {
            let calls = 0;
            const result = loadFrameTree(query, async () => {
                calls++;
                if (calls === 1) throw new TypeError('Network unavailable');
                return { total: 3, nodes: fixture() };
            }, new AbortController().signal);
            await Promise.resolve();
            await Promise.resolve();
            jest.advanceTimersByTime(500);
            expect((await result).nodes.size).toBe(3);
            expect(calls).toBe(2);
            calls = 0;
            await expect(loadFrameTree(query, async () => {
                calls++;
                throw new Error('Invalid event');
            }, new AbortController().signal)).rejects.toThrow('Invalid event');
            expect(calls).toBe(1);
        } finally {
            jest.useRealTimers();
        }
    });

    it('rejects a short page, changed total and duplicate page', async () => {
        const first = { total: 3, nodes: fixture().slice(0, 2) };
        for (const second of [{ total: 3, nodes: [] }, { total: 4, nodes: [fixture()[2]] },
            { total: 3, nodes: [fixture()[1]] }]) {
            await expect(loadFrameTree({ ...query, pageSize: 2 }, async params =>
                params.currentPage === 1 ? first : second, new AbortController().signal)).rejects.toThrow();
        }
    });

    it('discards an aborted late page and does not request another page', async () => {
        const controller = new AbortController();
        let calls = 0;
        await expect(loadFrameTree({ ...query, pageSize: 2 }, async () => {
            calls++;
            controller.abort();
            return { total: 3, nodes: fixture().slice(0, 2) };
        }, controller.signal)).rejects.toThrow();
        expect(calls).toBe(1);
    });
});

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
import { frameTreeTheme as theme, readyUnmerged } from './fixtures/frameTreeUI';
import React from 'react';
import { fireEvent, render } from '@testing-library/react';
import * as testingLibrary from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { nodeAt, validateFrameTree } from '../frameTree/model';
import { mergeSingleChildChains } from '../frameTree/mergeChains';
import { FrameTreePanel } from '../components/FrameTreePanel';
import { FrameTreeDetails } from '../frameTree/FrameTreeDetails';
import type { FrameTreeDetailsHandle } from '../frameTree/FrameTreeDetails';
import type { FrameTreeNodeDTO } from '../frameTree/types';
import { frameTreeLeaf } from './fixtures/frameTreeMock';

const frame = (id: string, parentId: string | null, depth: number, size: number, self: number, childCount: number,
    name = id): FrameTreeNodeDTO => ({
    ...frameTreeLeaf(id, parentId, String(size), depth),
    childCount,
    kind: parentId === null ? 'root' : 'frame',
    frame: parentId === null ? null : { name, filename: `${id}.py`, line: depth },
    selfBytes: String(self),
    selfRequestedBytes: String(self),
    blockCount: String(size),
    selfBlockCount: String(self),
});
const fixture = (): FrameTreeNodeDTO[] => [
    frame('root', null, 0, 120, 0, 2),
    frame('a', 'root', 1, 100, 10, 1, 'outer'),
    frame('b', 'a', 2, 90, 20, 1, 'torch::middle(Tensor input)'),
    frame('c', 'b', 3, 70, 0, 2, 'module::forward(Tensor input)'),
    frame('d', 'c', 4, 30, 30, 0),
    frame('e', 'c', 4, 40, 40, 0),
    { ...frame('static', 'root', 1, 20, 20, 0), kind: 'static', frame: null },
];
const showUnmerged = async (nodes = fixture()): Promise<ReturnType<typeof render> & { request: jest.Mock }> => {
    const request = jest.fn(async () => ({ total: nodes.length, nodes }));
    const view = render(<ThemeProvider theme={theme}>
        <FrameTreePanel context={{ datasetKey: 'chain', deviceId: '0', eventId: 3 }} request={request} />
    </ThemeProvider>);
    await readyUnmerged(view);
    return { ...view, request };
};

describe('single-child-chain display', () => {
    it('defaults to Python-only in both charts and restores native frames without another API request', async () => {
        const nodes = fixture();
        if (nodes[2].frame) nodes[2].frame.filename = 'native.cpp';
        const view = await showUnmerged(nodes);
        const toggle = view.getByRole('checkbox', { name: 'frameTree.pythonOnly' }) as HTMLInputElement;
        expect(toggle.checked).toBe(true);
        expect(view.queryByTestId('frame-tree-icicle-b')).toBeNull();
        expect(view.getByTestId('frame-tree-icicle-c')).toBeTruthy();
        expect(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="b"]')).toBeNull();
        fireEvent.click(view.getByTestId('frame-tree-icicle-c'));
        expect(view.getByTestId('frame-tree-ancestors').textContent).not.toContain('native.cpp');
        fireEvent.change(view.getByRole('textbox', { name: 'frameTree.search' }), { target: { value: 'native.cpp' } });
        await view.findByText('frameTree.noMatches');
        fireEvent.click(toggle);
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-locked')).toBe('false');
        expect(view.getByTestId('frame-tree-icicle-b')).toBeTruthy();
        expect(await view.findByTestId('frame-tree-search-b')).toBeTruthy();
        fireEvent.click(toggle);
        expect(view.queryByTestId('frame-tree-icicle-b')).toBeNull();
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('root');
        expect(view.request).toHaveBeenCalledTimes(1);
    });

    it('merges chains by default and still allows switching back to individual frames', async () => {
        const request = jest.fn(async () => ({ total: 7, nodes: fixture() }));
        const view = render(<ThemeProvider theme={theme}>
            <FrameTreePanel context={{ datasetKey: 'default-merge', deviceId: '0', eventId: 3 }} request={request} />
        </ThemeProvider>);
        await view.findByTestId('frame-tree-ready');
        const toggle = view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }) as HTMLInputElement;
        expect(toggle.checked).toBe(true);
        expect(view.getByTestId('frame-tree-icicle-a').getAttribute('aria-label')).toBe('forward \u00d73: 100 B');
        expect(view.queryByTestId('frame-tree-icicle-b')).toBeNull();
        fireEvent.click(toggle);
        expect(view.getByTestId('frame-tree-icicle-b')).toBeTruthy();
        expect(view.getByTestId('frame-tree-icicle-c')).toBeTruthy();
        expect(request).toHaveBeenCalledTimes(1);
        view.rerender(<ThemeProvider theme={theme}>
            <FrameTreePanel context={{ datasetKey: 'default-merge', deviceId: '0', eventId: 4 }} request={request} />
        </ThemeProvider>);
        await view.findByTestId('frame-tree-ready');
        expect((view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }) as HTMLInputElement).checked).toBe(true);
    });

    it('merges only frame chains, conserves all metrics and leaves the source tree untouched', () => {
        const nodes = fixture();
        const tree = validateFrameTree(nodes, nodes.length);
        const before = JSON.stringify(nodes);
        const merged = mergeSingleChildChains(tree);
        expect([...merged.tree.nodes.keys()]).toEqual(expect.arrayContaining(['root', 'a', 'd', 'e', 'static']));
        expect(merged.tree.nodes.size).toBe(5);
        const group = nodeAt(merged.tree, 'a');
        expect(group.merged?.ids).toEqual(['a', 'b', 'c']);
        expect(group.merged?.name).toBe('forward');
        expect(group.metrics.sizeBytes).toBe(100n);
        expect(group.metrics.selfBytes).toBe(30n);
        expect(group.metrics.selfRequestedBytes).toBe(30n);
        expect(group.metrics.selfBlockCount).toBe(30n);
        expect(merged.tree.nodes.get('d')?.dto).toMatchObject({ parentId: 'a', depth: 2 });
        expect(merged.owners.get('c')).toBe('a');
        expect(merged.tree.nodes.get('root')?.merged).toBeUndefined();
        expect(merged.tree.nodes.get('static')?.merged).toBeUndefined();
        expect(JSON.stringify(nodes)).toBe(before);
        expect(tree.nodes.get('a')?.dto.childCount).toBe(1);
        expect(tree.children.get('b')).toEqual(['c']);
        expect(validateFrameTree([...merged.tree.nodes.values()].map(node => node.dto), 5).nodes.size).toBe(5);
    });

    it('keeps focused frames addressable and does not merge a branching node with a child', () => {
        const nodes = fixture();
        const merged = mergeSingleChildChains(validateFrameTree(nodes, nodes.length), new Set(['b']));
        expect(merged.tree.nodes.get('a')?.merged).toBeUndefined();
        expect(merged.tree.nodes.get('b')?.merged?.ids).toEqual(['b', 'c']);
        expect(merged.tree.children.get('b')).toEqual(['e', 'd']);
        expect(merged.tree.nodes.get('d')?.merged).toBeUndefined();
    });

    it('handles deep, zero-byte chains without recursive traversal', () => {
        const nodes = Array.from({ length: 2401 }, (_, depth) => frame(
            `n${depth}`, depth ? `n${depth - 1}` : null, depth, 0, 0, depth === 2400 ? 0 : 1,
        ));
        const merged = mergeSingleChildChains(validateFrameTree(nodes, nodes.length));
        expect(merged.tree.nodes.size).toBe(2);
        expect(merged.tree.nodes.get('n1')?.merged?.ids).toHaveLength(2400);
        expect(merged.owners.get('n2400')).toBe('n1');
    });

    it('toggles merging, uses short labels and reveals the full original merged path without requests', async () => {
        const view = await showUnmerged();
        const toggle = view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }) as HTMLInputElement;
        expect(toggle.checked).toBe(false);
        fireEvent.click(view.getByTestId('frame-tree-icicle-b'));
        fireEvent.click(toggle);
        expect(view.queryByTestId('frame-tree-icicle-b')).toBeNull();
        const group = view.getByTestId('frame-tree-icicle-a');
        expect(group.getAttribute('aria-pressed')).toBe('true');
        expect(group.getAttribute('aria-label')).toBe('forward \u00d73: 100 B');
        fireEvent.click(group);
        const path = view.getByTestId('frame-tree-ancestors');
        expect(path.textContent).toContain('outer');
        expect(path.textContent).toContain('torch::middle(Tensor input)');
        expect(path.textContent).toContain('module::forward(Tensor input)');
        expect(path.textContent).toContain('b.py:2');
        expect(path.querySelectorAll('[data-merged="true"]')).toHaveLength(3);
        expect(view.container.querySelector('.frame-tree-raw')).toBeNull();
        expect(view.queryByText('frameTree.exactMetrics')).toBeNull();
        fireEvent.click(toggle);
        expect(view.getByTestId('frame-tree-icicle-b')).toBeTruthy();
        expect(view.getByTestId('frame-tree-icicle-c')).toBeTruthy();
        expect(view.request).toHaveBeenCalledTimes(1);
    });

    it('keeps canonical search and real-parent navigation when merging is enabled', async () => {
        const view = await showUnmerged();
        fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }));
        fireEvent.doubleClick(view.getByTestId('frame-tree-icicle-a'));
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('c');
        expect(view.container.querySelector('.frame-tree-inspector')?.getAttribute('data-node-id')).toBe('c');
        const controls = testingLibrary.within(view.container.querySelector('.frame-tree-toolbar') as HTMLElement);
        fireEvent.click(controls.getByRole('button', { name: 'frameTree.up' }));
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('b');
        fireEvent.change(view.getByRole('textbox', { name: 'frameTree.search' }), { target: { value: 'b.py' } });
        fireEvent.click(await view.findByTestId('frame-tree-search-b'));
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('a');
        expect(testingLibrary.within(view.getByTestId('frame-tree-details')).getByRole('heading').textContent)
            .toBe('torch::middle(Tensor input)');
        expect(view.getByTestId('frame-tree-icicle-a').getAttribute('aria-pressed')).toBe('true');
        expect(view.request).toHaveBeenCalledTimes(1);
    });

    it('keeps both navigation paths compact while navigating to the real end of each chain', async () => {
        const view = await showUnmerged();
        fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }));
        const treemap = view.getByTestId('frame-tree-chart');
        fireEvent.doubleClick(treemap.querySelector('[data-node-id="a"]') as Element);
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('root');
        const path = view.getByRole('navigation', { name: 'frameTree.treemapPath' });
        expect(testingLibrary.within(path).getAllByRole('button').map(button => button.textContent))
            .toEqual(['frameTree.kind.root', 'forward \u00d73']);
        fireEvent.click(testingLibrary.within(path).getByRole('button', { name: 'forward \u00d73' }));
        expect(treemap.getAttribute('data-focus')).toBe('c');
        fireEvent.click(testingLibrary.within(view.getByTestId('frame-tree-treemap-navigation'))
            .getByRole('button', { name: 'frameTree.root' }));
        fireEvent.doubleClick(view.getByTestId('frame-tree-icicle-a'));
        const upperPath = view.getByRole('navigation', { name: 'frameTree.path' });
        expect(testingLibrary.within(upperPath).getAllByRole('button').map(button => button.textContent))
            .toEqual(['frameTree.kind.root', 'forward \u00d73']);
        fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }));
        expect(testingLibrary.within(upperPath).getAllByRole('button')).toHaveLength(4);
        expect(view.request).toHaveBeenCalledTimes(1);
    });

    describe.each([
        ['treemap', 'frame-tree-chart', 'frame-tree-icicle'],
        ['icicle', 'frame-tree-icicle', 'frame-tree-chart'],
    ])('%s selection', (kind, chartId, otherChartId) => {
        it.each([false, true])('locks both charts and unlocks on reselection (merge=%s)', async mergeSingles => {
            const view = await showUnmerged();
            if (mergeSingles) fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }));
            const chart = view.getByTestId(chartId);
            const cell = (id: string): Element => chart.querySelector(`g[data-node-id="${id}"]`) as Element;
            const first = cell(mergeSingles ? 'a' : 'd');
            const other = cell('e');
            const heading = (): string | null => testingLibrary.within(view.getByTestId('frame-tree-details')).getByRole('heading').textContent;
            const selectedName = mergeSingles ? 'forward \u00d73' : 'd';
            fireEvent.mouseEnter(first);
            fireEvent.click(first);
            expect(chart.getAttribute('data-locked')).toBe('true');
            expect(first.getAttribute('data-locked')).toBe('true');
            expect(view.getByText('frameTree.locked')).toBeTruthy();
            fireEvent.mouseEnter(other);
            fireEvent.mouseLeave(chart.querySelector('svg') as Element);
            fireEvent.mouseLeave(chart);
            fireEvent.scroll(chart);
            fireEvent.scroll(view.getByTestId('frame-tree-ancestors'), { target: { scrollTop: 24 } });
            fireEvent.mouseEnter(view.getByTestId(otherChartId).querySelector('[data-node-id="e"]') as Element);
            expect(heading()).toBe(selectedName);
            expect(other.getAttribute('data-hovered')).toBeNull();
            if (kind === 'icicle') {
                expect(first.getAttribute('data-call-path')).toBe('true');
                expect(other.getAttribute('data-call-path')).toBeNull();
            }
            fireEvent.click(first);
            expect(chart.getAttribute('data-locked')).toBe('false');
            expect(view.queryByText('frameTree.locked')).toBeNull();
            fireEvent.mouseEnter(other);
            expect(heading()).toBe('e');
            expect(first.getAttribute('data-hovered')).toBeNull();
            if (kind === 'icicle') {
                expect(first.getAttribute('data-call-path')).toBe(mergeSingles ? 'true' : null);
                expect(other.getAttribute('data-call-path')).toBe('true');
            }
            fireEvent.click(other);
            fireEvent.mouseEnter(first);
            expect(heading()).toBe('e');
            expect(other.getAttribute('data-locked')).toBe('true');
            fireEvent.keyDown(other, { key: ' ' });
            expect(chart.getAttribute('data-locked')).toBe('false');
            fireEvent.mouseEnter(first);
            expect(heading()).toBe(selectedName);
            expect(view.queryByRole('tooltip')).toBeNull();
            expect(view.request).toHaveBeenCalledTimes(1);
        });

        it('supports keyboard locking, reselection and unlocking on navigation or display changes', async () => {
            const view = await showUnmerged();
            const chart = view.getByTestId(chartId);
            const cell = (id: string): Element => chart.querySelector(`g[data-node-id="${id}"]`) as Element;
            fireEvent.keyDown(cell('d'), { key: ' ' });
            expect(chart.getAttribute('data-locked')).toBe('true');
            fireEvent.click(cell('e'));
            fireEvent.mouseEnter(cell('d'));
            expect(view.container.querySelector('.frame-tree-inspector')?.getAttribute('data-node-id')).toBe('e');
            fireEvent.keyDown(cell('e'), { key: ' ' });
            expect(chart.getAttribute('data-locked')).toBe('false');
            fireEvent.click(cell('d'));
            fireEvent.doubleClick(cell('a'));
            expect(chart.getAttribute('data-locked')).toBe('false');
            expect(chart.getAttribute('data-focus')).toBe('a');
            fireEvent.mouseEnter(cell('b'));
            expect(view.container.querySelector('.frame-tree-inspector')?.getAttribute('data-node-id')).toBe('b');
            fireEvent.click(cell('d'));
            fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.mergeSingles' }));
            expect(chart.getAttribute('data-locked')).toBe('false');
        });
    });

    it('rejects late hover previews at the inspector while locked and accepts them after unlocking', () => {
        const nodes = fixture();
        const tree = validateFrameTree(nodes, nodes.length);
        const inspector = React.createRef<FrameTreeDetailsHandle>();
        const content = (locked: boolean): JSX.Element => <ThemeProvider theme={theme}>
            <FrameTreeDetails ref={inspector} tree={tree} selected="d" locked={locked} onSelect={jest.fn()} onEnter={jest.fn()} />
        </ThemeProvider>;
        const view = render(content(false));
        const delayedPreview = (): void => inspector.current?.preview('e');
        view.rerender(content(true));
        testingLibrary.act(delayedPreview);
        expect(view.getByRole('heading').textContent).toBe('d');
        view.rerender(content(false));
        testingLibrary.act(delayedPreview);
        expect(view.getByRole('heading').textContent).toBe('e');
    });
});

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
import { act, fireEvent, render } from '@testing-library/react';
import * as testingLibrary from '@testing-library/react';
import { ThemeProvider } from '@emotion/react';
import { FrameTreePanel } from '../components/FrameTreePanel';
import type { FrameTreePage, FrameTreePageRequest } from '../frameTree/types';
import { createMockFrameTreePage } from './fixtures/frameTreeMock';
import { FrameTreeIcicle } from '../frameTree/FrameTreeIcicle';
import { loadFrameTree } from '../frameTree/loader';
import { formatExactBytes } from '../frameTree/model';

const context = { datasetKey: 'dataset-a', deviceId: '0', eventId: 204412 };
const show = (request: FrameTreePageRequest = createMockFrameTreePage, event = context): JSX.Element =>
    <ThemeProvider theme={theme}><FrameTreePanel context={event} request={request} /></ThemeProvider>;

describe('memory frame tree panel', () => {
    it('shows one unknown group in both charts and hides every unattributed source when its switch is off', async () => {
        const original = await loadFrameTree({ deviceId: '0', eventId: 204412, includeStatic: true, pageSize: 1000 },
            createMockFrameTreePage, new AbortController().signal);
        const unknownBytes = [...original.nodes.values()].filter(node => node.dto.kind !== 'root' && node.dto.kind !== 'frame')
            .reduce((sum, node) => sum + node.metrics.sizeBytes, 0n);
        const request = jest.fn(createMockFrameTreePage);
        const view = render(show(request));
        await readyUnmerged(view);
        const toggle = view.getByRole('checkbox', { name: 'frameTree.includeUnknown' }) as HTMLInputElement;
        expect(toggle.checked).toBe(true);
        expect(view.queryByTestId('frame-tree-icicle-static')).toBeNull();
        expect(view.queryByTestId('frame-tree-icicle-empty')).toBeNull();
        expect(view.queryByTestId('frame-tree-icicle-preexisting')).toBeNull();
        expect(view.queryByTestId('frame-tree-icicle-missing')).toBeNull();
        const unknown = view.getByTestId('frame-tree-icicle-unknown');
        expect(unknown.getAttribute('aria-label')).toContain('frameTree.kind.unknown');
        expect(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="unknown"]')).toBeTruthy();
        fireEvent.click(unknown);
        expect(testingLibrary.within(view.getByTestId('frame-tree-details')).getByRole('heading').textContent).toBe('frameTree.kind.unknown');
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-locked')).toBe('true');
        fireEvent.click(toggle);
        await readyUnmerged(view);
        expect(view.queryByTestId('frame-tree-icicle-unknown')).toBeNull();
        expect(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="unknown"]')).toBeNull();
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-locked')).toBe('false');
        const root = original.nodes.get(original.rootId);
        expect(root).toBeDefined();
        const knownBytes = (root?.metrics.sizeBytes ?? 0n) - unknownBytes;
        expect(view.getByTestId('frame-tree-icicle-snapshot-root').getAttribute('aria-label')).toBe(`frameTree.kind.root: ${formatExactBytes(knownBytes)}`);
        expect(view.getByTestId('frame-tree-ready').textContent).not.toContain('frameTree.kind.static');
        expect(knownBytes).toBeGreaterThan(0n);
        fireEvent.click(toggle);
        await readyUnmerged(view);
        expect(view.getByTestId('frame-tree-icicle-unknown')).toBeTruthy();
    });

    it('keeps the configured 1000-node pages and API contract when unknown inclusion changes', async () => {
        const request = jest.fn(createMockFrameTreePage);
        const selected = { ...context, deviceId: '1', eventId: 204413 };
        const view = render(show(request, selected));
        await readyUnmerged(view);
        expect(request).toHaveBeenCalledWith({
            deviceId: '1', eventId: 204413, includeStatic: true, currentPage: 1, pageSize: 1000,
        }, expect.any(AbortSignal));
        fireEvent.click(view.getByRole('checkbox', { name: 'frameTree.includeUnknown' }));
        await readyUnmerged(view);
        expect(request).toHaveBeenCalledWith({
            deviceId: '1', eventId: 204413, includeStatic: false, currentPage: 1, pageSize: 1000,
        }, expect.any(AbortSignal));
    });

    it('highlights and previews the call chain without committing the icicle chart on pointer movement', async () => {
        let commits = 0;
        const tree = await loadFrameTree({ deviceId: '0', eventId: 204412, includeStatic: true, pageSize: 1000 },
            createMockFrameTreePage, new AbortController().signal);
        const preview = jest.fn();
        const view = render(<ThemeProvider theme={theme}>
            <React.Profiler id="icicle" onRender={(): void => { commits++; }}>
                <FrameTreeIcicle tree={tree} focus={tree.rootId} selected={tree.rootId} matches={new Set()}
                    onSelect={(): void => {}} onEnter={(): void => {}} onPreview={preview} />
            </React.Profiler>
        </ThemeProvider>);
        commits = 0;
        const cell = view.getByTestId('frame-tree-icicle-b5-g9-f17');
        fireEvent.mouseEnter(cell, { clientX: 100, clientY: 100 });
        for (let position = 0; position < 20; position++) fireEvent.mouseMove(cell, { clientX: 100 + position, clientY: 100 });
        expect(commits).toBe(0);
        expect(preview).toHaveBeenCalledWith('b5-g9-f17');
        for (const id of ['snapshot-root', 'branch-5', 'b5-g9', 'b5-g9-f17']) {
            expect(view.getByTestId(`frame-tree-icicle-${id}`).getAttribute('data-call-path')).toBe('true');
        }
        expect(view.getByTestId('frame-tree-icicle-branch-0').getAttribute('data-call-path')).toBeNull();
    });

    it('shows the reference icicle above the unchanged treemap with details on the right and no view toggles', async () => {
        let requests = 0;
        const request: FrameTreePageRequest = async params => { requests++; return createMockFrameTreePage(params); };
        const view = render(show(request));
        await readyUnmerged(view);
        expect(view.queryByText('frameTree.mock')).toBeNull();
        const before = requests;
        const controls = testingLibrary.within(view.container.querySelector('.frame-tree-toolbar') as HTMLElement);
        const details = testingLibrary.within(view.getByTestId('frame-tree-details'));
        const icicle = view.getByTestId('frame-tree-icicle');
        const treemap = view.getByTestId('frame-tree-chart');
        expect(icicle.compareDocumentPosition(treemap) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
        expect(view.queryByRole('tree')).toBeNull();
        expect(controls.queryByRole('button', { name: 'frameTree.icicle' })).toBeNull();
        expect(controls.queryByRole('button', { name: 'frameTree.treemap' })).toBeNull();
        expect(treemap.getAttribute('data-view')).toBe('treemap');
        fireEvent.click(view.getByTestId('frame-tree-icicle-branch-5'));
        expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('snapshot-root');
        expect(view.getByTestId('frame-tree-details').textContent).toContain('checkpoint');
        fireEvent.click(details.getByRole('button', { name: 'frameTree.enterSubtree' }));
        expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('branch-5');
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('branch-5');
        expect(requests).toBe(before);
        fireEvent.click(controls.getByRole('button', { name: 'frameTree.up' }));
        expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('snapshot-root');
        fireEvent.change(view.getByRole('textbox', { name: 'frameTree.search' }), { target: { value: 'deep-17' } });
        await view.findByTestId('frame-tree-search-deep-17');
        fireEvent.click(view.getByTestId('frame-tree-search-deep-17'));
        fireEvent.click(controls.getByRole('button', { name: 'frameTree.root' }));
        expect(view.getByTestId('frame-tree-icicle-snapshot-root')).toBeTruthy();
    });

    it('searches the complete tree and locates deep paths in both charts', async () => {
        const view = render(show());
        await readyUnmerged(view);
        fireEvent.change(view.getByRole('textbox', { name: 'frameTree.search' }), { target: { value: 'deep-17' } });
        await view.findByTestId('frame-tree-search-deep-17');
        fireEvent.click(view.getByTestId('frame-tree-search-deep-17'));
        expect(view.getByTestId('frame-tree-details').textContent).toContain('recursive_17');
        expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('deep-16');
        expect(view.getByTestId('frame-tree-icicle-deep-17')).toBeTruthy();
    });

    it.each(['resolve', 'reject'])('isolates a late old %s from the current event', async outcome => {
        let resolveOld!: (page: FrameTreePage) => void;
        let rejectOld!: (error: Error) => void;
        const old = new Promise<FrameTreePage>((resolve, reject) => { resolveOld = resolve; rejectOld = reject; });
        const request: FrameTreePageRequest = params => params.eventId === context.eventId
            ? old
            : createMockFrameTreePage(params);
        const view = render(show(request));
        view.rerender(show(request, { ...context, eventId: 204413 }));
        await readyUnmerged(view);
        await act(async () => {
            if (outcome === 'reject') rejectOld(new Error('old failure'));
            else resolveOld(await createMockFrameTreePage({ ...context, includeStatic: true, pageSize: 1000, currentPage: 1 }));
        });
        expect(view.getByTestId('frame-tree-ready')).toBeTruthy();
        expect(view.queryByText('old failure')).toBeNull();
        expect(view.getByTestId('frame-tree-context').textContent).toContain('204413');
    });

    it('shows a legitimate zero result and retries a failed query from page one', async () => {
        const root: FrameTreePage = {
            total: 1,
            nodes: [{
                nodeId: 'r',
                parentId: null,
                depth: 0,
                kind: 'root',
                frame: null,
                childCount: 0,
                sizeBytes: '0',
                selfBytes: '0',
                requestedBytes: '0',
                selfRequestedBytes: '0',
                blockCount: '0',
                selfBlockCount: '0',
            }],
        };
        let fail = true;
        const pages: number[] = [];
        const view = render(show(async params => {
            pages.push(params.currentPage);
            if (fail) throw new Error('business failure');
            return root;
        }));
        await view.findByRole('alert');
        fail = false;
        fireEvent.click(view.getByRole('button', { name: 'frameTree.retry' }));
        await view.findByText('frameTree.empty');
        expect(pages).toEqual([1, 1]);
    });

    it('shows a failure even if the rejected error has an empty message', async () => {
        const view = render(show(async () => { throw new Error(''); }));
        await view.findByRole('alert');
        expect(view.getByRole('button', { name: 'frameTree.retry' })).toBeTruthy();
    });

    it('clears a previously ready view immediately when the dataset changes', async () => {
        const request: FrameTreePageRequest = params => params.deviceId === '0'
            ? createMockFrameTreePage(params)
            : new Promise(() => undefined);
        const view = render(show(request));
        await readyUnmerged(view);
        view.rerender(show(request, { ...context, datasetKey: 'dataset-b', deviceId: '1' }));
        expect(view.queryByTestId('frame-tree-ready')).toBeNull();
        expect(view.getByTestId('frame-tree-context').textContent).toContain('1');
        view.unmount();
    });

    it('repeats locating the same search result without losing the selected node', async () => {
        const view = render(show());
        await readyUnmerged(view);
        fireEvent.change(view.getByRole('textbox', { name: 'frameTree.search' }), { target: { value: 'deep-17' } });
        const result = await view.findByTestId('frame-tree-search-deep-17');
        fireEvent.click(result);
        expect(view.getByTestId('frame-tree-icicle-deep-17').getAttribute('aria-pressed')).toBe('true');
        fireEvent.click(result);
        expect(view.getByTestId('frame-tree-icicle-deep-17').getAttribute('aria-pressed')).toBe('true');
    });

    it('updates the inspector on hover without changing selection or focus and keeps the preview available to read', async () => {
        const view = render(show());
        await readyUnmerged(view);
        const cell = view.getByTestId('frame-tree-icicle-branch-5');
        fireEvent.mouseEnter(cell, { clientX: 100, clientY: 100 });
        expect(view.getByTestId('frame-tree-icicle-snapshot-root').getAttribute('data-call-path')).toBe('true');
        expect(view.getByTestId('frame-tree-icicle-branch-0').getAttribute('data-call-path')).toBeNull();
        const details = view.getByTestId('frame-tree-details');
        expect(testingLibrary.within(details).getByRole('heading', { level: 4 }).textContent).toBe('checkpoint');
        expect(view.getByTestId('frame-tree-ancestors').textContent).toContain('checkpoint');
        expect(cell.getAttribute('aria-pressed')).toBe('false');
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('snapshot-root');
        fireEvent.mouseMove(cell, { clientX: 200, clientY: 150 });
        expect(view.queryByRole('tooltip')).toBeNull();
        fireEvent.mouseLeave(view.getByTestId('frame-tree-icicle').getElementsByTagName('svg')[0]);
        expect(view.getByTestId('frame-tree-icicle-snapshot-root').getAttribute('data-call-path')).toBeNull();
        expect(testingLibrary.within(details).getByRole('heading', { level: 4 }).textContent).toBe('checkpoint');
        fireEvent.click(cell);
        fireEvent.mouseEnter(view.getByTestId('frame-tree-icicle-branch-0'), { clientX: 500, clientY: 400 });
        expect(testingLibrary.within(details).getByRole('heading', { level: 4 }).textContent).toBe('checkpoint');
        expect(cell.getAttribute('aria-pressed')).toBe('true');
        fireEvent.click(cell);
        fireEvent.mouseEnter(view.getByTestId('frame-tree-icicle-branch-0'), { clientX: 500, clientY: 400 });
        expect(testingLibrary.within(details).getByRole('heading', { level: 4 }).textContent).toBe('train_step');
        fireEvent.click(cell);
        expect(testingLibrary.within(details).getByRole('heading', { level: 4 }).textContent).toBe('checkpoint');
    });

    it('does not scroll away from the treemap between single-click and double-click', async () => {
        const descriptor = Object.getOwnPropertyDescriptor(Element.prototype, 'scrollIntoView');
        const scroll = jest.fn();
        Object.defineProperty(Element.prototype, 'scrollIntoView', { configurable: true, value: scroll });
        try {
            const view = render(show());
            await readyUnmerged(view);
            const cell = Array.from(view.getByTestId('frame-tree-chart').querySelectorAll('[role="button"]'))
                .find(element => element.getAttribute('aria-label')?.startsWith('checkpoint:'));
            if (!cell) throw new Error('Checkpoint treemap cell is missing');
            fireEvent.click(cell);
            expect(scroll).not.toHaveBeenCalled();
            fireEvent.doubleClick(cell);
            expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('branch-5');
            expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('snapshot-root');
            const navigation = testingLibrary.within(view.getByTestId('frame-tree-treemap-navigation'));
            fireEvent.click(navigation.getByRole('button', { name: 'frameTree.up' }));
            expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('snapshot-root');
        } finally {
            if (descriptor) Object.defineProperty(Element.prototype, 'scrollIntoView', descriptor);
            else Reflect.deleteProperty(Element.prototype, 'scrollIntoView');
        }
    });

    it('shows the full treemap without depth controls and readable ancestor rows in the inspector', async () => {
        const view = render(show());
        await readyUnmerged(view);
        expect(view.queryByRole('combobox', { name: 'frameTree.treemapDepth' })).toBeNull();
        expect(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="deep-17"]')).toBeTruthy();
        fireEvent.click(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="deep-17"]') as Element);
        const path = view.getByTestId('frame-tree-ancestors');
        expect(path.textContent).toContain('L0');
        expect(path.textContent).toContain('L19');
        expect(testingLibrary.within(path).getAllByRole('button')).toHaveLength(20);
        expect(view.getByTestId('frame-tree-details').textContent).toContain('frameTree.size');
    });

    it('previews a treemap node without entering its subtree or changing the icicle focus', async () => {
        const view = render(show());
        await readyUnmerged(view);
        const cell = view.getByTestId('frame-tree-chart').querySelector('[data-node-id="branch-5"]');
        if (!cell) throw new Error('Treemap checkpoint node is missing');
        fireEvent.mouseEnter(cell, { clientX: 100, clientY: 100 });
        const heading = testingLibrary.within(view.getByTestId('frame-tree-details')).getByRole('heading', { level: 4 });
        expect(heading.textContent).toBe('checkpoint');
        expect(view.getByTestId('frame-tree-chart').getAttribute('data-focus')).toBe('snapshot-root');
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('snapshot-root');
        expect(view.queryByRole('tooltip')).toBeNull();
        expect(cell.getAttribute('data-hovered')).toBe('true');
        fireEvent.mouseLeave(view.getByTestId('frame-tree-chart'));
        expect(cell.getAttribute('data-hovered')).toBeNull();
        expect(heading.textContent).toBe('checkpoint');
    });

    it('does not intercept icicle Escape navigation after previewing a treemap node', async () => {
        const view = render(show());
        await readyUnmerged(view);
        fireEvent.mouseEnter(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="branch-5"]') as Element,
            { clientX: 100, clientY: 100 });
        expect(view.queryByRole('tooltip')).toBeNull();
        fireEvent.keyDown(document, { key: 'Escape' });
        expect(view.queryByRole('tooltip')).toBeNull();
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('snapshot-root');
        fireEvent.keyDown(view.getByTestId('frame-tree-icicle-branch-5'), { key: 'Enter' });
        const entered = view.getByTestId('frame-tree-icicle-branch-5');
        fireEvent.mouseEnter(view.getByTestId('frame-tree-chart').querySelector('[data-node-id="branch-5"]') as Element,
            { clientX: 100, clientY: 100 });
        fireEvent.keyDown(entered, { key: 'Escape' });
        expect(view.queryByRole('tooltip')).toBeNull();
        expect(view.getByTestId('frame-tree-icicle').getAttribute('data-focus')).toBe('snapshot-root');
    });

    it('makes a tiny branch and its deepest descendant visible at the minimum width', async () => {
        const view = render(show());
        await readyUnmerged(view);
        const rect = view.getByTestId('frame-tree-icicle-deep-0').querySelector('rect');
        expect(Number(rect?.getAttribute('width'))).toBeGreaterThanOrEqual(6 - 1e-8);
        expect(Number(view.getByTestId('frame-tree-icicle-deep-17').querySelector('rect')?.getAttribute('width'))).toBeGreaterThanOrEqual(6 - 1e-8);
    });
});

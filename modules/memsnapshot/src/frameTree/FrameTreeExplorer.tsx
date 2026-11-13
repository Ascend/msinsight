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
import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import styled from '@emotion/styled';
import { Button, Checkbox, Input } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import type { FrameTree } from './types';
import { formatExactBytes, nodeAt, pathToNode, searchFrameTree } from './model';
import { FrameTreeChart, frameTreeNodeName } from './FrameTreeChart';
import { FrameTreeIcicle } from './FrameTreeIcicle';
import { ANCESTOR_ROW_HEIGHT, MIN_ANCESTOR_ROWS, FrameTreeDetails } from './FrameTreeDetails';
import type { FrameTreeDetailsHandle } from './FrameTreeDetails';
import { WindowedList } from './WindowedList';
import { mergeChainPath, mergeSingleChildChains } from './mergeChains';
import { filterPythonFrames } from './filterPython';

const Explorer = styled.div`
    color: ${(p): string => p.theme.textColorPrimary};
    font-size: 12px;
    .frame-tree-toolbar, .frame-tree-path, .frame-tree-legend { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; margin: 12px 0; }
    .frame-tree-path { height: 36px; flex-wrap: nowrap; overflow: auto; padding: 8px; background: ${(p): string => p.theme.bgColorLight}; }
    .frame-tree-path button { flex: none; max-width: 240px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; padding: 0 3px; font: inherit; color: ${(p): string => p.theme.primaryColor}; background: none; border: 0; cursor: pointer; }
    .frame-tree-main { display: grid; grid-template-columns: minmax(0, 1fr) minmax(300px, 32%); gap: 16px; align-items: start; }
    .frame-tree-details { contain: size; align-self: stretch; min-height: 0; padding: 12px; overflow-wrap: anywhere; border: 1px solid ${(p): string => p.theme.borderColor}; overflow: auto; }
    .frame-tree-inspector { display: flex; flex-direction: column; height: 100%; min-height: 560px; gap: 8px; }
    .frame-tree-inspector-summary, .frame-tree-inspector-metrics, .frame-tree-inspector-footer { flex: none; }
    .frame-tree-inspector-summary h4 { margin: 4px 0; color: inherit; font-size: 13px; line-height: 1.4; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .frame-tree-inspector-size { font-size: 22px; font-weight: 600; font-variant-numeric: tabular-nums; }
    .frame-tree-inspector-size > span { margin-right: 8px; font-size: 12px; font-weight: 400; color: ${(p): string => p.theme.textColorSecondary}; }
    .frame-tree-inspector-ratios { display: flex; flex-wrap: wrap; gap: 2px 8px; margin: 3px 0 6px; font-size: 11px; color: ${(p): string => p.theme.textColorSecondary}; }
    .frame-tree-inspector-share { height: 3px; background: ${(p): string => p.theme.borderColorLight}; }
    .frame-tree-inspector-share span { display: block; height: 100%; background: ${(p): string => p.theme.primaryColor}; }
    .frame-tree-inspector-source { margin: 6px 0 0; color: ${(p): string => p.theme.textColorSecondary}; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .frame-tree-inspector-metrics { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 5px 8px; margin: 0; padding-bottom: 8px; border-bottom: 1px solid ${(p): string => p.theme.borderColorLight}; }
    .frame-tree-inspector-metrics dt { font-size: 11px; line-height: 16px; }
    .frame-tree-details dt { color: ${(p): string => p.theme.textColorSecondary}; }
    .frame-tree-details dd { margin: 2px 0 0; font-variant-numeric: tabular-nums; user-select: text; }
    .frame-tree-ancestor-section { display: flex; flex-direction: column; flex: 1; min-height: ${ANCESTOR_ROW_HEIGHT * MIN_ANCESTOR_ROWS + 24}px; }
    .frame-tree-inspector-section-label { display: flex; justify-content: space-between; padding-bottom: 6px; }
    .frame-tree-ancestors { flex: 1; min-height: ${ANCESTOR_ROW_HEIGHT * MIN_ANCESTOR_ROWS}px; overflow: auto; overscroll-behavior: contain; margin: 0; padding: 0; list-style: none; }
    .frame-tree-ancestors button { display: grid; grid-template-columns: 36px minmax(0, 1fr) auto; gap: 6px; width: max-content; min-width: 100%; height: ${ANCESTOR_ROW_HEIGHT}px; padding: 3px 4px; border: 0; border-left: 2px solid ${(p): string => p.theme.borderColorLight}; background: none; color: inherit; text-align: left; font: inherit; line-height: 18px; cursor: pointer; }
    .frame-tree-ancestors button > .frame-tree-muted { white-space: nowrap; }
    .frame-tree-ancestors button[data-merged="true"] { border-left-color: ${(p): string => p.theme.primaryColor}; }
    .frame-tree-ancestors button:hover, .frame-tree-ancestors button[aria-current="true"] { border-left-color: ${(p): string => p.theme.primaryColor}; background: ${(p): string => p.theme.bgColorLight}; }
    .frame-tree-ancestor-name { white-space: nowrap; user-select: text; }
    .frame-tree-inspector-footer { display: flex; flex-direction: column; gap: 8px; }
    .frame-tree-treemap-navigation { display: flex; align-items: center; gap: 8px; margin: 12px 0 8px; }
    .frame-tree-treemap-navigation .frame-tree-path { flex: 1; min-width: 0; margin: 0; }
    .frame-tree-treemap-navigation > strong { white-space: nowrap; }
    .frame-tree-results { margin: 8px 0; padding: 8px; border: 1px solid ${(p): string => p.theme.borderColor}; }
    .frame-tree-result { display: grid; width: 100%; height: 54px; text-align: left; padding: 6px; color: inherit; background: none; border: 0; border-bottom: 1px solid ${(p): string => p.theme.borderColorLight}; cursor: pointer; }
    .frame-tree-result span { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .frame-tree-muted { color: ${(p): string => p.theme.textColorSecondary}; }
    @media (max-width: 1000px) {
        .frame-tree-main { grid-template-columns: minmax(0, 1fr); }
        .frame-tree-details { contain: none; height: 640px; }
        .frame-tree-treemap-navigation { flex-wrap: wrap; }
        .frame-tree-treemap-navigation .frame-tree-path { flex-basis: 100%; order: 1; }
    }
`;

export function FrameTreeExplorer({ tree: sourceTree }: { tree: FrameTree }): JSX.Element {
    const { t } = useTranslation('leaks');
    const [pythonOnly, setPythonOnly] = useState(true);
    const tree = useMemo(() => pythonOnly ? filterPythonFrames(sourceTree) : sourceTree, [sourceTree, pythonOnly]);
    const name = (id: string): string => frameTreeNodeName(nodeAt(tree, id), t);
    const [focus, setFocus] = useState(tree.rootId);
    const [treemapFocus, setTreemapFocus] = useState(tree.rootId);
    const [selected, setSelected] = useState(tree.rootId);
    const [locked, setLocked] = useState(false);
    const [mergeSingles, setMergeSingles] = useState(true);
    const [selectedGroup, setSelectedGroup] = useState(false);
    const merged = useMemo(() => mergeSingles
        ? mergeSingleChildChains(tree, new Set([focus, treemapFocus]))
        : { tree, owners: new Map<string, string>() }, [tree, mergeSingles, focus, treemapFocus]);
    const viewTree = merged.tree;
    const displaySelected = merged.owners.get(selected) ?? selected;
    const inspector = useRef<FrameTreeDetailsHandle>(null);
    const preview = useCallback((id: string | null): void => inspector.current?.preview(id, id === null ? undefined : viewTree.nodes.get(id)), [viewTree]);
    useEffect(() => { inspector.current?.preview(null); }, [viewTree]);
    const [search, setSearch] = useState('');
    const [searchText, setSearchText] = useState('');
    const [notice, setNotice] = useState('');
    const [scrollRequest, setScrollRequest] = useState<{ id: string; token: number }>();
    const scrollToNode = (id: string): void => setScrollRequest(previous => ({ id, token: (previous?.token ?? 0) + 1 }));
    useEffect(() => {
        if (!search.trim()) { setSearchText(''); return; }
        const timer = setTimeout(() => setSearchText(search), 200);
        return () => clearTimeout(timer);
    }, [search]);
    const results = useMemo(() => searchFrameTree(tree, searchText), [tree, searchText]);
    const matches = useMemo(() => new Set(results.map(id => merged.owners.get(id) ?? id)), [results, merged]);
    const displayScroll = useMemo(() => scrollRequest && ({
        ...scrollRequest, id: merged.owners.get(scrollRequest.id) ?? scrollRequest.id,
    }), [scrollRequest, merged]);
    const focusPath = useMemo(() => {
        const path = pathToNode(tree, focus);
        return mergeSingles ? mergeChainPath(tree, path) : path.map(id => nodeAt(tree, id));
    }, [tree, focus, mergeSingles]);
    const treemapPath = useMemo(() => {
        const path = pathToNode(tree, treemapFocus);
        return mergeSingles ? mergeChainPath(tree, path) : path.map(id => nodeAt(tree, id));
    }, [tree, treemapFocus, mergeSingles]);
    const atRoot = focus === tree.rootId;
    const select = (id: string, group = false): void => { preview(null); setLocked(false); setSelected(id); setSelectedGroup(group); setNotice(''); };
    const selectBlock = (id: string): void => {
        const nextLock = !(locked && displaySelected === id);
        select(id, true);
        setLocked(nextLock);
    };
    const enter = (id: string): void => {
        select(id);
        if (nodeAt(tree, id).dto.childCount > 0) {
            setFocus(id);
            setTreemapFocus(id);
            scrollToNode(id);
        } else setNotice(t('frameTree.leaf'));
    };
    const enterCell = (id: string, lower = false): void => {
        const members = viewTree.nodes.get(id)?.merged?.ids;
        const tail = members?.[members.length - 1] ?? id;
        if (nodeAt(tree, tail).dto.childCount === 0) { select(id, true); setNotice(t('frameTree.leaf')); return; }
        select(tail);
        if (lower) setTreemapFocus(tail);
        else { setFocus(tail); setTreemapFocus(tail); scrollToNode(tail); }
    };
    const locate = (id: string): void => {
        const parentId = nodeAt(tree, id).dto.parentId ?? tree.rootId;
        setFocus(parentId);
        setTreemapFocus(parentId);
        select(id);
        scrollToNode(id);
    };
    const up = (): void => {
        preview(null); setLocked(false);
        const parentId = nodeAt(tree, focus).dto.parentId;
        if (parentId !== null) { setFocus(parentId); setTreemapFocus(parentId); scrollToNode(parentId); }
    };
    return <Explorer data-testid="frame-tree-ready" tabIndex={0} onKeyDown={(event): void => {
        const target = event.target as HTMLElement;
        if (event.key === 'Escape' && !target.closest('input,textarea,select,[contenteditable="true"],[role="combobox"],[role="dialog"]')) {
            event.preventDefault(); event.stopPropagation();
            if (target.closest('[data-testid="frame-tree-chart"],[data-testid="frame-tree-treemap-navigation"]')) {
                setLocked(false);
                setTreemapFocus(nodeAt(tree, treemapFocus).dto.parentId ?? tree.rootId);
            } else up();
        }
    }}>
        <div className="frame-tree-toolbar">
            <Input aria-label={t('frameTree.search')} placeholder={t('frameTree.search')} value={search} allowClear
                onChange={(event): void => setSearch(event.target.value)} style={{ width: 260, maxWidth: '100%' }} />
            <Checkbox checked={mergeSingles} onChange={(event): void => {
                preview(null); setLocked(false); setMergeSingles(event.target.checked); scrollToNode(selected);
            }}>{t('frameTree.mergeSingles')}</Checkbox>
            <Checkbox checked={pythonOnly} onChange={(event): void => {
                preview(null); setLocked(false); setPythonOnly(event.target.checked);
                setFocus(tree.rootId); setTreemapFocus(tree.rootId); select(tree.rootId); scrollToNode(tree.rootId);
            }}>{t('frameTree.pythonOnly')}</Checkbox>
            <Button disabled={atRoot} onClick={up}>{t('frameTree.up')}</Button>
            <Button disabled={atRoot} onClick={(): void => { setFocus(tree.rootId); setTreemapFocus(tree.rootId); select(tree.rootId); scrollToNode(tree.rootId); }}>{t('frameTree.root')}</Button>
        </div>
        {searchText.trim()
            ? <div className="frame-tree-results">
                <div role="status">{t('frameTree.matches', { count: results.length })}</div>
                {results.length === 0
                    ? <p>{t('frameTree.noMatches')}</p>
                    : <WindowedList items={results} height={162} rowHeight={54}
                        label={t('frameTree.searchResults')} renderRow={(id): JSX.Element => {
                            const node = nodeAt(tree, id);
                            const path = pathToNode(tree, id).map(name).join(' > ');
                            return <button key={id} type="button" className="frame-tree-result" data-testid={`frame-tree-search-${id}`} onClick={(): void => locate(id)}>
                                <span>{name(id)} · {formatExactBytes(node.metrics.sizeBytes)} · {node.dto.frame?.filename}:{node.dto.frame?.line ?? '\u2014'}</span>
                                <span className="frame-tree-muted">{path}</span>
                            </button>;
                        }} />}
            </div>
            : null}
        <nav className="frame-tree-path" aria-label={t('frameTree.path')}>
            {focusPath.map((node, index) => <React.Fragment key={node.dto.nodeId}>
                {index > 0 ? <span aria-hidden="true">&gt;</span> : null}
                <button type="button" onClick={(): void => {
                    const id = node.dto.nodeId;
                    setFocus(id); setTreemapFocus(id); select(id); scrollToNode(id);
                }}>{frameTreeNodeName(node, t)}</button>
            </React.Fragment>)}
        </nav>
        <div className="frame-tree-main">
            <FrameTreeIcicle tree={viewTree} focus={focus} selected={displaySelected} locked={locked} matches={matches} scrollRequest={displayScroll}
                onSelect={selectBlock} onEnter={(id): void => enterCell(id)} onPreview={preview} />
            <aside className="frame-tree-details" data-testid="frame-tree-details" aria-label={t('frameTree.details')}>
                <FrameTreeDetails ref={inspector} tree={tree} selected={selected} locked={locked}
                    displayNode={selectedGroup ? viewTree.nodes.get(displaySelected) : undefined} onSelect={select} onEnter={enter} />
            </aside>
        </div>
        <div className="frame-tree-legend">
            <span>{t('frameTree.kind.unknown')}</span>
            <span className="frame-tree-muted">{t('frameTree.chartHelp')}</span>
            {pythonOnly ? <span className="frame-tree-muted">{t('frameTree.pythonHelp')}</span> : null}
        </div>
        <div className="frame-tree-treemap-navigation" data-testid="frame-tree-treemap-navigation">
            <strong>{t('frameTree.treemapAll')}</strong>
            <nav className="frame-tree-path" aria-label={t('frameTree.treemapPath')}>
                {treemapPath.map((node, index) => <React.Fragment key={node.dto.nodeId}>
                    {index > 0 ? <span aria-hidden="true">&gt;</span> : null}
                    <button type="button" onClick={(): void => { setTreemapFocus(node.dto.nodeId); select(node.dto.nodeId); }}>{frameTreeNodeName(node, t)}</button>
                </React.Fragment>)}
            </nav>
            <Button disabled={treemapFocus === tree.rootId} onClick={(): void => {
                preview(null); setLocked(false); setTreemapFocus(nodeAt(tree, treemapFocus).dto.parentId ?? tree.rootId);
            }}>{t('frameTree.up')}</Button>
            <Button disabled={treemapFocus === tree.rootId} onClick={(): void => { setTreemapFocus(tree.rootId); select(tree.rootId); }}>{t('frameTree.root')}</Button>
        </div>
        <FrameTreeChart tree={viewTree} focus={treemapFocus} selected={displaySelected} locked={locked}
            onSelect={selectBlock} onEnter={(id): void => enterCell(id, true)} onPreview={preview} />
        {notice ? <p role="status">{notice}</p> : null}
    </Explorer>;
}

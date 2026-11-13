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
import React, { forwardRef, useImperativeHandle, useLayoutEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Button } from '@insight/lib/components';
import { METRIC_KEYS } from './types';
import type { FrameTree, FrameTreeNode } from './types';
import { exactRatio, formatExactBytes, nodeAt, pathToNode } from './model';
import { frameTreeNodeName } from './FrameTreeChart';

const percentage = (value: bigint, total: bigint): string => {
    const ratio = exactRatio(value, total);
    return ratio === null ? '\u2014' : `${(ratio * 100).toFixed(2)}%`;
};

export const ANCESTOR_ROW_HEIGHT = 24;
export const MIN_ANCESTOR_ROWS = 11;
export interface FrameTreeDetailsHandle {
    preview: (id: string | null, node?: FrameTreeNode) => void;
}
interface DetailsProps {
    tree: FrameTree;
    selected: string;
    locked?: boolean;
    displayNode?: FrameTreeNode;
    onSelect: (id: string) => void;
    onEnter: (id: string) => void;
}
export const FrameTreeDetails = forwardRef<FrameTreeDetailsHandle, DetailsProps>(function FrameTreeDetails(
    { tree, selected, locked = false, displayNode, onSelect, onEnter }, ref,
): JSX.Element {
    const { t } = useTranslation('leaks');
    const [preview, setPreview] = useState<{ id: string; node?: FrameTreeNode } | null>(null);
    useImperativeHandle(ref, () => ({
        preview: (id, node) => {
            if (!locked || id === null) setPreview(id === null ? null : { id, node });
        },
    }), [locked]);
    const current = preview?.id ?? selected;
    const ancestors = useRef<HTMLOListElement>(null);
    const original = nodeAt(tree, current);
    const node = preview ? preview.node ?? original : displayNode ?? original;
    const members = node.merged?.ids;
    const memberIds = useMemo(() => new Set(members), [members]);
    const last = members?.[members.length - 1] ?? current;
    const path = pathToNode(tree, last);
    const total = nodeAt(tree, tree.rootId).metrics.sizeBytes;
    const parent = tree.nodes.get(original.dto.parentId ?? '');
    const share = (exactRatio(node.metrics.sizeBytes, total) ?? 0) * 100;
    useLayoutEffect(() => {
        if (!ancestors.current) return;
        ancestors.current.scrollTop = ancestors.current.scrollHeight;
    }, [current, last]);
    useLayoutEffect(() => {
        const element = ancestors.current;
        if (!element || typeof ResizeObserver === 'undefined') return;
        const observer = new ResizeObserver(() => {
            element.scrollTop = element.scrollHeight;
        });
        observer.observe(element);
        return () => observer.disconnect();
    }, []);
    return <div className="frame-tree-inspector" data-node-id={current}>
        <header className="frame-tree-inspector-summary">
            <span className="frame-tree-muted">{t(preview === null ? 'frameTree.details' : 'frameTree.hoverPreview')} / {t(`frameTree.kind.${node.dto.kind}`)}
                {locked ? <>{' \u00b7 '}<span>{t('frameTree.locked')}</span></> : null}</span>
            <h4>{frameTreeNodeName(node, t)}</h4>
            <div className="frame-tree-inspector-size"><span>{t('frameTree.size')}</span>{formatExactBytes(node.metrics.sizeBytes)}</div>
            <div className="frame-tree-inspector-ratios">
                <span>{t('frameTree.totalRatio')} {percentage(node.metrics.sizeBytes, total)}</span>
                <span>{t('frameTree.parentRatio')} {percentage(node.metrics.sizeBytes, parent?.metrics.sizeBytes ?? node.metrics.sizeBytes)}</span>
            </div>
            <div className="frame-tree-inspector-share" role="meter" aria-label={t('frameTree.totalRatio')}
                aria-valuenow={share} aria-valuemin={0} aria-valuemax={100}><span style={{ width: `${share}%` }} /></div>
            {node.dto.frame && !members
                ? <p className="frame-tree-inspector-source">{node.dto.frame.filename || t('frameTree.unknownFile')}:{node.dto.frame.line ?? '\u2014'}</p>
                : null}
        </header>
        <dl className="frame-tree-inspector-metrics">
            {METRIC_KEYS.map(key => <div key={key}>
                <dt>{t(`frameTree.metric.${key}`)}</dt>
                <dd title={key.endsWith('Bytes') ? `${node.metrics[key].toString()} B` : undefined}>
                    {key === 'blockCount' || key === 'selfBlockCount' ? node.metrics[key].toString() : formatExactBytes(node.metrics[key])}
                </dd>
            </div>)}
        </dl>
        <div className="frame-tree-ancestor-section">
            <div className="frame-tree-inspector-section-label">{t('frameTree.path')} <span className="frame-tree-muted">L0 &gt; L{nodeAt(tree, last).dto.depth}</span></div>
            <ol ref={ancestors} className="frame-tree-ancestors" data-testid="frame-tree-ancestors" aria-label={t('frameTree.path')}>
                {path.map(id => {
                    const ancestor = nodeAt(tree, id);
                    const name = frameTreeNodeName(ancestor, t);
                    const source = ancestor.dto.frame ? ` ${ancestor.dto.frame.filename}:${ancestor.dto.frame.line ?? '\u2014'}` : '';
                    const text = `${name}${source}`;
                    return <li key={id}>
                        <button type="button" onClick={(): void => onSelect(id)} aria-current={id === last ? 'true' : undefined}
                            data-merged={memberIds.has(id)}
                            aria-label={`L${ancestor.dto.depth} ${text}`}>
                            <span className="frame-tree-muted">L{ancestor.dto.depth}</span>
                            <span className="frame-tree-ancestor-name" title={text}>{text}</span>
                            <span className="frame-tree-muted">{formatExactBytes(ancestor.metrics.sizeBytes)}</span>
                        </button>
                    </li>;
                })}
            </ol>
        </div>
        <footer className="frame-tree-inspector-footer">
            <Button disabled={nodeAt(tree, last).dto.childCount === 0} onClick={(): void => onEnter(last)}>{t('frameTree.enterSubtree')}</Button>
        </footer>
    </div>;
});

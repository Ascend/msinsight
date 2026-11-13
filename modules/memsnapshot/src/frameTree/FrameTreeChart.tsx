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
import { useTranslation } from 'react-i18next';
import { formatExactBytes, layoutTreemap, nodeAt } from './model';
import type { FrameTree, FrameTreeNode } from './types';
import { fitIcicleLabel, icicleTextColor, shadeIcicleColor } from './referenceIcicle';

const COLORS = ['#4C7DFF', '#2FA87C', '#D6A319', '#E45C91', '#2F9EAA', '#FF7A45', '#7A86D8'];
const Chart = styled.div`
    position: relative;
    height: 360px;
    min-height: 240px;
    max-height: 720px;
    resize: vertical;
    overflow: auto;
    overscroll-behavior: contain auto;
    border: 1px solid ${(p): string => p.theme.borderColor};
    background: ${(p): string => p.theme.bgColorLight};
    > svg { display: block; }
    [role="button"] { cursor: pointer; outline: none; }
    [role="button"]:focus-visible rect { stroke: ${(p): string => p.theme.textColorPrimary}; stroke-width: 3px; }
    [data-hovered="true"] > rect { stroke: ${(p): string => p.theme.textColorPrimary}; stroke-width: 2px; }
`;
export function frameTreeNodeName(node: FrameTreeNode, translate: (key: string) => string): string {
    if (node.merged) return `${node.merged.name || translate('frameTree.unknownFunction')} \u00d7${node.merged.ids.length}`;
    return node.dto.kind === 'frame'
        ? node.dto.frame?.name ? node.dto.frame.name : translate('frameTree.unknownFunction')
        : translate(`frameTree.kind.${node.dto.kind}`);
}

export function FrameTreeChart({ tree, focus, selected, locked = false, onSelect, onEnter, onPreview }: {
    tree: FrameTree;
    focus: string;
    selected: string;
    locked?: boolean;
    onSelect: (id: string) => void;
    onEnter: (id: string) => void;
    onPreview?: (id: string) => void;
}): JSX.Element {
    const { t } = useTranslation('leaks');
    const ref = useRef<HTMLDivElement>(null);
    const hovered = useRef<SVGGElement>();
    const [size, setSize] = useState({ width: 1000, height: 360 });
    useEffect(() => {
        const container = ref.current;
        if (!container) return;
        const measure = (): void => {
            const width = container.clientWidth;
            const height = container.clientHeight;
            if (width > 0 && height > 0) setSize(previous => previous.width === width && previous.height === height ? previous : { width, height });
        };
        measure();
        const observer = typeof ResizeObserver === 'undefined' ? undefined : new ResizeObserver(measure);
        observer?.observe(container);
        window.addEventListener('resize', measure);
        return () => { observer?.disconnect(); window.removeEventListener('resize', measure); };
    }, []);
    const layout = useMemo(() => layoutTreemap(tree, focus, Infinity, size), [tree, focus, size]);
    const { cells } = layout;
    const clearHover = useCallback((): void => {
        hovered.current?.removeAttribute('data-hovered');
        hovered.current = undefined;
    }, []);
    useEffect(() => { clearHover(); return clearHover; }, [focus, cells, locked, selected, clearHover]);
    const colors = useMemo(() => {
        const result = new Map<string, string>();
        const pending = [...(tree.children.get(tree.rootId) ?? [])].map((id, index) => ({ id, color: COLORS[index % COLORS.length] }));
        while (pending.length) {
            const next = pending.pop();
            if (!next) break;
            const { id, color } = next;
            result.set(id, color);
            for (const child of tree.children.get(id) ?? []) pending.push({ id: child, color });
        }
        result.set(tree.rootId, COLORS[0]);
        return result;
    }, [tree]);
    const total = nodeAt(tree, focus).metrics.sizeBytes;
    return <Chart ref={ref} data-testid="frame-tree-chart" data-focus={focus} data-locked={locked} data-view="treemap" data-layout-width={size.width} onMouseLeave={clearHover}>
        {total === 0n
            ? <p role="status">{t('frameTree.noArea')}</p>
            : <svg viewBox={`0 0 ${layout.width} ${layout.height}`} width={layout.width} height={layout.height} preserveAspectRatio="none"
                aria-label={t('frameTree.treemap')}>
                {cells.map(cell => {
                    const node = nodeAt(tree, cell.nodeId);
                    const name = `${frameTreeNodeName(node, t)}${cell.self ? ` / ${t('frameTree.self')}` : ''}`;
                    const width = cell.width * layout.width;
                    const height = cell.height * layout.height;
                    // Single-child pass-through frames keep their full paths without covering identical rectangles.
                    if (width < 0.4 || height < 0.4 || (!cell.self && cell.nodeId !== focus &&
                        node.dto.childCount === 1 && node.metrics.selfBytes === 0n)) return null;
                    const isBoundary = cell.self || cell.truncated || node.dto.childCount === 0;
                    const value = cell.self ? node.metrics.selfBytes : node.metrics.sizeBytes;
                    const baseColor = colors.get(cell.nodeId) ?? COLORS[0];
                    const color = cell.self ? shadeIcicleColor(baseColor, 0.2) : baseColor;
                    const textColor = icicleTextColor(color);
                    const labelHeight = isBoundary ? height : Math.min(18, height * 0.12);
                    return <g key={`${cell.nodeId}-${cell.self ? 'self' : 'node'}`} role="button" tabIndex={0} aria-label={`${name}: ${formatExactBytes(value)}`}
                        aria-pressed={selected === cell.nodeId} data-locked={locked && selected === cell.nodeId}
                        data-node-id={cell.nodeId} data-child-count={cell.self ? 0 : node.dto.childCount}
                        onMouseEnter={(event): void => {
                            if (locked) return;
                            clearHover();
                            hovered.current = event.currentTarget;
                            hovered.current.setAttribute('data-hovered', 'true');
                            onPreview?.(cell.nodeId);
                        }}
                        onClick={(): void => onSelect(cell.nodeId)} onDoubleClick={(): void => onEnter(cell.nodeId)}
                        onKeyDown={(event): void => {
                            if (event.key === 'Enter') { event.preventDefault(); onEnter(cell.nodeId); } else if (event.key === ' ') { event.preventDefault(); onSelect(cell.nodeId); }
                        }}>
                        <rect x={cell.x * layout.width} y={cell.y * layout.height} width={width} height={height}
                            fill={color}
                            stroke={selected === cell.nodeId ? '#ffffff' : 'rgba(255,255,255,.4)'}
                            strokeWidth={selected === cell.nodeId ? 3 : 0.6} vectorEffect="non-scaling-stroke" />
                        {width > 80 && labelHeight > 14
                            ? <svg x={cell.x * layout.width + 5} y={cell.y * layout.height + 2} width={Math.max(0, width - 10)} height={Math.max(0, labelHeight - 2)} overflow="hidden" pointerEvents="none">
                                <text x={0} y={12} fill={textColor} fontSize={12}>{fitIcicleLabel(name, (width - 10) / 12)}</text>
                                {isBoundary && height > 45 ? <text x={0} y={29} fill={textColor} fontSize={11}>{formatExactBytes(value)}</text> : null}
                                {cell.truncated && height > 62
                                    ? <text x={0} y={46} fill={textColor} fontSize={10}>
                                        {t('frameTree.moreChildren', { count: node.dto.childCount })}
                                    </text>
                                    : null}
                            </svg>
                            : null}
                    </g>;
                })}
                {cells.filter(cell => cell.nodeId === selected && !cell.self).map(cell =>
                    <rect key={`selected-${cell.nodeId}`} x={cell.x * layout.width} y={cell.y * layout.height} width={cell.width * layout.width}
                        height={cell.height * layout.height} fill="none" stroke="#ffffff" strokeWidth={3}
                        vectorEffect="non-scaling-stroke" pointerEvents="none" />)}
            </svg>}
    </Chart>;
}

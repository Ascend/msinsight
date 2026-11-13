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
import { useTheme } from '@emotion/react';
import styled from '@emotion/styled';
import { useTranslation } from 'react-i18next';
import type { FrameTree } from './types';
import { formatExactBytes, nodeAt, pathToNode } from './model';
import { frameTreeNodeName } from './FrameTreeChart';
import { ICICLE_STYLE, fitIcicleLabel, icicleColors, icicleTextColor, icicleTicks, layoutReferenceIcicle, shadeIcicleColor } from './referenceIcicle';
import { ICICLE_CELL_GAP, MIN_FRAME_SIZE } from './minimumSize';

const Icicle = styled.div`
    min-width: 0;
    border: 1px solid ${(p): string => p.theme.borderColor};
    background: ${(p): string => p.theme.bgColor ?? p.theme.bgColorLight};
    background-image: none;
    color: ${(p): string => p.theme.textColorPrimary};
    font-size: 12px;
    .icicle-stage { position: relative; width: 100%; height: clamp(600px, 75vh, 900px); overflow: auto; overscroll-behavior: contain auto; }
    .icicle-stage > svg { display: block; min-width: 100%; height: auto; }
    .icicle-cell { cursor: pointer; outline: none; }
    .icicle-cell rect { stroke: ${(p): string => p.theme.borderColorLight}; stroke-width: .8px; }
    .icicle-cell[data-selected="true"] rect, .icicle-cell[data-match="true"] rect {
        stroke: ${(p): string => p.theme.primaryColor}; stroke-width: 2px;
    }
    svg[data-hovering="true"] .icicle-cell { opacity: .28; }
    svg[data-hovering="true"] .icicle-cell[data-call-path="true"] { opacity: 1; }
    .icicle-cell[data-call-path="true"] rect { stroke: ${(p): string => p.theme.textColorPrimary}; stroke-width: 1px; }
    .icicle-cell[data-hovered="true"] rect, .icicle-cell:focus-visible rect {
        stroke: ${(p): string => p.theme.textColorPrimary}; stroke-width: 2px;
    }
`;

export function FrameTreeIcicle({ tree, focus, selected, locked = false, matches, scrollRequest, onSelect, onEnter, onPreview }: {
    tree: FrameTree;
    focus: string;
    selected: string;
    locked?: boolean;
    matches: Set<string>;
    scrollRequest?: { id: string; token: number };
    onSelect: (id: string) => void;
    onEnter: (id: string) => void;
    onPreview?: (id: string) => void;
}): JSX.Element {
    const { t } = useTranslation('leaks');
    const theme = useTheme();
    const container = useRef<HTMLDivElement>(null);
    const plot = useRef<SVGSVGElement>(null);
    const elements = useRef(new Map<string, SVGGElement>());
    const selfElements = useRef(new Map<string, SVGGElement>());
    const activePath = useRef(new Set<string>());
    const activeCell = useRef<SVGGElement>();
    const [width, setWidth] = useState(1000);
    useEffect(() => {
        const host = container.current;
        if (!host) return;
        const measure = (): void => { if (host.clientWidth > 0) setWidth(Math.max(560, host.clientWidth)); };
        measure();
        const observer = typeof ResizeObserver === 'undefined' ? undefined : new ResizeObserver(measure);
        observer?.observe(host);
        window.addEventListener('resize', measure);
        return () => { observer?.disconnect(); window.removeEventListener('resize', measure); };
    }, []);
    const rootLayout = useMemo(() => layoutReferenceIcicle(tree, tree.rootId, width), [tree, width]);
    const layout = useMemo(() => focus === tree.rootId ? rootLayout : layoutReferenceIcicle(tree, focus, width), [tree, focus, width, rootLayout]);
    const colors = useMemo(() => {
        const branches = [theme.colorPalette?.royalblue, theme.colorPalette?.turquoise, theme.colorPalette?.coral,
            theme.colorPalette?.gold, theme.colorPalette?.palevioletred].filter((color): color is string => typeof color === 'string');
        return icicleColors(tree, {
            root: theme.bgColorLight,
            branches: branches.length ? branches : [theme.primaryColor],
            synthetic: theme.bgColorGrey ?? theme.borderColor,
        });
    }, [tree, theme]);
    const total = nodeAt(tree, focus).metrics.sizeBytes;
    const ticks = useMemo(() => icicleTicks(total), [total]);
    const focusDepth = nodeAt(tree, focus).dto.depth;
    const clearHover = useCallback((): void => {
        for (const id of activePath.current) {
            elements.current.get(id)?.removeAttribute('data-call-path');
            selfElements.current.get(id)?.removeAttribute('data-call-path');
        }
        activePath.current.clear();
        activeCell.current?.removeAttribute('data-hovered');
        activeCell.current = undefined;
        plot.current?.removeAttribute('data-hovering');
    }, []);
    useEffect(() => {
        if (!scrollRequest) return;
        const element = elements.current.get(scrollRequest.id);
        const viewport = container.current?.querySelector('.icicle-stage');
        if (!element || !viewport) return;
        if (scrollRequest.id === focus) {
            viewport.scrollTop = 0;
            viewport.scrollLeft = 0;
            return;
        }
        const cell = element.getBoundingClientRect();
        const bounds = viewport.getBoundingClientRect();
        if (cell.top < bounds.top) viewport.scrollTop += cell.top - bounds.top;
        else if (cell.bottom > bounds.bottom) viewport.scrollTop += cell.bottom - bounds.bottom;
        if (cell.left < bounds.left) viewport.scrollLeft += cell.left - bounds.left;
        else if (cell.right > bounds.right) viewport.scrollLeft += cell.right - bounds.right;
    }, [scrollRequest, layout, focus]);
    const showHover = useCallback((id: string, self: boolean): void => {
        const path = pathToNode(tree, id);
        const nextPath = new Set(path);
        // Pointer updates touch only the old/new call chains, never the React chart tree.
        for (const previous of activePath.current) {
            if (nextPath.has(previous)) continue;
            elements.current.get(previous)?.removeAttribute('data-call-path');
            selfElements.current.get(previous)?.removeAttribute('data-call-path');
        }
        for (const current of nextPath) {
            if (activePath.current.has(current)) continue;
            elements.current.get(current)?.setAttribute('data-call-path', 'true');
            selfElements.current.get(current)?.setAttribute('data-call-path', 'true');
        }
        activePath.current = nextPath;
        activeCell.current?.removeAttribute('data-hovered');
        activeCell.current = (self ? selfElements : elements).current.get(id);
        activeCell.current?.setAttribute('data-hovered', 'true');
        if (plot.current?.getAttribute('data-hovering') !== 'true') plot.current?.setAttribute('data-hovering', 'true');
        onPreview?.(id);
    }, [tree, onPreview]);
    useEffect(() => {
        const self = activeCell.current !== undefined && activeCell.current === selfElements.current.get(selected);
        clearHover();
        if (locked) showHover(selected, self);
        return clearHover;
    }, [tree, layout, t, locked, selected, clearHover, showHover]);
    const { rowHeight, gutter, top, bottom } = ICICLE_STYLE;
    return <Icicle ref={container} data-testid="frame-tree-icicle" data-focus={focus} data-locked={locked} data-view="icicle" data-theme={theme.mode}>
        <div className="icicle-stage" style={{ maxHeight: Math.max(600, rootLayout.height) }}>
            {total === 0n
                ? <p role="status">{t('frameTree.noArea')}</p>
                : <svg ref={plot}
                    viewBox={`0 0 ${layout.width} ${layout.height}`} width={layout.width} height={layout.height}
                    aria-label={t('frameTree.icicle')} onMouseLeave={(): void => { if (!locked) clearHover(); }} onMouseMove={(event): void => {
                        if (!locked && (!(event.target instanceof Element) || !event.target.closest('.icicle-cell'))) clearHover();
                    }}>
                    <rect x={0} y={0} width={layout.width} height={layout.height} fill="transparent" />
                    {Array.from({ length: layout.levels }, (_, row) => {
                        const y = top + row * rowHeight;
                        return <g key={`level-${row}`} aria-hidden="true">
                            {row % 2 === 0 ? <rect x={0} y={y} width={layout.width} height={rowHeight} fill={theme.bgColorLight} opacity={0.35} /> : null}
                            <line x1={0} y1={y} x2={layout.width} y2={y} stroke={theme.borderColorLight} strokeWidth={1} opacity={0.5} />
                            <text x={6} y={y + rowHeight * 0.5 + 3.2} fill={theme.textColorSecondary} fontSize={10}>{`L${focusDepth + row}`}</text>
                        </g>;
                    })}
                    <line x1={gutter} y1={top} x2={gutter} y2={layout.height - bottom} stroke={theme.borderColor} strokeWidth={1} />
                    {ticks.map(tick => {
                        const x = gutter + tick.ratio * (layout.width - gutter);
                        return <g key={tick.label} aria-hidden="true">
                            <line x1={x} y1={top - 8} x2={x} y2={layout.height - bottom + 4} stroke={theme.borderColorLight} strokeWidth={1} opacity={0.5} />
                            <text x={x} y={top - 12} fill={theme.textColorSecondary} fontSize={10.5} textAnchor={tick.ratio === 0 ? 'start' : 'middle'}>{`\u2248${tick.label}`}</text>
                        </g>;
                    })}
                    {layout.cells.map(cell => {
                        const node = nodeAt(tree, cell.nodeId);
                        const name = `${frameTreeNodeName(node, t).replace(/^\u21b3 /, '')}${cell.self ? ` / ${t('frameTree.self')}` : ''}`;
                        const color = colors.get(cell.nodeId) ?? theme.bgColorLight;
                        const fill = cell.self ? shadeIcicleColor(color, 0.25) : color;
                        const text = node.dto.kind === 'root' ? theme.textColorPrimary : icicleTextColor(fill);
                        const fontSize = cell.depth <= 2 ? 12 : 10.5;
                        const bytes = cell.self ? node.metrics.selfBytes : node.metrics.sizeBytes;
                        const value = formatExactBytes(bytes);
                        const showValue = cell.width > 140 && (node.dto.kind === 'root' || node.dto.depth === 1) && !cell.self;
                        const maxUnits = (cell.width - 8 - (showValue ? value.length * 9.5 * 0.62 + 12 : 0)) / fontSize;
                        const suffix = node.merged ? ` \u00d7${node.merged.ids.length}` : '';
                        const countUnits = suffix.length * 0.58;
                        const label = suffix
                            ? ((maxUnits > countUnits + 1 ? fitIcicleLabel(name.replace(suffix, ''), maxUnits - countUnits) : '') + suffix).trim()
                            : fitIcicleLabel(name, maxUnits);
                        return <g key={`${cell.nodeId}-${cell.self ? 'self' : 'node'}`} className="icicle-cell" role="button" tabIndex={0}
                            ref={(element): void => {
                                const map = (cell.self ? selfElements : elements).current;
                                if (element) map.set(cell.nodeId, element); else map.delete(cell.nodeId);
                            }}
                            data-testid={cell.self ? undefined : `frame-tree-icicle-${cell.nodeId}`}
                            data-node-id={cell.nodeId} data-parent-id={node.dto.parentId ?? ''} data-depth={cell.depth}
                            data-merged-count={!cell.self ? node.merged?.ids.length : undefined}
                            data-selected={selected === cell.nodeId} data-match={matches.has(cell.nodeId)}
                            data-locked={locked && selected === cell.nodeId}
                            aria-label={`${name}: ${value}`} aria-pressed={selected === cell.nodeId}
                            onMouseEnter={(): void => { if (!locked) showHover(cell.nodeId, cell.self); }}
                            onClick={(): void => { showHover(cell.nodeId, cell.self); onSelect(cell.nodeId); }} onDoubleClick={(): void => onEnter(cell.nodeId)}
                            onKeyDown={(event): void => {
                                if (event.key === 'Enter') { event.preventDefault(); onEnter(cell.nodeId); } else if (event.key === ' ') {
                                    event.preventDefault(); showHover(cell.nodeId, cell.self); onSelect(cell.nodeId);
                                }
                            }}>
                            <rect x={cell.x} y={cell.y} width={Math.max(MIN_FRAME_SIZE, cell.width - ICICLE_CELL_GAP)} height={cell.height} fill={fill} />
                            {cell.width > 24 && maxUnits > 2
                                ? <text x={cell.x + 4} y={cell.y + cell.height * 0.5 + 3.6} fill={text}
                                    fontSize={fontSize} fontWeight={cell.depth <= 2 ? 600 : 400} pointerEvents="none">{label}</text>
                                : null}
                            {showValue
                                ? <text x={cell.x + cell.width - 5} y={cell.y + cell.height * 0.5 + 3.4} fill={text} fontSize={9.5}
                                    opacity={0.9} textAnchor="end" pointerEvents="none">{value}</text>
                                : null}
                        </g>;
                    })}
                </svg>}
        </div>
    </Icicle>;
}

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
import type { FrameTree } from './types';
import { childIds, exactRatio, nodeAt } from './model';
import { allocateMinimumSizes, ICICLE_CELL_GAP, MIN_FRAME_SIZE } from './minimumSize';

// Keep the compact, full-stack geometry; visual colors come from the host theme.
export const ICICLE_STYLE = {
    rowHeight: 21,
    gutter: 42,
    top: 26,
    bottom: 24,
};

export interface ReferenceIcicleCell {
    nodeId: string;
    x: number;
    y: number;
    width: number;
    height: number;
    row: number;
    depth: number;
    self: boolean;
}

export function layoutReferenceIcicle(tree: FrameTree, focus: string, availableWidth: number): {
    width: number; height: number; levels: number; cells: ReferenceIcicleCell[];
} {
    const { rowHeight, gutter, top, bottom } = ICICLE_STYLE;
    const minimums = new Map<string, number>();
    const ordered = [focus];
    for (let index = 0; index < ordered.length; index++) {
        for (const child of childIds(tree, ordered[index])) ordered.push(child);
    }
    for (let index = ordered.length - 1; index >= 0; index--) {
        const id = ordered[index];
        const node = nodeAt(tree, id);
        const childrenWidth = childIds(tree, id).reduce((sum, child) =>
            sum + (nodeAt(tree, child).metrics.sizeBytes > 0n ? minimums.get(child) ?? 0 : 0), 0);
        minimums.set(id, Math.max(MIN_FRAME_SIZE + ICICLE_CELL_GAP,
            childrenWidth + (node.metrics.selfBytes > 0n && node.dto.childCount > 0 ? MIN_FRAME_SIZE + ICICLE_CELL_GAP : 0)));
    }
    const width = Math.max(560, Number.isFinite(availableWidth) ? availableWidth : 560, gutter + (minimums.get(focus) ?? 0));
    const cells: ReferenceIcicleCell[] = [];
    const pending: Array<{ id: string; x: number; width: number; row: number; self: boolean }> = [{
        id: focus, x: gutter, width: width - gutter, row: 0, self: false,
    }];
    let levels = 1;
    const baseDepth = nodeAt(tree, focus).dto.depth;
    while (pending.length) {
        const entry = pending.pop();
        if (!entry) break;
        const node = nodeAt(tree, entry.id);
        levels = Math.max(levels, entry.row + 1);
        cells.push({
            nodeId: entry.id,
            x: entry.x,
            y: top + entry.row * rowHeight,
            width: entry.width,
            height: rowHeight - 2,
            row: entry.row,
            depth: baseDepth + entry.row,
            self: entry.self,
        });
        if (entry.self || node.metrics.sizeBytes === 0n) continue;
        const children = childIds(tree, entry.id);
        if (children.length === 0) continue;
        const parts = children.filter(id => nodeAt(tree, id).metrics.sizeBytes > 0n)
            .map(id => ({ id, value: nodeAt(tree, id).metrics.sizeBytes, self: false }));
        if (node.metrics.selfBytes > 0n) {
            const tailIndex = parts.findIndex(part => ['static', 'unknown'].includes(nodeAt(tree, part.id).dto.kind));
            parts.splice(tailIndex < 0 ? parts.length : tailIndex, 0, { id: entry.id, value: node.metrics.selfBytes, self: true });
        }
        const sizes = allocateMinimumSizes(parts.map(part => ({
            value: Number(part.value) / Number(node.metrics.sizeBytes),
            minimum: part.self ? MIN_FRAME_SIZE + ICICLE_CELL_GAP : minimums.get(part.id) ?? MIN_FRAME_SIZE + ICICLE_CELL_GAP,
        })), entry.width);
        let offset = 0;
        const next: typeof pending = [];
        for (const [index, part] of parts.entries()) {
            next.push({
                id: part.id,
                x: entry.x + offset,
                width: sizes[index],
                row: entry.row + 1,
                self: part.self,
            });
            offset += sizes[index];
        }
        for (let index = next.length - 1; index >= 0; index--) pending.push(next[index]);
    }
    return { width, height: top + bottom + levels * rowHeight, levels, cells };
}

export function shadeIcicleColor(hex: string, amount: number): string {
    return '#' + [1, 3, 5].map(offset =>
        Math.max(0, Math.min(255, Math.round(parseInt(hex.slice(offset, offset + 2), 16) * (1 + amount))))
            .toString(16).padStart(2, '0')).join('');
}

export function icicleColors(tree: FrameTree, palette: { root: string; branches: readonly string[]; synthetic: string }): Map<string, string> {
    const colors = new Map([[tree.rootId, palette.root]]);
    childIds(tree, tree.rootId).forEach((id, index) => {
        const base = nodeAt(tree, id).dto.kind === 'frame'
            ? palette.branches[index % palette.branches.length] ?? palette.synthetic
            : palette.synthetic;
        colors.set(id, base);
        const children = childIds(tree, id);
        const pending = children.map((child, rank) => ({
            id: child, color: shadeIcicleColor(base, (rank / Math.max(1, children.length - 1) - 0.5) * 0.3),
        }));
        while (pending.length) {
            const entry = pending.pop();
            if (!entry) break;
            colors.set(entry.id, entry.color);
            for (const child of childIds(tree, entry.id)) pending.push({ id: child, color: entry.color });
        }
    });
    return colors;
}

export function icicleTextColor(hex: string): string {
    const [r, g, b] = [1, 3, 5].map(offset => {
        const value = parseInt(hex.slice(offset, offset + 2), 16) / 255;
        return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4;
    });
    return 0.2126 * r + 0.7152 * g + 0.0722 * b > 0.179 ? '#111111' : '#ffffff';
}

const characterUnits = (character: string): number => /[\u3000-\u9fff\uff00-\uffef]/.test(character) ? 1 : 0.58;
export function fitIcicleLabel(text: string, maxUnits: number): string {
    if ([...text].reduce((sum, character) => sum + characterUnits(character), 0) <= maxUnits) return text;
    let output = '';
    let units = 0;
    for (const character of text) {
        const next = characterUnits(character);
        if (units + next > maxUnits - 1) break;
        output += character;
        units += next;
    }
    return output + '\u2026';
}

export function icicleTicks(total: bigint): Array<{ ratio: number; label: string }> {
    if (total <= 0n) return [];
    const units: Array<[bigint, string]> = [[1073741824n, 'GiB'], [1048576n, 'MiB'], [1024n, 'KiB'], [1n, 'B']];
    const [unit, name] = units.find(([value]) => total >= value) ?? [1n, 'B'];
    const target = total / unit / 5n;
    let power = 1n;
    while (power * 10n <= target) power *= 10n;
    const step = ([1n, 2n, 5n, 10n].find(value => value * power >= target) ?? 10n) * power * unit;
    const ticks: Array<{ ratio: number; label: string }> = [];
    for (let value = 0n; value <= total; value += step) {
        ticks.push({ ratio: exactRatio(value, total) ?? 0, label: `${value / unit} ${name}` });
    }
    return ticks;
}

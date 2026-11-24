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
import type { MatrixEdge } from '../../domain/types';
export type Metric = 'bandwidth' | 'transitSize' | 'transitTime' | 'transportType';
export const METRICS: Metric[] = ['bandwidth', 'transitSize', 'transportType', 'transitTime'];
export const UNITS: Partial<Record<Metric, string>> = { bandwidth: '(GB/s)', transitSize: '(MB)', transitTime: '(ms)' };
export type ValueRange = [number | null, number | null];
// These are semantic heatmap bands from the existing communication view, not a decorative palette.
export const BANDS = ['#f82d18', '#eac299', '#c7eef5', '#0177ff', '#48AA82', '#9254de'];
export const LINK_TYPES = ['HCCS', 'PCIE', 'RDMA', 'LOCAL', 'SIO', 'UB'];
export interface Cell { value: [number, number, number]; edges: MatrixEdge[] }
export function matrixRanks(edges: MatrixEdge[], stage: string): number[] {
    return [...new Set(stage === 'p2p'
        ? edges.flatMap(edge => [edge.srcRank, edge.dstRank])
        : stage.replace(/[()[\]]/g, '').split(',').map(value => value.trim()).filter(Boolean).map(Number).filter(Number.isFinite))].sort((a, b) => a - b);
}
export function matrixCells(edges: MatrixEdge[], ranks: number[], metric: Metric, inner: boolean, range?: ValueRange): Cell[] {
    const positions = new Map(ranks.map((rank, index) => [rank, index]));
    const cells = new Map<string, Cell>();
    for (const edge of edges) {
        if (!positions.has(edge.srcRank) || !positions.has(edge.dstRank) || (!inner && edge.srcRank === edge.dstRank)) { continue; }
        const value = metric === 'transportType' ? LINK_TYPES.indexOf(edge.transportType) : edge[metric];
        if (metric !== 'transportType' && range && ((range[0] !== null && value < range[0]) || (range[1] !== null && value > range[1]))) { continue; }
        const key = `${edge.srcRank}:${edge.dstRank}`; const existing = cells.get(key);
        if (existing) { existing.edges.unshift(edge); } else { cells.set(key, { value: [positions.get(edge.srcRank) as number, positions.get(edge.dstRank) as number, value], edges: [edge] }); }
    }
    return [...cells.values()];
}
export function matrixExtent(cells: Cell[]): [number, number] {
    let min = Infinity; let max = -Infinity;
    for (const cell of cells) { min = Math.min(min, cell.value[2]); max = Math.max(max, cell.value[2]); }
    return cells.length ? [min, max] : [0, 0];
}
export function cellLabel(cell: Cell, metric: Metric): string {
    if (metric === 'transportType') { return LINK_TYPES[cell.value[2]] ?? ''; }
    const values = cell.edges.map(edge => edge[metric]);
    return values.length > 1 ? `[${values.join(',')}]` : String(values[0]);
}
export function decimalPlaces(value: number): number {
    const [coefficient, exponent = '0'] = String(value).split('e');
    return Math.max(0, (coefficient.split('.')[1]?.length ?? 0) - Number(exponent));
}

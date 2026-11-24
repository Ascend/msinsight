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
import { cellLabel, matrixCells, matrixExtent, matrixRanks, LINK_TYPES } from '../features/matrix/data';
import { operatorColor } from '../features/operatorTimeline/colors';
const edges = [
    { srcRank: 1, dstRank: 2, bandwidth: 1.123456, transitSize: 3, transitTime: 5, transportType: 'RDMA', opName: 'op' },
    { srcRank: 1, dstRank: 2, bandwidth: 9, transitSize: 6, transitTime: 8, transportType: 'PCIE', opName: 'op' },
    { srcRank: 1, dstRank: 1, bandwidth: 100, transitSize: 2, transitTime: 1, transportType: 'LOCAL', opName: 'op' },
];
it('keeps empty group ranks and first colour value with reverse duplicate details', () => {
    const ranks = matrixRanks(edges, '(3, 1, 2)'); expect(ranks).toEqual([1, 2, 3]);
    const cells = matrixCells(edges, ranks, 'bandwidth', false);
    expect(cells).toHaveLength(1); expect(cells[0].value).toEqual([0, 1, 1.123456]);
    expect(cellLabel(cells[0], 'bandwidth')).toBe('[9,1.123456]'); expect(matrixExtent(cells)).toEqual([1.123456, 1.123456]);
    expect(matrixCells(edges, ranks, 'bandwidth', true)).toHaveLength(2);
});
it('uses inclusive filters and a stable transport mapping', () => {
    const ranks = matrixRanks(edges, 'p2p'); expect(ranks).toEqual([1, 2]);
    expect(matrixCells(edges, ranks, 'bandwidth', false, [9, 9])[0].value[2]).toBe(9);
    expect(matrixCells(edges, ranks, 'transportType', false)[0].value[2]).toBe(LINK_TYPES.indexOf('RDMA'));
});
it('uses the existing deterministic operator palette hash', () => {
    expect(operatorColor('allReduce')).toBe(operatorColor('Reduceall'));
    expect(operatorColor('a')).toBe('royalPurple');
});

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
import { buildCommunicationWebGLIndex, selectVisibleCommunicationOperators, hitTestCommunicationOperator, findCommunicationOperator } from '../dataIndex';

const data = {
    minTime: 0,
    maxTime: 10000000,
    rows: Array.from({ length: 10000 }, (_, id) => ({
        rankId: String(id),
        dbPath: `${id}.db`,
        operators: [{ operatorName: 'allReduce', startTime: 1000000, duration: 5000000 }],
    })),
};
it('indexes every rank and limits selection to the visible rank/time window', () => {
    const index = buildCommunicationWebGLIndex(data);
    expect(index.ranks).toHaveLength(10000);
    expect(selectVisibleCommunicationOperators(index, { xMin: 0, xMax: 10, yStartIndex: 0, yEndIndex: 9999 })).toHaveLength(10000);
    expect(selectVisibleCommunicationOperators(index, { xMin: 0, xMax: 10, yStartIndex: 50, yEndIndex: 59 })).toHaveLength(12);
    expect(selectVisibleCommunicationOperators(index, { xMin: 8, xMax: 10, yStartIndex: 50, yEndIndex: 59 })).toHaveLength(0);
    const op = findCommunicationOperator(index, '9999', 'allReduce', 1);
    expect(op?.original).toBe(data.rows[9999].operators[0]);
    expect(index.ranks[op?.rankIndex ?? -1].row).toBe(data.rows[9999]);
});
it('finds long overlapping operators and sorts out-of-order input without changing source values', () => {
    const operators = [
        { operatorName: 'short', startTime: 4000000, duration: 1000000 },
        { operatorName: '<long>', startTime: 0, duration: 10000000 },
    ];
    const index = buildCommunicationWebGLIndex({ minTime: 0, maxTime: 10000000, rows: [{ rankId: '0', dbPath: 'r.db', operators }] });
    expect(hitTestCommunicationOperator(index, 0, 8)?.operatorName).toBe('<long>');
    expect(hitTestCommunicationOperator(index, 0, 4)?.operatorName).toBe('short');
    expect(operators[0].operatorName).toBe('short');
});

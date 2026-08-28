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
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import type { AnalysisChartData, OperatorTimeItem } from '../communication/CommunicationTimeAnalysisChart';
import {
    buildCommunicationWebGLIndex,
    CommunicationOperatorSource,
    findCommunicationOperator,
    hitTestCommunicationOperator,
    selectVisibleCommunicationOperators,
} from '../communication/communicationTimeWebglData';

const MS_TO_NS = 1000000;

const createOperator = (operatorName: string, startTime: number, duration: number = 1): OperatorTimeItem => ({
    operatorName,
    startTime: startTime * MS_TO_NS,
    duration: duration * MS_TO_NS,
});

const createDataSource = (
    compare: OperatorTimeItem[][],
    baseline: OperatorTimeItem[][] = [],
): AnalysisChartData => ({
    minTime: 0,
    maxTime: 100 * MS_TO_NS,
    data: compare.map((items, rankId) => ({
        rankId: `${rankId}`,
        dbPath: `rank-${rankId}.db`,
        lists: {
            compare: items,
            baseline: baseline[rankId] ?? [],
            diff: [],
        },
    })),
});

it('selects only operators overlapping the current WebGL viewport', () => {
    const index = buildCommunicationWebGLIndex(createDataSource([
        [createOperator('rank-0-visible', 5), createOperator('rank-0-hidden', 80)],
        [createOperator('rank-1', 5)],
        [createOperator('rank-2', 5)],
        [createOperator('rank-3', 5)],
    ]), false);

    const visible = selectVisibleCommunicationOperators(index, {
        xMin: 4,
        xMax: 8,
        yStartIndex: 2,
        yEndIndex: 2,
    });

    expect(visible.map(item => item.operatorName)).toEqual(['rank-2', 'rank-1', 'rank-0-visible']);
});

it('keeps comparison and baseline operators separately for hit testing', () => {
    const index = buildCommunicationWebGLIndex(createDataSource(
        [[createOperator('comparison', 5, 3)]],
        [[createOperator('baseline', 5, 3)]],
    ), true);

    expect(hitTestCommunicationOperator(index, 0, 6, CommunicationOperatorSource.COMPARISON)?.operatorName)
        .toBe('comparison');
    expect(hitTestCommunicationOperator(index, 0, 6, CommunicationOperatorSource.BASELINE)?.operatorName)
        .toBe('baseline');
});

it('finds the closest repeated operator when slow-rank positioning supplies a start time', () => {
    const index = buildCommunicationWebGLIndex(createDataSource([[
        createOperator('AllReduce', 10),
        createOperator('AllReduce', 40),
    ]]), false);

    expect(findCommunicationOperator(index, 0, 'AllReduce', 39)?.startTime).toBe(40);
});

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

import type { AnalysisChartData, OperatorTimeItem } from './CommunicationTimeAnalysisChart';
import { colorPalette, hashToNumber } from '../../utils/colorUtil';

const NS_TO_MS_FACTOR = 0.000001;
const TIME_OVERSCAN_RATIO = 0.02;
const RANK_OVERSCAN = 1;

export enum CommunicationOperatorSource {
    COMPARISON = 0,
    BASELINE = 1,
}

export interface CommunicationWebGLOperator {
    id: number;
    rankIndex: number;
    operatorName: string;
    startTime: number;
    endTime: number;
    duration: number;
    source: CommunicationOperatorSource;
    colorIndex: number;
}

interface IndexedOperatorList {
    items: CommunicationWebGLOperator[];
    prefixMaxEnd: number[];
}

export interface CommunicationWebGLRank {
    rankId: string;
    dbPath: string;
    compare: IndexedOperatorList;
    baseline: IndexedOperatorList;
}

export interface CommunicationWebGLIndex {
    minTime: number;
    maxTime: number;
    ranks: CommunicationWebGLRank[];
    rankIndexById: Map<string, number>;
}

export interface CommunicationWebGLViewport {
    xMin: number;
    xMax: number;
    yStartIndex: number;
    yEndIndex: number;
}

interface OperatorRange {
    list: IndexedOperatorList;
    start: number;
    end: number;
}

const nsToMs = (value: number): number => value * NS_TO_MS_FACTOR;

const buildOperatorList = (
    items: OperatorTimeItem[],
    rankIndex: number,
    source: CommunicationOperatorSource,
    nextId: { value: number },
): IndexedOperatorList => {
    const indexedItems = items.map(item => {
        const startTime = nsToMs(item.startTime);
        const duration = nsToMs(item.duration);
        return {
            id: nextId.value++,
            rankIndex,
            operatorName: item.operatorName,
            startTime,
            endTime: startTime + duration,
            duration,
            source,
            colorIndex: hashToNumber(item.operatorName, colorPalette.length),
        };
    });
    const sorted = indexedItems.every((item, index) =>
        index === 0 || indexedItems[index - 1].startTime <= item.startTime);
    if (!sorted) {
        indexedItems.sort((left, right) => left.startTime - right.startTime || left.endTime - right.endTime);
    }
    const prefixMaxEnd: number[] = [];
    let maxEnd = Number.NEGATIVE_INFINITY;
    indexedItems.forEach(item => {
        maxEnd = Math.max(maxEnd, item.endTime);
        prefixMaxEnd.push(maxEnd);
    });
    return { items: indexedItems, prefixMaxEnd };
};

export const buildCommunicationWebGLIndex = (
    dataSource: AnalysisChartData,
    isCompare: boolean,
): CommunicationWebGLIndex => {
    const ranks = (dataSource?.data ?? []).slice().reverse();
    const nextId = { value: 0 };
    const indexedRanks = ranks.map((rank, rankIndex) => ({
        rankId: rank.rankId,
        dbPath: rank.dbPath,
        compare: buildOperatorList(
            rank.lists?.compare ?? [],
            rankIndex,
            CommunicationOperatorSource.COMPARISON,
            nextId,
        ),
        baseline: buildOperatorList(
            isCompare ? rank.lists?.baseline ?? [] : [],
            rankIndex,
            CommunicationOperatorSource.BASELINE,
            nextId,
        ),
    }));
    return {
        minTime: nsToMs(dataSource?.minTime ?? 0),
        maxTime: nsToMs(dataSource?.maxTime ?? 0),
        ranks: indexedRanks,
        rankIndexById: new Map(indexedRanks.map((rank, index) => [rank.rankId, index])),
    };
};

const lowerBound = (values: number[], target: number): number => {
    let left = 0;
    let right = values.length;
    while (left < right) {
        const middle = Math.floor((left + right) / 2);
        if (values[middle] < target) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left;
};

const upperBoundByStart = (items: CommunicationWebGLOperator[], target: number): number => {
    let left = 0;
    let right = items.length;
    while (left < right) {
        const middle = Math.floor((left + right) / 2);
        if (items[middle].startTime <= target) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return left;
};

const getOperatorRange = (list: IndexedOperatorList, xMin: number, xMax: number): OperatorRange | null => {
    if (list.items.length === 0) {
        return null;
    }
    const start = lowerBound(list.prefixMaxEnd, xMin);
    const end = upperBoundByStart(list.items, xMax);
    return start < end ? { list, start, end } : null;
};

export const selectVisibleCommunicationOperators = (
    index: CommunicationWebGLIndex,
    viewport: CommunicationWebGLViewport,
): CommunicationWebGLOperator[] => {
    if (index.ranks.length === 0 || viewport.xMin > viewport.xMax) {
        return [];
    }
    const duration = viewport.xMax - viewport.xMin;
    const xOverscan = Math.max(0, duration * TIME_OVERSCAN_RATIO);
    const xMin = Math.max(index.minTime, viewport.xMin - xOverscan);
    const xMax = Math.min(index.maxTime, viewport.xMax + xOverscan);
    const yStart = Math.max(0, Math.floor(viewport.yStartIndex) - RANK_OVERSCAN);
    const yEnd = Math.min(index.ranks.length - 1, Math.ceil(viewport.yEndIndex) + RANK_OVERSCAN);
    const visible: CommunicationWebGLOperator[] = [];
    for (let rankIndex = yStart; rankIndex <= yEnd; rankIndex++) {
        const rank = index.ranks[rankIndex];
        const ranges = [
            getOperatorRange(rank.compare, xMin, xMax),
            getOperatorRange(rank.baseline, xMin, xMax),
        ];
        ranges.forEach(range => {
            if (!range) {
                return;
            }
            for (let itemIndex = range.start; itemIndex < range.end; itemIndex++) {
                const operator = range.list.items[itemIndex];
                if (operator.endTime >= xMin && operator.startTime <= xMax) {
                    visible.push(operator);
                }
            }
        });
    }
    return visible;
};

const findHitInList = (list: IndexedOperatorList, time: number): CommunicationWebGLOperator | null => {
    const range = getOperatorRange(list, time, time);
    if (!range) {
        return null;
    }
    for (let index = range.end - 1; index >= range.start; index--) {
        const operator = range.list.items[index];
        if (operator.startTime <= time && operator.endTime >= time) {
            return operator;
        }
    }
    return null;
};

export const hitTestCommunicationOperator = (
    index: CommunicationWebGLIndex,
    rankIndex: number,
    time: number,
    source?: CommunicationOperatorSource,
): CommunicationWebGLOperator | null => {
    const rank = index.ranks[rankIndex];
    if (!rank) {
        return null;
    }
    if (source === CommunicationOperatorSource.COMPARISON) {
        return findHitInList(rank.compare, time);
    }
    if (source === CommunicationOperatorSource.BASELINE) {
        return findHitInList(rank.baseline, time);
    }
    return findHitInList(rank.baseline, time) ?? findHitInList(rank.compare, time);
};

export const findCommunicationOperator = (
    index: CommunicationWebGLIndex,
    rankId: string | number,
    operatorName: string,
    startTime?: number,
): CommunicationWebGLOperator | null => {
    const rankIndex = index.rankIndexById.get(`${rankId}`);
    if (rankIndex === undefined) {
        return null;
    }
    const rank = index.ranks[rankIndex];
    const candidates = [...rank.compare.items, ...rank.baseline.items]
        .filter(item => item.operatorName === operatorName);
    if (candidates.length === 0) {
        return null;
    }
    if (startTime === undefined) {
        return candidates[0];
    }
    return candidates.reduce((closest, item) =>
        Math.abs(item.startTime - startTime) < Math.abs(closest.startTime - startTime) ? item : closest);
};

/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
import type {
    InsightMetaData,
    MetaDataEnumType,
    ProcessMetaData,
    ThreadTraceRequest,
    ThreadMetaData,
    CounterRequest,
    MetaDataInnerBase,
    LabelMetaData,
    CounterMetaData,
} from '../../entity/data';
import type { StatusData } from '../../entity/chart';
import { UnitHeight } from '../../entity/insight';
import type { ChartDesc, InsightUnit } from '../../entity/insight';
import {
    CounterUnit,
    ProcessUnit,
    ThreadUnit,
    LabelUnit,
    ThreadingProcessUnit,
    ThreadingLlcCacheUnit,
    ThreadingThreadUnit,
} from './AscendUnit';
import { LLC_CACHE_METRIC_GROUP } from './llcCache';

const THREADING_ANALYSIS_META_TYPE = 'THREADING_ANALYSIS';

const parentMetaDataTree = new Map();

const MAX_RECURSIVE_COUNT = 10;
export function recursiveExpandUnit<T extends keyof MetaDataEnumType>(metaDataList: Array<InsightMetaData<T>>, parentUnit: InsightUnit, depth: number = 0): void {
    if (depth >= MAX_RECURSIVE_COUNT || metaDataList === undefined || parentUnit === undefined) {
        return;
    }
    for (const metaData of metaDataList) {
        metaData.metadata.dbPath = (metaData.metadata.dbPath as string | undefined) ?? parentUnit.metadata.dbPath as string;
        const existingUnit = parentUnit?.children?.find(unit => checkMetaData(unit.metadata, metaData));
        if (existingUnit) {
            recursiveExpandUnit(metaData.children ?? [], existingUnit, depth + 1);
        } else {
            const newUnit = newLane(metaData, parentUnit.metadata);
            if (newUnit !== undefined) {
                parentUnit.children = parentUnit.children ?? [];
                parentUnit.children.push(newUnit);
                recursiveExpandUnit(metaData.children ?? [], newUnit, depth + 1);
            }
        }
    }
    if (depth === 0) {
        reorderMultiSourceCardLanes(parentUnit);
    }
}

export function getHardwareSummaryDbPaths(unit: InsightUnit | undefined): string[] {
    const paths: string[] = [];
    const seen = new Set<string>();
    unit?.children?.forEach(child => {
        const dbPath = child.metadata.dbPath as string | undefined;
        if (dbPath !== undefined && dbPath !== '' && !seen.has(dbPath)) {
            seen.add(dbPath);
            paths.push(dbPath);
        }
    });
    return paths;
}

export function mergeSummaryStatusData(data: StatusData[]): StatusData[] {
    const sorted = [...data].filter(item => item.duration > 0).sort((left, right) => left.startTime - right.startTime);
    const merged: StatusData[] = [];
    sorted.forEach(item => {
        const previous = merged[merged.length - 1];
        if (previous === undefined || item.startTime > previous.startTime + previous.duration) {
            merged.push({ ...item });
            return;
        }
        const endTime = Math.max(previous.startTime + previous.duration, item.startTime + item.duration);
        previous.duration = endTime - previous.startTime;
    });
    return merged;
}

export function convertSummaryProcessData(
    data: Array<{ startTime: number; duration: number }> | undefined, timestampOffset: number,
): StatusData[] {
    return (data ?? []).map(item => ({
        startTime: item.startTime - timestampOffset,
        duration: item.duration,
        name: '',
        type: '',
    }));
}

export function reorderMultiSourceCardLanes(cardUnit: InsightUnit): void {
    const children = cardUnit.children;
    if (children === undefined) {
        return;
    }
    const hardware = children.find(unit => unit.metadata.metaType === 'Ascend Hardware');
    if (getHardwareSummaryDbPaths(hardware).length <= 1) {
        return;
    }
    const processUnits: InsightUnit[] = [];
    const otherUnits: InsightUnit[] = [];
    children.forEach(unit => {
        const processName = unit.metadata.processName as string | undefined;
        if (unit.name === 'Process' && processName?.startsWith('Process ') === true) {
            processUnits.push(unit);
        } else {
            otherUnits.push(unit);
        }
    });
    cardUnit.children = [...processUnits, ...otherUnits];
}

export function clearParentMap(): void {
    parentMetaDataTree.clear();
}

export function updateDataSourceAndParentMetaDataMap<T extends keyof MetaDataEnumType>(insightMetaData: InsightMetaData<T>, dataSource: DataSource, isClear = true): void {
    isClear && parentMetaDataTree.clear();
    insightMetaData.children?.forEach(processInfo => {
        const processMetadata = (processInfo.metadata as ProcessMetaData);
        processMetadata.dataSource = dataSource;
        insightMetaData.metadata.dataSource = dataSource;
        parentMetaDataTree.set(processMetadata, insightMetaData.metadata);
        handleChildren(processInfo, dataSource);
    });
}

function handleChildren<T extends keyof MetaDataEnumType>(processInfo: InsightMetaData<T>, dataSource: DataSource): void {
    processInfo.children?.forEach(threadInfo => {
        const threadMetadata = (threadInfo.metadata as ThreadMetaData);
        threadMetadata.dataSource = dataSource;
        parentMetaDataTree.set(threadInfo.metadata, processInfo.metadata);
        if (threadInfo.children && threadInfo.children.length > 0) {
            handleChildren(threadInfo, dataSource);
        }
    });
};

function newLane(insightMetaData: InsightMetaData<any>, parentMetaData: any): InsightUnit | undefined {
    switch (insightMetaData.type) {
        case 'label': {
            const parentMetaDataFromTree = parentMetaDataTree.get(insightMetaData.metadata);
            const meta = generateMetaData<LabelMetaData>({ cardId: parentMetaDataFromTree.cardId, dbPath: parentMetaDataFromTree.dbPath },
                insightMetaData.metadata.processId, insightMetaData.metadata.processName);
            meta.dataSource = parentMetaDataFromTree.dataSource;
            meta.metaType = insightMetaData.metadata.metaType;
            return new LabelUnit(meta);
        }
        case 'process': {
            const meta = generateMetaData<ProcessMetaData>({ cardId: insightMetaData.metadata.cardId, dbPath: insightMetaData.metadata.dbPath ?? parentMetaData.dbPath },
                insightMetaData.metadata.processId, insightMetaData.metadata.processName, insightMetaData.metadata.threadId);
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.label = insightMetaData.metadata.label;
            meta.metaType = insightMetaData.metadata.metaType;
            meta.bucketWidthNs = insightMetaData.metadata.bucketWidthNs;
            return meta.metaType === THREADING_ANALYSIS_META_TYPE
                ? new ThreadingProcessUnit(meta)
                : new ProcessUnit(meta);
        }
        case 'thread': {
            const meta = generateMetaData<ThreadMetaData>({ cardId: insightMetaData.metadata.cardId, dbPath: insightMetaData.metadata.dbPath ?? parentMetaData.dbPath },
                (parentMetaData as ProcessMetaData).processId, (parentMetaData as ProcessMetaData).processName,
                insightMetaData.metadata.threadId, insightMetaData.metadata.threadName);
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.metaType = insightMetaData.metadata.metaType;
            meta.sourceLabel = insightMetaData.metadata.sourceLabel;
            meta.groupNameValue = insightMetaData.metadata.groupNameValue;
            meta.rankList = insightMetaData.metadata.rankList;
            meta.headerTooltip = insightMetaData.metadata.headerTooltip;
            meta.bucketWidthNs = insightMetaData.metadata.bucketWidthNs;
            if (meta.metaType === THREADING_ANALYSIS_META_TYPE) {
                return new ThreadingThreadUnit(meta);
            }
            const threadUnit = new ThreadUnit(meta);
            const chart = threadUnit.chart as ChartDesc<'stackStatus'>;
            if (insightMetaData.metadata.maxDepth === 1 || insightMetaData.metadata.maxDepth === 0) {
                chart.height = UnitHeight.STANDARD;
                (chart.config as any).isCollapse = false;
                threadUnit.collapsible = false;
            }
            (chart.config as any).maxDepth = insightMetaData.metadata.maxDepth;
            return threadUnit;
        }
        case 'counter': {
            if (insightMetaData.metadata.metaType === THREADING_ANALYSIS_META_TYPE) {
                const sourceMetaData = parentMetaDataTree.get(insightMetaData.metadata);
                const meta = generateMetaData<CounterMetaData>(
                    { cardId: insightMetaData.metadata.cardId, dbPath: parentMetaData.dbPath },
                    insightMetaData.metadata.processId ?? '', insightMetaData.metadata.processName ?? 'Threading Analysis',
                    insightMetaData.metadata.threadId, insightMetaData.metadata.threadName,
                );
                meta.dataSource = sourceMetaData.dataSource;
                meta.dataType = insightMetaData.metadata.dataType;
                meta.metaType = insightMetaData.metadata.metaType;
                meta.bucketWidthNs = insightMetaData.metadata.bucketWidthNs;
                meta.metricGroup = insightMetaData.metadata.metricGroup;
                return meta.metricGroup === LLC_CACHE_METRIC_GROUP
                    ? new ThreadingLlcCacheUnit(meta)
                    : undefined;
            }
            const grandParentMetaData = parentMetaDataTree.get(parentMetaDataTree.get(insightMetaData.metadata));
            const meta = generateMetaData<CounterMetaData>({ cardId: grandParentMetaData.cardId, dbPath: grandParentMetaData.dbPath },
                (parentMetaData as ProcessMetaData).processId, insightMetaData.metadata.processName, insightMetaData.metadata.threadId,
                insightMetaData.metadata.threadName,
            );
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.dataType = insightMetaData.metadata.dataType;
            meta.metaType = insightMetaData.metadata.metaType;
            meta.headerTooltip = insightMetaData.metadata.headerTooltip;
            meta.maxValue = insightMetaData.metadata.maxValue;
            return new CounterUnit(meta);
        }
        default:
            return undefined;
    }
}

function generateMetaData<T extends MetaDataInnerBase = MetaDataInnerBase>(
    cardInfo: { cardId: string; dbPath: string }, processId: string, processName: string, threadId: string = '', threadName: string = ''): T {
    return {
        cardId: cardInfo.cardId,
        dbPath: cardInfo.dbPath,
        metaType: '',
        processId,
        processName,
        threadId,
        threadName,
    } as T;
}

function checkMetaData<T extends keyof MetaDataEnumType>(unitMetaData: any, paramMetaData: InsightMetaData<T>): boolean {
    const sameDbPath = unitMetaData.dbPath === paramMetaData.metadata.dbPath;
    if (paramMetaData.type === 'thread' && sameDbPath &&
        (unitMetaData as ThreadMetaData).threadId === (paramMetaData.metadata as ThreadMetaData).threadId) {
        return true;
    } else if (unitMetaData.type === 'process' && paramMetaData.type === 'process' && sameDbPath &&
        (unitMetaData as ProcessMetaData).processId === (paramMetaData.metadata as ProcessMetaData).processId) {
        return true;
    } else {
        return false;
    }
}

export function createStatusParam(method: string, params: Record<string, unknown>): string {
    const processParams = params as unknown as ThreadTraceRequest;
    const threadIds = processParams.threadIdList?.join(',') ?? '';
    return `cardId${processParams.cardId}&dbPath${processParams.dbPath ?? ''}&processId${processParams.processId}` +
        `&threadId${processParams.threadId ?? ''}&threadIdList${threadIds}&metaType${processParams.metaType}` +
        `&unitType${processParams.unitType}&s${processParams.startTime}&e${processParams.endTime}`;
}

export function createCounterParam(method: string, params: Record<string, unknown>): string {
    const counterParams = params as unknown as CounterRequest;
    return [
        method,
        counterParams.rankId,
        counterParams.dbPath ?? '',
        counterParams.pid,
        counterParams.threadId ?? '',
        counterParams.metricGroup ?? '',
        counterParams.metaType ?? '',
        counterParams.startTime,
        counterParams.endTime,
    ].join('&');
}

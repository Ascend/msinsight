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
import { UnitHeight } from '../../entity/insight';
import type { ChartDesc, InsightUnit } from '../../entity/insight';
import { CounterUnit, ProcessUnit, ThreadUnit, LabelUnit } from './AscendUnit';

const parentMetaDataTree = new Map();

const MAX_RECURSIVE_COUNT = 10;
export function recursiveExpandUnit<T extends keyof MetaDataEnumType>(metaDataList: Array<InsightMetaData<T>>, parentUnit: InsightUnit, depth: number = 0): void {
    if (depth >= MAX_RECURSIVE_COUNT || metaDataList === undefined || parentUnit === undefined) {
        return;
    }
    for (const metaData of metaDataList) {
        metaData.metadata.dbPath = parentUnit.metadata.dbPath as string;
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
            const meta = generateMetaData<ProcessMetaData>({ cardId: insightMetaData.metadata.cardId, dbPath: parentMetaData.dbPath },
                insightMetaData.metadata.processId, insightMetaData.metadata.processName, insightMetaData.metadata.threadId);
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.label = insightMetaData.metadata.label;
            meta.metaType = insightMetaData.metadata.metaType;
            return new ProcessUnit(meta);
        }
        case 'thread': {
            const meta = generateMetaData<ThreadMetaData>({ cardId: insightMetaData.metadata.cardId, dbPath: parentMetaData.dbPath },
                (parentMetaData as ProcessMetaData).processId, (parentMetaData as ProcessMetaData).processName,
                insightMetaData.metadata.threadId, insightMetaData.metadata.threadName);
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.metaType = insightMetaData.metadata.metaType;
            meta.groupNameValue = insightMetaData.metadata.groupNameValue;
            meta.rankList = insightMetaData.metadata.rankList;
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
            const grandParentMetaData = parentMetaDataTree.get(parentMetaDataTree.get(insightMetaData.metadata));
            const meta = generateMetaData<CounterMetaData>({ cardId: grandParentMetaData.cardId, dbPath: grandParentMetaData.dbPath },
                (parentMetaData as ProcessMetaData).processId, insightMetaData.metadata.processName, insightMetaData.metadata.threadId,
                insightMetaData.metadata.threadName,
            );
            meta.dataSource = parentMetaDataTree.get(insightMetaData.metadata).dataSource;
            meta.dataType = insightMetaData.metadata.dataType;
            meta.metaType = insightMetaData.metadata.metaType;
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
    if (paramMetaData.type === 'thread' && (unitMetaData as ThreadMetaData).threadId === (paramMetaData.metadata as ThreadMetaData).threadId) {
        return true;
    } else if (unitMetaData.type === 'process' && paramMetaData.type === 'process' && (unitMetaData as ProcessMetaData).processId === (paramMetaData.metadata as ProcessMetaData).processId) {
        return true;
    } else {
        return false;
    }
}

export function createStatusParam(method: string, params: Record<string, unknown>): string {
    const processParams = params as unknown as ThreadTraceRequest;
    return `cardId${processParams.cardId}&processId${processParams.processId}&metaType${processParams.metaType}&unitType${processParams.unitType}&s${processParams.startTime}&e${processParams.endTime}`;
}

export function createCounterParam(method: string, params: Record<string, unknown>): string {
    const counterParams = params as unknown as CounterRequest;
    return `cardId${counterParams.rankId}&processId${counterParams.pid}&threadId${counterParams.threadName}` +
        `&s${counterParams.startTime}&e${counterParams.endTime}`;
}

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

import { makeAutoObservable } from 'mobx';
import { getRootUnit, isStreamUnit } from '../utils';
import type { ProcessMetaData, ThreadMetaData } from './data';
import type { InsightUnit } from './insight';
import type { Session } from './session';

export type ThreadGroup = {
    cardId: string;
    processId: string;
    threadIds: string[];
};

export interface AutoMergeGroupUnits {
    group: ThreadGroup;
    sourceUnits: Array<InsightUnit | undefined>;
}

const normalizeThreadGroup = (group: ThreadGroup): ThreadGroup => ({
    cardId: group.cardId,
    processId: group.processId,
    threadIds: Array.from(new Set(group.threadIds)).sort(),
});

const getThreadGroupKey = (group: ThreadGroup): string => {
    const normalized = normalizeThreadGroup(group);
    return JSON.stringify([normalized.cardId, normalized.processId, normalized.threadIds]);
};

const areThreadGroupsConflicting = (first: ThreadGroup, second: ThreadGroup): boolean => {
    return first.cardId === second.cardId &&
        first.processId === second.processId &&
        first.threadIds.some(threadId => second.threadIds.includes(threadId));
};

const normalizeThreadGroups = (groups: ThreadGroup[]): ThreadGroup[] => {
    const groupMap = new Map<string, ThreadGroup>();
    groups.forEach((group) => {
        const normalized = normalizeThreadGroup(group);
        if (normalized.threadIds.length > 0) {
            groupMap.set(getThreadGroupKey(normalized), normalized);
        }
    });
    return Array.from(groupMap.values());
};

const getThreadKey = ({ cardId, processId, threadId }:
Pick<ThreadMetaData, 'cardId' | 'processId' | 'threadId'>): string => JSON.stringify([cardId, processId, threadId]);

const getAscendHardwareStreamUnits = (session: Session): InsightUnit[] => {
    const streamUnits: InsightUnit[] = [];
    const pendingUnits = [...getRootUnit(session.units)].reverse();
    while (pendingUnits.length > 0) {
        const unit = pendingUnits.pop();
        if (unit === undefined) { continue; }
        const processName = (unit.metadata as ProcessMetaData).processName;
        if (processName !== undefined && !processName.startsWith('Ascend Hardware')) { continue; }
        if (isStreamUnit(unit)) {
            streamUnits.push(unit);
        }
        if (unit.children !== undefined) {
            pendingUnits.push(...[...unit.children].reverse());
        }
    }
    return streamUnits;
};

const getActiveMergedGroupKeys = (streamUnits: InsightUnit[]): Set<string> => new Set(streamUnits.filter(unit => {
    const metadata = unit.metadata as ThreadMetaData;
    return !unit.isMerged && Array.isArray(metadata.threadIdList) && metadata.threadIdList.length > 0;
}).map(unit => getThreadGroupKey({
    cardId: (unit.metadata as ThreadMetaData).cardId,
    processId: (unit.metadata as ThreadMetaData).processId ?? '',
    threadIds: (unit.metadata as ThreadMetaData).threadIdList ?? [],
})));

/**
 * 存储合并的线程组数据
 * 它的生命周期应与 Project(DataSource) 的生命周期一致；即 Project 切换时，它要触发 clear
 * 在 Project 切换时，它要么清空 session.units 数据，要么完全刷新 session.units 数据
 * 因此当 session.units 数据被清空或完全刷新时，本对象也要清空
 */
export class MergedThreadData {
    private _mergedThreadGroupList: ThreadGroup[];
    private _autoMergedThreadGroupList: ThreadGroup[];
    private readonly _autoMergedThreadGroupKeySet: Set<string>;
    private readonly _autoMergedThreadKeySet: Set<string>;

    constructor() {
        makeAutoObservable(this);
        this._mergedThreadGroupList = [];
        this._autoMergedThreadGroupList = [];
        this._autoMergedThreadGroupKeySet = new Set();
        this._autoMergedThreadKeySet = new Set();
    }

    get mergedThreadGroupList(): ThreadGroup[] { return this._mergedThreadGroupList; }

    set mergedThreadGroupList(value: ThreadGroup[]) {
        this._mergedThreadGroupList = normalizeThreadGroups(value);
    }

    get autoMergedThreadGroupList(): ThreadGroup[] { return this._autoMergedThreadGroupList; }

    concatThreadGroupList(other: ThreadGroup[]): void {
        this._mergedThreadGroupList = normalizeThreadGroups(this._mergedThreadGroupList.concat(other));
    }

    private rebuildAutoThreadIndexes(): void {
        this._autoMergedThreadGroupKeySet.clear();
        this._autoMergedThreadKeySet.clear();
        this._autoMergedThreadGroupList.forEach(group => {
            this._autoMergedThreadGroupKeySet.add(getThreadGroupKey(group));
            group.threadIds.forEach(threadId => this._autoMergedThreadKeySet.add(getThreadKey({
                cardId: group.cardId,
                processId: group.processId,
                threadId,
            })));
        });
    }

    registerAutoThreadGroups(groups: ThreadGroup[]): void {
        const normalizedGroups = normalizeThreadGroups(groups);
        const newAutoGroups = normalizedGroups.filter(group => !this._autoMergedThreadGroupKeySet.has(getThreadGroupKey(group)));
        this._autoMergedThreadGroupList = normalizeThreadGroups(this._autoMergedThreadGroupList.concat(normalizedGroups));
        this.rebuildAutoThreadIndexes();
        // 后到的自动组不能覆盖已有的手工拓扑；重复上报也不能重新激活已经取消的自动组。
        const groupsToActivate = newAutoGroups.filter(group => !this._mergedThreadGroupList.some(currentGroup => {
            return getThreadGroupKey(currentGroup) !== getThreadGroupKey(group) && areThreadGroupsConflicting(currentGroup, group);
        }));
        this.concatThreadGroupList(groupsToActivate);
    }

    isAutoMergeSource(metadata: ThreadMetaData): boolean {
        if (Array.isArray(metadata.threadIdList) || metadata.threadId === '') {
            return false;
        }
        return this._autoMergedThreadKeySet.has(getThreadKey({
            cardId: metadata.cardId,
            processId: metadata.processId ?? '',
            threadId: metadata.threadId,
        }));
    }

    isAutoMergedGroup(metadata: ThreadMetaData): boolean {
        if (!Array.isArray(metadata.threadIdList) || metadata.threadIdList.length === 0) {
            return false;
        }
        return this._autoMergedThreadGroupKeySet.has(getThreadGroupKey({
            cardId: metadata.cardId,
            processId: metadata.processId ?? '',
            threadIds: metadata.threadIdList,
        }));
    }

    addMergedGroup(addedGroup: ThreadGroup): void {
        const normalizedGroup = normalizeThreadGroup(addedGroup);
        if (normalizedGroup.threadIds.length === 0) { return; }
        // 同一卡、同一进程内有成员重叠的组不能同时存在。
        const groupsWithoutConflicts = this._mergedThreadGroupList.filter(group => {
            return !areThreadGroupsConflicting(group, normalizedGroup);
        });
        this._mergedThreadGroupList = normalizeThreadGroups(groupsWithoutConflicts.concat(normalizedGroup));
    }

    removeMergedGroups(removedGroups: ThreadGroup[]): void {
        const removedGroupKeys = new Set(normalizeThreadGroups(removedGroups).map(getThreadGroupKey));
        if (removedGroupKeys.size === 0) { return; }
        this._mergedThreadGroupList = this._mergedThreadGroupList.filter(group => {
            return !removedGroupKeys.has(getThreadGroupKey(group));
        });
    }

    removeThreadGroupsByCardIds(cardIds: string[]): void {
        const removedCardIds = new Set(cardIds);
        if (removedCardIds.size === 0) { return; }
        this._mergedThreadGroupList = this._mergedThreadGroupList.filter(group => !removedCardIds.has(group.cardId));
        this._autoMergedThreadGroupList = this._autoMergedThreadGroupList.filter(group => !removedCardIds.has(group.cardId));
        this.rebuildAutoThreadIndexes();
    }

    getNeedMergeThreadLists(session: Session): InsightUnit[][] {
        const mergedThreadGroupList: ThreadGroup[] = this._mergedThreadGroupList;
        if (mergedThreadGroupList.length === 0) { return []; }
        const streamUnits = getAscendHardwareStreamUnits(session);
        const activeMergedGroupKeys = getActiveMergedGroupKeys(streamUnits);
        const sourceUnits = streamUnits.filter(node => {
            const metadata = node.metadata as ThreadMetaData;
            return !node.isMerged && !Array.isArray(metadata.threadIdList);
        });
        const map = new Map(sourceUnits.map(item => [getThreadKey(item.metadata as ThreadMetaData), item]));
        const invalidGroupIndexes = new Set<number>();
        const needMergeThreadLists = mergedThreadGroupList.map((threadGroup, idx): InsightUnit[] => {
            if (activeMergedGroupKeys.has(getThreadGroupKey(threadGroup))) { return []; }
            const threads = threadGroup.threadIds.map((id) => {
                const key = getThreadKey({
                    cardId: threadGroup.cardId,
                    processId: threadGroup.processId,
                    threadId: id,
                });
                console.assert(map.has(key), `${key} is not found when merge units`);
                return map.get(key);
            }).filter(item => item !== undefined);
            if (threads.length === 0) {
                invalidGroupIndexes.add(idx);
            }
            // 自动合并必须以服务端给出的完整线程组为单位，不能在成员缺失时生成部分合并泳道。
            return threads.length === threadGroup.threadIds.length ? threads : [];
        });
        // 清除无效数据
        this._mergedThreadGroupList = this._mergedThreadGroupList.filter((_, idx) => !invalidGroupIndexes.has(idx));
        return needMergeThreadLists;
    }

    getAutoMergeGroupUnits(session: Session, relatedSourceUnits?: InsightUnit[]): AutoMergeGroupUnits[] {
        if (this._autoMergedThreadGroupList.length === 0) { return []; }

        const streamUnits = getAscendHardwareStreamUnits(session);
        const sourceUnits = streamUnits.filter(unit => {
            const metadata = unit.metadata as ThreadMetaData;
            return !Array.isArray(metadata.threadIdList) && metadata.threadId !== '';
        });
        const sourceUnitMap = new Map(sourceUnits.map(unit => {
            const metadata = unit.metadata as ThreadMetaData;
            return [getThreadKey({
                cardId: metadata.cardId,
                processId: metadata.processId ?? '',
                threadId: metadata.threadId,
            }), unit] as const;
        }));
        const relatedSourceKeys = relatedSourceUnits === undefined
            ? undefined
            : new Set(relatedSourceUnits.map(unit => {
                const metadata = unit.metadata as ThreadMetaData;
                return getThreadKey({
                    cardId: metadata.cardId,
                    processId: metadata.processId ?? '',
                    threadId: metadata.threadId,
                });
            }));

        return this._autoMergedThreadGroupList.reduce<AutoMergeGroupUnits[]>((result, group) => {
            if (relatedSourceKeys !== undefined && !group.threadIds.some(threadId => relatedSourceKeys.has(getThreadKey({
                cardId: group.cardId,
                processId: group.processId,
                threadId,
            })))) {
                return result;
            }
            const sourceThreadUnits = group.threadIds.map(threadId => sourceUnitMap.get(getThreadKey({
                cardId: group.cardId,
                processId: group.processId,
                threadId,
            })));
            result.push({ group, sourceUnits: sourceThreadUnits });
            return result;
        }, []);
    }

    clear(): void {
        this._mergedThreadGroupList = [];
        this._autoMergedThreadGroupList = [];
        this.rebuildAutoThreadIndexes();
    }
}

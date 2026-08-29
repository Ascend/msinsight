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

import { runInAction } from 'mobx';
import { register } from './register';
import type { Session } from '../entity/session';
import { ThreadMetaData } from '../entity/data';
import { type ChartDesc, InsightUnit, UnitHeight } from '../entity/insight';
import { message } from 'antd';
import type { StackStatusConfig } from '../entity/chart';
import i18n from '@insight/lib/i18n';
import { isStreamUnit } from '../utils';
import { ThreadGroup } from '../entity/mergedThreadData';
import { isAncestorPinned, isPinned, switchPinned } from '../components/ChartContainer/unitPin';

const clearSelectedUnits = (session: Session): void => {
    session.selectedUnits = [];
};

interface MergeUnitListResult {
    mergedMeta: ThreadMetaData;
    sourceUnits: InsightUnit[];
}

const findInsertIndex = (allUnits: InsightUnit[], selectedUnits: InsightUnit[]): number => {
    let minIndex = Infinity;

    for (const sUnit of selectedUnits) {
        const sMetaData = sUnit.metadata as ThreadMetaData;

        const indexInAllUnits = allUnits.findIndex(aUnit => {
            const aMetaData = aUnit.metadata as ThreadMetaData;

            return sMetaData.threadId !== '' ? aMetaData.threadId === sMetaData.threadId : aMetaData.threadIdList === sMetaData.threadIdList;
        });
        if (indexInAllUnits !== -1 && indexInAllUnits < minIndex) {
            minIndex = indexInAllUnits;
        }
    }

    return minIndex === Infinity ? 0 : minIndex;
};

const extractThreadIds = (units: InsightUnit[]): string[] => {
    return units.flatMap(unit => {
        const { threadIdList, threadId } = unit.metadata as ThreadMetaData;
        return threadIdList ?? [threadId];
    }).filter((threadId): threadId is string => typeof threadId === 'string' && threadId !== '');
};

const normalizeThreadIds = (threadIds: string[]): string[] => Array.from(new Set(threadIds)).sort();

const hasSameThreadIds = (unit: InsightUnit, threadIds: string[]): boolean => {
    const unitThreadIds = (unit.metadata as ThreadMetaData).threadIdList;
    if (!Array.isArray(unitThreadIds)) { return false; }
    const normalizedUnitThreadIds = normalizeThreadIds(unitThreadIds);
    const normalizedThreadIds = normalizeThreadIds(threadIds);
    return normalizedUnitThreadIds.length === normalizedThreadIds.length &&
        normalizedUnitThreadIds.every((threadId, index) => threadId === normalizedThreadIds[index]);
};

const canMergeUnitList = (units: InsightUnit[]): boolean => {
    const [firstUnit] = units;
    const unitParent = firstUnit?.parent;
    if (!unitParent?.children || units.length === 0 || units.some(unit => unit.parent !== unitParent)) {
        return false;
    }
    const threadIds = extractThreadIds(units);
    return threadIds.length > 0 && !unitParent.children.some(unit => hasSameThreadIds(unit, threadIds));
};

const getThreadNameList = (threadIds: string[], firstUnit: InsightUnit): string[] => {
    const childrenUnits = firstUnit.parent?.children ?? [];
    const threadMap = new Map<string, ThreadMetaData>();

    for (const unit of childrenUnits) {
        const meta = unit.metadata as ThreadMetaData;
        if (meta.threadId) {
            threadMap.set(meta.threadId, meta);
        }
    }

    return threadIds.sort().map(threadId => {
        const meta = threadMap.get(threadId);
        return meta?.threadName?.replace(/^Stream\s*/, '') ?? '';
    });
};

const getMergedUnitMetaData = (selectedUnits: InsightUnit[]): ThreadMetaData => {
    const [firstUnit] = selectedUnits;
    const firstMeta = firstUnit.metadata as ThreadMetaData;

    const threadIdList = extractThreadIds(selectedUnits);
    const threadNameList = getThreadNameList(threadIdList, firstUnit);

    return {
        ...firstMeta,
        threadIdList,
        threadId: '',
        threadName: `Stream Merged (${threadNameList.join(', ')})`,
    };
};

// 标记被合并的泳道
const markMergedUnits = (selectedUnits: InsightUnit[], allUnitList: InsightUnit[]): void => {
    for (const unit of selectedUnits) {
        const meta = unit.metadata as ThreadMetaData;
        if (meta.threadIdList) {
            const index = allUnitList.indexOf(unit);
            if (index === -1) { continue; }
            allUnitList.splice(index, 1);
        } else {
            unit.isMerged = true;
        }
    }
};

type ThreadUnitConstructor = new (metadata: ThreadMetaData) => InsightUnit;

const createMergedUnit = (mergedMeta: ThreadMetaData, sourceUnit: InsightUnit, isAutoMergedUnit = false): InsightUnit => {
    const UnitConstructor = sourceUnit.constructor as ThreadUnitConstructor;
    const threadUnit = new UnitConstructor(mergedMeta);
    const chart = threadUnit.chart as ChartDesc<'stackStatus'>;
    const chartConfig = chart.config as StackStatusConfig;

    chart.height = UnitHeight.STANDARD;
    chart.renderTooltip = (data): Map<string, string> => new Map([
        ['Name', data.name],
        ['Stream', data.threadId ?? '-'],
    ]);
    chartConfig.maxDepth = 0;
    chartConfig.isCollapse = false;
    threadUnit.collapsible = false;
    threadUnit.isAutoMergedUnit = isAutoMergedUnit;
    threadUnit.notifications = [(): string => i18n.t('Merged unit', { ns: 'timeline' })];

    return threadUnit;
};

/**
 * 在同一父节点下创建合并泳道，并更新源泳道在树中的状态。
 * Session 级的合并记录和渲染状态由调用方按操作类型处理。
 * @returns 合并结果；不满足合并条件时返回 undefined。
 */
const mergeUnitList = (units: InsightUnit[], isAutoMergedUnit = false): MergeUnitListResult | undefined => {
    const [firstUnit] = units;
    const unitParent = firstUnit?.parent;
    if (!unitParent?.children || !canMergeUnitList(units)) {
        return undefined;
    }

    // 构建合并泳道的 metadata 和实例。
    const mergedMeta = getMergedUnitMetaData(units);
    const threadUnit = createMergedUnit(mergedMeta, firstUnit, isAutoMergedUnit);
    threadUnit.parent = unitParent;

    // 插入到最靠前的源泳道位置，并标记或移除原泳道。
    const insertIndex = findInsertIndex(unitParent.children, units);
    unitParent.children.splice(insertIndex, 0, threadUnit);
    markMergedUnits(units, unitParent.children);

    return { mergedMeta, sourceUnits: units };
};

const toThreadGroup = (mergedMeta: ThreadMetaData): ThreadGroup => ({
    cardId: mergedMeta.cardId ?? '',
    processId: mergedMeta.processId ?? '',
    threadIds: mergedMeta.threadIdList ?? [],
});

/**
 * 合并泳道，用户操作情况，需要 addMergedGroup
 * @param session
 */
const mergeUnits = (session: Session): void => {
    const selectedUnits = session.selectedUnits;
    if (selectedUnits.length === 0) { return; }

    const [firstUnit] = selectedUnits;
    const { cardId } = firstUnit.metadata as ThreadMetaData;

    // 1. 校验
    const allStreamUnits = selectedUnits.every(isStreamUnit);
    if (!allStreamUnits) {
        message.warning(i18n.t('timeline:MergeStreamOnly'));
        return;
    }

    const allSameCard = selectedUnits.every(unit => {
        const metaData = unit.metadata as ThreadMetaData;
        return metaData.cardId === cardId;
    });
    if (!allSameCard) {
        message.warning(i18n.t('timeline:MergeInSameCardOnly'));
        return;
    }

    runInAction(() => {
        const result = mergeUnitList(selectedUnits);
        if (result === undefined) { return; }
        clearSelectedUnits(session);
        session.mergedThreadData.addMergedGroup(toThreadGroup(result.mergedMeta));
        session.renderTrigger = !session.renderTrigger;
    });
};

/**
 * 合并泳道，自动更新情况，根据 session.mergedThreadData 合并，不需要 addMergedGroup
 * @param session
 */
export const mergeUnitsWhenLoadProject = (session: Session): void => {
    const needMergeThreadLists = session.mergedThreadData.getNeedMergeThreadLists(session);

    runInAction(() => {
        let hasMerged = false;
        needMergeThreadLists.forEach((needMergeThreadList) => {
            if (needMergeThreadList.length === 0) { return; }
            const mergedMeta = getMergedUnitMetaData(needMergeThreadList);
            const isAutoMergedUnit = session.mergedThreadData.isAutoMergedGroup(mergedMeta);
            hasMerged = mergeUnitList(needMergeThreadList, isAutoMergedUnit) !== undefined || hasMerged;
        });
        if (hasMerged) {
            session.renderTrigger = !session.renderTrigger;
        }
    });
};

const clearAffectedSourceSelections = (session: Session, sourceUnits: Set<InsightUnit>): void => {
    const selectedUnits = session.selectedUnits.filter(unit => !sourceUnits.has(unit));
    if (selectedUnits.length !== session.selectedUnits.length) {
        session.selectedUnits = selectedUnits;
    }
    if (session.sliceSelection.targetUnit !== null && sourceUnits.has(session.sliceSelection.targetUnit)) {
        session.selectedRange = undefined;
        session.sliceSelection.targetUnit = null;
    }
};

const mergeAutoThreadLists = (session: Session, threadLists: InsightUnit[][]): Set<InsightUnit> => {
    const affectedUnits = new Set<InsightUnit>();
    threadLists.forEach((threadList) => {
        const result = mergeUnitList(threadList, true);
        if (result === undefined) { return; }
        result.sourceUnits.forEach(unit => affectedUnits.add(unit));
        session.mergedThreadData.addMergedGroup(toThreadGroup(result.mergedMeta));
    });
    return affectedUnits;
};

interface AutoMergeRestorePlan {
    threadLists: InsightUnit[][];
    reasons: string[];
}

const getAutoMergeRestorePlan = (session: Session, relatedSourceUnits?: InsightUnit[]): AutoMergeRestorePlan => {
    const groupUnitsList = session.mergedThreadData.getAutoMergeGroupUnits(session, relatedSourceUnits);
    const reasons = new Set<string>();
    const threadLists: InsightUnit[][] = [];
    const plannedSourceUnits = new Set<InsightUnit>();

    if (groupUnitsList.length === 0) {
        reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByIncomplete'));
    }
    groupUnitsList.forEach(groupUnits => {
        let isGroupBlocked = false;
        if (groupUnits.sourceUnits.some(unit => unit === undefined)) {
            reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByIncomplete'));
            return;
        }
        const completeSourceUnits = groupUnits.sourceUnits as InsightUnit[];
        const parent = completeSourceUnits[0]?.parent;
        if (parent === undefined || completeSourceUnits.some(unit => unit.parent !== parent) ||
            completeSourceUnits.some(unit => plannedSourceUnits.has(unit))) {
            reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByIncomplete'));
            isGroupBlocked = true;
        }
        if (completeSourceUnits.some(unit => unit.isMerged) || (!isGroupBlocked && !canMergeUnitList(completeSourceUnits))) {
            reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByMerged'));
            isGroupBlocked = true;
        }
        if (completeSourceUnits.some(unit => !unit.isUnitVisible)) {
            reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByHidden'));
            isGroupBlocked = true;
        }
        if (completeSourceUnits.some(unit => isPinned(unit) || isAncestorPinned(unit))) {
            reasons.add(i18n.t('timeline:autoMerge.restoreBlockedByPinned'));
            isGroupBlocked = true;
        }
        if (!isGroupBlocked) {
            completeSourceUnits.forEach(unit => plannedSourceUnits.add(unit));
            threadLists.push(completeSourceUnits);
        }
    });

    return { threadLists, reasons: Array.from(reasons) };
};

const applyAutoMergeRestorePlan = (session: Session, threadLists: InsightUnit[][]): boolean => {
    let restored = false;
    runInAction(() => {
        const affectedUnits = mergeAutoThreadLists(session, threadLists);
        if (affectedUnits.size === 0) { return; }
        clearAffectedSourceSelections(session, affectedUnits);
        session.renderTrigger = !session.renderTrigger;
        restored = true;
    });
    return restored;
};

export const isAutoMergeActionDisabled = (session: Session): boolean => {
    return session.phase !== 'download' || session.units.some(unit => unit.phase === 'analyzing');
};

export const tryRestoreAutoMergedGroup = (session: Session, sourceUnit: InsightUnit): boolean => {
    if (isAutoMergeActionDisabled(session)) { return false; }
    const { threadLists, reasons } = getAutoMergeRestorePlan(session, [sourceUnit]);
    if (reasons.length > 0) {
        message.error(i18n.t('timeline:autoMerge.restoreFailed', { reason: reasons.join('; ') }));
        return false;
    }
    return threadLists.length > 0 && applyAutoMergeRestorePlan(session, threadLists);
};

const hasMergedUnit = (selectedUnits: InsightUnit[]): boolean => {
    const mergedUnits = selectedUnits.filter(unit => {
        const metaData = unit.metadata as ThreadMetaData;

        return metaData.threadIdList;
    });

    return mergedUnits.length > 0;
};

// 从 parent 中移除指定 unit
const removeUnitFromParent = (unit: InsightUnit, parent?: { children?: InsightUnit[] }): void => {
    const children = parent?.children;
    if (!children) { return; }

    const index = children.indexOf(unit);
    if (index !== -1) {
        children.splice(index, 1);
    }
};

interface UnmergedUnitListResult {
    threadIds: Set<string>;
    removedGroups: ThreadGroup[];
}

// 获取合并泳道的 threadId 并移除合并泳道
const getMergedThreadIdsAndRemoveMergedUnit = (selectedUnits: InsightUnit[], parent: InsightUnit): UnmergedUnitListResult => {
    const threadIds = new Set<string>();
    const removedGroups: ThreadGroup[] = [];

    for (const unit of selectedUnits) {
        const metadata = unit.metadata as ThreadMetaData;

        if (Array.isArray(metadata.threadIdList) && parent.children?.includes(unit)) {
            removeUnitFromParent(unit, parent);
            metadata.threadIdList.forEach(id => threadIds.add(id));
            removedGroups.push(toThreadGroup(metadata));
        }
    }

    return { threadIds, removedGroups };
};

// 取消指定泳道的合并标记
const unmarkMergedUnits = (threadIds: Set<string>, children?: InsightUnit[]): void => {
    if (!children) { return; }

    for (const unit of children) {
        const metadata = unit.metadata as ThreadMetaData;
        if (threadIds.has(metadata.threadId as string)) {
            unit.isMerged = false;
        }
    }
};

const removeDirectlyPinnedMergedUnits = (session: Session, mergedUnits: InsightUnit[]): void => {
    const pinnedMergedUnits = mergedUnits.filter(unit => isPinned(unit) || session.pinnedUnits.includes(unit));
    if (pinnedMergedUnits.length === 0) { return; }
    session.pinnedUnits = session.pinnedUnits.filter(unit => !pinnedMergedUnits.includes(unit));
    pinnedMergedUnits.forEach(unit => {
        if (isPinned(unit)) {
            switchPinned(unit);
        }
    });
};

const isSliceSelectionMode = (session: Session): boolean => {
    const { active, targetUnit } = session.sliceSelection;
    return active && targetUnit?.name === 'Thread';
};

const unmergeUnits = (session: Session, mergedUnits: InsightUnit[] = session.selectedUnits, clearAllSelected = true): boolean => {
    if (!hasMergedUnit(mergedUnits)) {
        return false;
    }
    const parent = mergedUnits[0].parent;
    const children = parent?.children;
    if (!children) {
        return false;
    }

    let unmerged = false;
    runInAction(() => {
        const {
            threadIds: mergedThreadIds,
            removedGroups,
        } = getMergedThreadIdsAndRemoveMergedUnit(mergedUnits, parent);
        if (removedGroups.length === 0) { return; }

        unmarkMergedUnits(mergedThreadIds, children);
        session.mergedThreadData.removeMergedGroups(removedGroups);
        removeDirectlyPinnedMergedUnits(session, mergedUnits);
        if (clearAllSelected) {
            clearSelectedUnits(session);
        } else if (session.selectedUnits.some(unit => mergedUnits.includes(unit))) {
            session.selectedUnits = session.selectedUnits.filter(unit => !mergedUnits.includes(unit));
        }
        if (isSliceSelectionMode(session) && (clearAllSelected ||
            (session.sliceSelection.targetUnit !== null && mergedUnits.includes(session.sliceSelection.targetUnit)))) {
            session.selectedRange = undefined;
            session.sliceSelection.targetUnit = null;
        }
        session.renderTrigger = !session.renderTrigger;
        unmerged = true;
    });
    return unmerged;
};

export const unmergeAutoMergedGroup = (session: Session, mergedUnit: InsightUnit): boolean => {
    if (isAutoMergeActionDisabled(session)) {
        return false;
    }
    const metadata = mergedUnit.metadata as ThreadMetaData;
    if (mergedUnit.isAutoMergedUnit !== true || !session.mergedThreadData.isAutoMergedGroup(metadata)) {
        return false;
    }
    return unmergeUnits(session, [mergedUnit], false);
};

export const actionMergeUnits = register({
    name: 'mergeUnits',
    label: 'timeline:contextMenu.Merge Units',
    visible: (session) => {
        return session.selectedUnits.length > 1;
    },
    perform: (session): void => {
        mergeUnits(session);
    },
});

export const actionUnmergeUnits = register({
    name: 'unmergeUnits',
    label: 'timeline:contextMenu.Unmerge Units',
    visible: (session) => {
        return hasMergedUnit(session.selectedUnits);
    },
    perform: (session): void => {
        unmergeUnits(session);
    },
});

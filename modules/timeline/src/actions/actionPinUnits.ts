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

import { register } from './register';
import type { Session } from '../entity/session';
import { runInAction } from 'mobx';
import { message } from 'antd';
import i18n from '@insight/lib/i18n';
import { isPinned, switchPinned } from '../components/ChartContainer/unitPin';
import type { InsightUnit } from '../entity/insight';
import type { CardMetaData, CounterMetaData, ProcessMetaData, ThreadMetaData } from '../entity/data';
import { getAutoKey } from '../utils/dataAutoKey';
import { preOrderFlatten } from '../entity/common';
import { getRootUnit } from '../utils';

// 限制单次置顶最大数量，防止过多泳道导致渲染性能下降
const MAX_PIN_UNIT_COUNT = 100;

const hasStringValue = (str: string = ''): boolean => str !== '';

function getUnitLevelName(unit: InsightUnit): string {
    const md = unit.metadata;
    const threadId = (md as ThreadMetaData)?.threadId;
    if (hasStringValue(threadId)) {
        return (md as ThreadMetaData).threadName ?? '';
    }
    if (unit.name === 'Counter') {
        return (md as CounterMetaData)?.threadName ?? '';
    }
    const processId = (md as ProcessMetaData)?.processId;
    if (hasStringValue(processId)) {
        return (md as ProcessMetaData).processName ?? '';
    }
    return (md as CardMetaData)?.cardName ?? '';
}

interface SelectedUnitStatus {
    isThreadNameStartWithGroup: boolean;
    groupNameValue: string;
    unitLevel: string;
    unitLevelName: string;
    isPinned: boolean;
    isLeafUnit: boolean;
    isHiddenUnit: boolean;
    isMergedUnit: boolean;
}

interface SelectedUnitListStatus {
    isAllThreadNameStartWithGroup: boolean;
    isSameGroupNameValue: boolean;
    isSameUnitLevelName: boolean;
    unitLevelName: string;
    isAllPinned: boolean;
    isAllUnpinned: boolean;
    isAllLeafUnit: boolean;
    isAllVisibleUnits: boolean;
    isAllUnmergedUnits: boolean;
}

function calculateSelectedUnitStatus(selectedUnit: InsightUnit): SelectedUnitStatus {
    const metadata = selectedUnit.metadata as ThreadMetaData;
    return {
        isThreadNameStartWithGroup: metadata?.threadName?.includes('Group') ?? false,
        groupNameValue: metadata?.groupNameValue ?? '',
        unitLevel: selectedUnit.name,
        unitLevelName: getUnitLevelName(selectedUnit),
        isPinned: isPinned(selectedUnit),
        isLeafUnit: !selectedUnit.children || selectedUnit.children.length === 0,
        isHiddenUnit: selectedUnit.name === 'Empty',
        isMergedUnit: Array.isArray(metadata?.threadIdList) && metadata.threadIdList.length > 0,
    };
}

export function calculateSelectedUnitListStatus(selectedUnits: InsightUnit[]): SelectedUnitListStatus {
    const selectedUnitStatuses = selectedUnits.map(calculateSelectedUnitStatus);
    const isAllThreadNameStartWithGroup = selectedUnitStatuses
        .every(({ isThreadNameStartWithGroup }) => isThreadNameStartWithGroup);

    const isAllPinned = selectedUnitStatuses.every((status) => status.isPinned);
    const isAllUnpinned = selectedUnitStatuses.every((status) => !status.isPinned);
    const isAllLeafUnit = selectedUnitStatuses.every((status) => status.isLeafUnit);
    const isAllVisibleUnits = selectedUnitStatuses.every((status) => !status.isHiddenUnit);
    const isAllUnmergedUnits = selectedUnitStatuses.every((status) => !status.isMergedUnit);

    const groupNameValue = selectedUnitStatuses?.[0]?.groupNameValue ?? '';
    const isSameGroupNameValue = hasStringValue(groupNameValue) && selectedUnitStatuses
        .every((status) => status.groupNameValue === groupNameValue);

    const unitLevelName = selectedUnitStatuses?.[0]?.unitLevelName ?? '';
    const isSameUnitLevelName = selectedUnitStatuses
        .every(({ unitLevel, unitLevelName: name }) =>
            unitLevel === selectedUnitStatuses[0]?.unitLevel && name === selectedUnitStatuses[0]?.unitLevelName);

    return {
        isAllThreadNameStartWithGroup,
        isSameGroupNameValue,
        isSameUnitLevelName,
        unitLevelName,
        isAllPinned,
        isAllUnpinned,
        isAllLeafUnit,
        isAllVisibleUnits,
        isAllUnmergedUnits,
    };
}

function unpinAll(session: Session): void {
    const { pinnedUnits } = session;
    runInAction(() => {
        session.pinnedUnits = [];
        pinnedUnits.forEach((item): void => switchPinned(item));
    });
}

function getSameLevelUnits(session: Session): InsightUnit[] {
    if (session.selectedUnits.length === 0) {
        return [];
    }
    const selectedUnit = session.selectedUnits[0];
    const selectedGroupNameValue = (selectedUnit.metadata as ThreadMetaData)?.groupNameValue;
    const flattenUnits = preOrderFlatten(getRootUnit(session.units), 0);
    // 优先按 groupNameValue 匹配（兼容老数据）
    if (hasStringValue(selectedGroupNameValue)) {
        return flattenUnits.filter((unit) => (unit.metadata as ThreadMetaData)?.groupNameValue === selectedGroupNameValue);
    }
    const selectedName = getUnitLevelName(selectedUnit);
    if (!hasStringValue(selectedName)) {
        return [];
    }
    const selectedLevel = selectedUnit.name;
    return flattenUnits.filter((unit) => unit.name === selectedLevel && getUnitLevelName(unit) === selectedName);
}

function showMaxPinWarning(count: number): void {
    message.warning(i18n.t('timeline:contextMenu.Pin Unit Max Warning', { count, max: MAX_PIN_UNIT_COUNT }));
}

function pinAllSameLevel(session: Session): void {
    if (session.selectedUnits.length === 0) {
        return;
    }
    const sameLevelUnits = getSameLevelUnits(session);
    if (sameLevelUnits.length === 0) {
        return;
    }
    if (sameLevelUnits.length > MAX_PIN_UNIT_COUNT) {
        showMaxPinWarning(sameLevelUnits.length);
    }
    const targetUnits = sameLevelUnits.slice(0, MAX_PIN_UNIT_COUNT);
    const pinnedUnitKeys = session.pinnedUnits.map((item) => getAutoKey(item));
    const addUnits = targetUnits.reduce<InsightUnit[]>((acc, curr): InsightUnit[] => {
        const key = getAutoKey(curr);
        if (pinnedUnitKeys.includes(key)) {
            return acc;
        }
        acc.push(curr);
        pinnedUnitKeys.push(key);
        return acc;
    }, []);
    runInAction(() => {
        session.pinnedUnits = [...session.pinnedUnits, ...addUnits];
        addUnits.forEach((item): void => switchPinned(item));
    });
}

function unpinAllSameLevel(session: Session): void {
    const sameLevelUnits = getSameLevelUnits(session);
    if (sameLevelUnits.length === 0) {
        return;
    }
    if (sameLevelUnits.length > MAX_PIN_UNIT_COUNT) {
        showMaxPinWarning(sameLevelUnits.length);
    }
    const targetUnits = sameLevelUnits.slice(0, MAX_PIN_UNIT_COUNT);
    const { pinnedUnits } = session;
    const pinnedUnitKeys = pinnedUnits.map((item) => getAutoKey(item));
    const subtractUnits = targetUnits.reduce<InsightUnit[]>((acc, curr): InsightUnit[] => {
        const key = getAutoKey(curr);
        const findIdx = pinnedUnitKeys.findIndex((pinnedKey: string): boolean => pinnedKey === key);
        if (findIdx < 0) {
            return acc;
        }
        acc.push(curr);
        pinnedUnits.splice(findIdx, 1);
        pinnedUnitKeys.splice(findIdx, 1);
        return acc;
    }, []);
    runInAction(() => {
        session.pinnedUnits = [...pinnedUnits];
        subtractUnits.forEach((item): void => switchPinned(item));
    });
}

export const actionUnpinAll = register({
    name: 'unpinAll',
    label: 'timeline:contextMenu.Unpin All',
    visible: (session) => {
        // 为多选 unit 功能做准备
        const selectedUnitListStatus = calculateSelectedUnitListStatus(session.selectedUnits);
        return selectedUnitListStatus.isAllPinned;
    },
    perform: (session): void => {
        unpinAll(session);
    },
});

export const actionPinByUnitName = register({
    name: 'pinByUnitName',
    label: (session, t) => {
        const selectedUnitListStatus = calculateSelectedUnitListStatus(session.selectedUnits);
        return t('timeline:contextMenu.Pin by Unit Name', { name: selectedUnitListStatus.unitLevelName });
    },
    visible: (session) => {
        const selectedUnitListStatus = calculateSelectedUnitListStatus(session.selectedUnits);
        return selectedUnitListStatus.isAllUnpinned && selectedUnitListStatus.isAllLeafUnit &&
            selectedUnitListStatus.isAllVisibleUnits && selectedUnitListStatus.isAllUnmergedUnits &&
            (selectedUnitListStatus.isSameGroupNameValue || selectedUnitListStatus.isSameUnitLevelName);
    },
    perform: (session): void => {
        pinAllSameLevel(session);
    },
});

export const actionUnpinByUnitName = register({
    name: 'unpinByUnitName',
    label: (session, t) => {
        const selectedUnitListStatus = calculateSelectedUnitListStatus(session.selectedUnits);
        return t('timeline:contextMenu.Unpin by Unit Name', { name: selectedUnitListStatus.unitLevelName });
    },
    visible: (session) => {
        const selectedUnitListStatus = calculateSelectedUnitListStatus(session.selectedUnits);
        return selectedUnitListStatus.isAllPinned && selectedUnitListStatus.isAllLeafUnit &&
            selectedUnitListStatus.isAllVisibleUnits && selectedUnitListStatus.isAllUnmergedUnits &&
            (selectedUnitListStatus.isSameGroupNameValue || selectedUnitListStatus.isSameUnitLevelName);
    },
    perform: (session): void => {
        unpinAllSameLevel(session);
    },
});

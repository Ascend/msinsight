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

import { type ThreadMetaData } from '../entity/data';
import { type InsightUnit } from '../entity/insight';
import { forEach } from 'lodash';

export const getOperatingSystem = function (): string {
    const userAgent = navigator.userAgent.toLowerCase();

    if (userAgent.includes('windows')) {
        return 'Windows';
    } else if (userAgent.includes('macintosh') || userAgent.includes('mac os')) {
        return 'Mac OS';
    } else if (userAgent.includes('linux')) {
        return 'Linux';
    } else {
        return 'Unknown';
    }
};

export const getRootUnit = (units: InsightUnit[]): InsightUnit[] => {
    const result: InsightUnit[] = [];
    forEach(units, (unit) => {
        if (unit.parent) {
            if (!result.includes(unit.parent)) {
                result.push(unit.parent);
            }
        } else {
            result.push(unit);
        }
    });
    return result;
};

export const getUnitUniqueId = (unit: InsightUnit): string => {
    const { cardId = '', processId = '', threadId = '' } = unit.metadata ?? {};
    return `${cardId}_${processId}_${threadId}`;
};

export const isStreamUnit = (unit: InsightUnit): boolean => {
    const metaData = unit.metadata as ThreadMetaData;

    if (!metaData.processName || !metaData.threadName) {
        return false;
    }

    return metaData.processName.startsWith('Ascend Hardware') && metaData.threadName.startsWith('Stream');
};

export const checkIsSameUnit = (referenceMetadata?: ThreadMetaData, comparisonMetadata?: ThreadMetaData): boolean => {
    if (!referenceMetadata || !comparisonMetadata) {
        return false;
    }
    const isSameBase = referenceMetadata.processId === comparisonMetadata.processId &&
        referenceMetadata.cardId === comparisonMetadata.cardId &&
        referenceMetadata.metaType === comparisonMetadata.metaType;
    return Array.isArray(referenceMetadata.threadIdList)
        ? isSameBase && referenceMetadata.threadIdList.toString() === comparisonMetadata.threadIdList?.toString()
        : isSameBase && referenceMetadata.threadId === comparisonMetadata.threadId;
};

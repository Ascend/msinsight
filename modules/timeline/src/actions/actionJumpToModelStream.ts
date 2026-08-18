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

import { message } from 'antd';
import i18n from '@insight/lib/i18n';
import { runInAction } from 'mobx';
import { preOrderFlatten } from '../entity/common';
import type { ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';
import type { SelectedDataType, Session } from '../entity/session';
import { ThreadUnit } from '../insight/units/AscendUnit';
import { getTimeOffset } from '../insight/units/utils';
import { getRootUnit, isStreamUnit } from '../utils';
import { getSelectedDataUnit } from '../utils/selectionContext';
import { register } from './register';

const MODEL_EXECUTE = 'MODELEXECUTE';
const INVALID_MODEL_ID = '4294967295';

const normalizeTaskType = (name: unknown): string => typeof name === 'string'
    ? name.replace(/[\s_-]+/g, '').toUpperCase()
    : '';

const normalizeModelId = (value: unknown): string | undefined => {
    if (typeof value !== 'string' && typeof value !== 'number') {
        return undefined;
    }
    const modelId = String(value).trim();
    return modelId === '' || modelId === INVALID_MODEL_ID ? undefined : modelId;
};

const getModelIdFromArgs = (args: unknown): string | undefined => {
    let parsedArgs: unknown = args;
    if (typeof args === 'string') {
        try {
            parsedArgs = JSON.parse(args);
        } catch {
            return undefined;
        }
    }

    const isModelIdKey = (key: unknown): boolean => typeof key === 'string' &&
        key.replace(/[\s_-]+/g, '').toLowerCase() === 'modelid';

    if (Array.isArray(parsedArgs)) {
        for (let index = 0; index + 1 < parsedArgs.length; index += 2) {
            if (isModelIdKey(parsedArgs[index])) {
                return normalizeModelId(parsedArgs[index + 1]);
            }
        }
        return undefined;
    }

    if (typeof parsedArgs !== 'object' || parsedArgs === null) {
        return undefined;
    }
    const modelIdEntry = Object.entries(parsedArgs).find(([key]) => isModelIdKey(key));
    return normalizeModelId(modelIdEntry?.[1]);
};

const getModelStreamIds = (value: unknown): Set<string> => {
    if (!Array.isArray(value)) {
        return new Set();
    }
    return new Set(value
        .filter(item => typeof item === 'string' || typeof item === 'number')
        .map(String)
        .map(item => item.trim())
        .filter(Boolean));
};

const getLaneThreadIds = (metadata: ThreadMetaData): string[] => {
    return metadata.threadIdList?.length
        ? metadata.threadIdList
        : metadata.threadId
            ? [metadata.threadId]
            : [];
};

const getRelatedVisibleStreamUnits = (
    session: Session,
    cardId: string,
    processId: string | undefined,
    modelStreamIds: Set<string>,
): InsightUnit[] => {
    return preOrderFlatten(getRootUnit(session.units), 0).filter(unit => {
        if (!(unit instanceof ThreadUnit) || !isStreamUnit(unit) || !unit.isUnitVisible || unit.isMerged) {
            return false;
        }
        const metadata = unit.metadata as ThreadMetaData;
        return metadata.cardId === cardId && metadata.processId === processId &&
            getLaneThreadIds(metadata).some(threadId => modelStreamIds.has(String(threadId)));
    });
};

interface ModelStreamInfo {
    modelId?: string;
    streamIds: Set<string>;
}

const requestModelStreamInfo = async (
    session: Session,
    selectedData: SelectedDataType,
    metadata: ThreadMetaData,
): Promise<ModelStreamInfo> => {
    const timestampOffset = getTimeOffset(session, metadata);
    const params = {
        rankId: selectedData.cardId ?? metadata.cardId,
        dbPath: selectedData.dbPath ?? metadata.dbPath,
        metaType: selectedData.metaType ?? metadata.metaType,
        pid: selectedData.processId ?? metadata.processId,
        tid: selectedData.threadId,
        id: selectedData.id,
        startTime: Math.floor(selectedData.startTime + timestampOffset),
        depth: selectedData.depth,
        timePerPx: session.domain.timePerPx,
    };
    const result = await window.request(metadata.dataSource, { command: 'unit/threadDetail', params });
    return {
        modelId: getModelIdFromArgs(result?.data?.args),
        streamIds: getModelStreamIds(result?.data?.modelStreamIds),
    };
};

const showModelIdUnavailableWarning = (): void => {
    message.warning(i18n.t('timeline:contextMenu.Model Execute Model ID Unavailable'));
};

const jumpToModelStream = async (session: Session): Promise<void> => {
    if (session.selectedRangeIsLock) {
        return;
    }
    const selectedData = session.selectedData;
    const selectedUnit = getSelectedDataUnit(session);
    if (selectedData === undefined || !(selectedUnit instanceof ThreadUnit) || !selectedData.threadId) {
        showModelIdUnavailableWarning();
        return;
    }
    const metadata = selectedUnit.metadata as ThreadMetaData;
    const cardId = selectedData.cardId ?? metadata.cardId;
    const processId = selectedData.processId ?? metadata.processId;
    let modelStreamInfo: ModelStreamInfo;
    try {
        modelStreamInfo = await requestModelStreamInfo(session, selectedData, metadata);
    } catch {
        // window.request reports transport and server errors through errorCenter.
        return;
    }
    // Ignore a late detail response after the user has selected another slice.
    if (session.selectedData !== selectedData || getSelectedDataUnit(session) !== selectedUnit) {
        return;
    }
    if (modelStreamInfo.modelId === undefined) {
        showModelIdUnavailableWarning();
        return;
    }
    if (modelStreamInfo.streamIds.size === 0) {
        message.warning(i18n.t('timeline:contextMenu.Model Execute Stream Group Unavailable'));
        return;
    }
    const relatedUnits = getRelatedVisibleStreamUnits(
        session, cardId, processId, modelStreamInfo.streamIds);
    if (relatedUnits.length === 0) {
        message.warning(i18n.t('timeline:contextMenu.Model Execute Stream Group Unavailable'));
        return;
    }
    const selectedUnits = [selectedUnit, ...relatedUnits.filter(unit => unit !== selectedUnit)];
    const targetUnit = relatedUnits.find(unit => unit !== selectedUnit) ?? selectedUnit;

    runInAction(() => {
        session.locateUnit = {
            target: (unit): boolean => unit === targetUnit,
            onSuccess: (): void => {
                session.selectedUnits = selectedUnits;
            },
            showDetail: false,
            tuneToSelectedSlice: false,
        };
    });
};

export const actionJumpToModelStream = register({
    name: 'jumpToModelStream',
    label: 'timeline:contextMenu.Jump to Model Stream',
    visible: (session): boolean => !session.selectedRangeIsLock &&
        normalizeTaskType(session.selectedData?.name) === MODEL_EXECUTE,
    perform: (session): void => {
        void jumpToModelStream(session);
    },
});

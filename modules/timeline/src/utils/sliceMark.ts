/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 * -------------------------------------------------------------------------
 */

import { message } from 'antd';
import i18n from '@insight/lib/i18n';
import type { OpDetail } from '../api/interface';
import { runInAction } from 'mobx';
import type { ThreadMetaData } from '../entity/data';
import type { Session, SelectedDataType } from '../entity/session';
import { getTimeOffset } from '../insight/units/utils';
import jumpToUnitOperator from './jumpToUnitOperator';

export const SLICE_MARK_KEYS = ['1', '2', '3', '4', '5', '6', '7', '8', '9'] as const;
export type SliceMarkKey = typeof SLICE_MARK_KEYS[number];

export interface SliceMark {
    key: SliceMarkKey;
    name: string;
    operator: OpDetail;
}

export type SliceMarkResult = 'created' | 'jumped' | 'duplicate' | 'ignored';

type SliceMarkCreationResult =
    | { status: 'created'; mark: SliceMark }
    | { status: 'duplicate'; mark: SliceMark }
    | { status: 'ignored' };

export const isSliceMarkKey = (key: string): key is SliceMarkKey =>
    (SLICE_MARK_KEYS as readonly string[]).includes(key);

export const isEditableTarget = (target: EventTarget | null): boolean => {
    if (!(target instanceof HTMLElement)) {
        return false;
    }
    return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement || target.isContentEditable || target.contentEditable === 'true' ||
        target.getAttribute('contenteditable') === '';
};

const getMetadata = (session: Session): ThreadMetaData | undefined => {
    const metadata = session.selectedDataUnit?.metadata;
    return metadata === undefined ? undefined : metadata as ThreadMetaData;
};

const getTimestamp = (session: Session, selectedData: SelectedDataType, metadata: ThreadMetaData): number | undefined => {
    const originalTimestamp = Number(selectedData.timestamp);
    if (Number.isFinite(originalTimestamp)) {
        return originalTimestamp;
    }
    const offset = getTimeOffset(session, metadata);
    const timestamp = selectedData.startTime + offset;
    return Number.isFinite(timestamp) ? timestamp : undefined;
};

const buildSliceMark = (session: Session, key: SliceMarkKey): SliceMark | undefined => {
    const selectedData = session.selectedData;
    const metadata = getMetadata(session);
    if (selectedData === undefined || metadata === undefined) {
        return undefined;
    }

    const timestamp = getTimestamp(session, selectedData, metadata);
    const cardId = selectedData.cardId ?? metadata.cardId;
    const tid = selectedData.threadId ?? metadata.threadId;
    const pid = selectedData.processId ?? metadata.processId;
    if (timestamp === undefined || cardId === undefined || tid === undefined || pid === undefined ||
        selectedData.name === undefined || selectedData.duration === undefined || selectedData.depth === undefined) {
        return undefined;
    }

    return {
        key,
        name: selectedData.name,
        operator: {
            id: selectedData.id,
            cardId,
            dbPath: selectedData.dbPath ?? metadata.dbPath,
            tid,
            pid,
            depth: selectedData.depth,
            duration: selectedData.duration,
            name: selectedData.name,
            timestamp,
            metaType: selectedData.metaType ?? metadata.metaType,
        },
    };
};

const isSameOperator = (left: OpDetail, right: OpDetail): boolean =>
    left.id === right.id && left.cardId === right.cardId && left.dbPath === right.dbPath &&
    left.tid === right.tid && left.pid === right.pid && left.depth === right.depth &&
    left.duration === right.duration && left.name === right.name && left.timestamp === right.timestamp &&
    left.metaType === right.metaType;

const createSliceMarkResult = (session: Session, key: SliceMarkKey): SliceMarkCreationResult => {
    const mark = buildSliceMark(session, key);
    if (mark === undefined) {
        return { status: 'ignored' };
    }
    const duplicate = Array.from(session.sliceMarks.values())
        .find(existingMark => isSameOperator(existingMark.operator, mark.operator));
    if (duplicate !== undefined) {
        return { status: 'duplicate', mark: duplicate };
    }
    runInAction(() => {
        session.sliceMarks.set(key, mark);
    });
    return { status: 'created', mark };
};

export const createSliceMark = (session: Session, key: SliceMarkKey): SliceMark | undefined => {
    const result = createSliceMarkResult(session, key);
    return result.status === 'created' ? result.mark : undefined;
};

export const jumpToSliceMark = (session: Session, key: SliceMarkKey): boolean => {
    const mark = session.sliceMarks.get(key);
    if (mark === undefined) {
        return false;
    }
    jumpToUnitOperator(mark.operator);
    return true;
};

export const deleteSliceMark = (session: Session, key: SliceMarkKey): boolean => {
    let deleted = false;
    runInAction(() => {
        deleted = session.sliceMarks.delete(key);
    });
    return deleted;
};

/**
 * 当某张卡片被移除或基线被取消时，清理掉来源于这些卡片的标记，
 * 避免失效标记继续占用数字键并跳转到已不存在的数据。
 * @param session 当前会话
 * @param removedUnits 被移除单元的来源标识（cardId / dbPath）
 */
export const removeSliceMarksForUnits = (
    session: Session,
    removedUnits: Array<{ cardId?: string; dbPath?: string }>,
): void => {
    if (removedUnits.length === 0) {
        return;
    }
    const isRemoved = (operator: OpDetail): boolean => removedUnits.some(({ cardId, dbPath }) => {
        if (cardId === undefined && dbPath === undefined) {
            return false;
        }
        const cardMatched = cardId === undefined || operator.cardId === cardId;
        const dbMatched = dbPath === undefined || operator.dbPath === dbPath;
        return cardMatched && dbMatched;
    });
    runInAction(() => {
        for (const [key, mark] of session.sliceMarks) {
            if (isRemoved(mark.operator)) {
                session.sliceMarks.delete(key);
            }
        }
    });
};

export const handleSliceMarkKey = (session: Session, key: string): SliceMarkResult => {
    if (!isSliceMarkKey(key)) {
        return 'ignored';
    }
    if (session.sliceMarks.has(key)) {
        jumpToSliceMark(session, key);
        return 'jumped';
    }
    const result = createSliceMarkResult(session, key);
    if (result.status === 'duplicate') {
        message.warning(i18n.t('timeline:SliceMarkAlreadyBound', { key: result.mark.key }));
    }
    return result.status;
};

export const handleSliceMarkShortcut = (session: Session, event: KeyboardEvent): boolean => {
    if (event.defaultPrevented || event.repeat || event.ctrlKey || event.metaKey || event.altKey || event.shiftKey ||
        isEditableTarget(document.activeElement) || !isSliceMarkKey(event.key)) {
        return false;
    }
    const result = handleSliceMarkKey(session, event.key);
    if (result === 'ignored') {
        return false;
    }
    event.preventDefault();
    event.stopPropagation();
    return true;
};

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
import { KEYS } from '@insight/lib/utils';
import type { Session } from '../entity/session';
import { runInAction } from 'mobx';
import { CardMetaData, SliceData, SliceMeta, ThreadTrace } from '../entity/data';
import { getTimeOffsetKey, visitUnitTree } from '../insight/units/utils';

const setBenchmarkSlice = (session: Session): void => {
    runInAction(() => {
        session.benchMarkData = { ...session.selectedData };
    });
};

const clearBenchmarkSlice = (session: Session): void => {
    runInAction(() => {
        session.benchMarkData = undefined;
        session.alignSliceData = [];
        session.alignRender = !session.alignRender;
    });
};

const isSetBaseSliceMenuVisible = (session: Session): boolean => {
    if (session.selectedData === undefined) {
        return false;
    }
    if (session.benchMarkData === undefined) {
        return true;
    }
    const selectedData = session.selectedData;
    const benchMarkData = session.benchMarkData as ThreadTrace;
    if (selectedData.id === benchMarkData.id && selectedData.threadId === benchMarkData.threadId) {
        return false;
    }
    return true;
};

const extractDeviceId = (processId: number): number => {
    const DEVICE_ID_MASK = 0x1F;
    return processId & DEVICE_ID_MASK;
};

const updateSameDeviceOffset = (selectSliceMeta: SliceMeta, session: Session, selectOffsetKey: string, offsetDiff: number): void => {
    const deviceId = extractDeviceId(parseInt(selectSliceMeta.processId));
    const benchMeta = session.benchMarkData as SliceMeta;
    const benchKey = getTimeOffsetKey(session, benchMeta);
    session.units.forEach((unit) => {
        if ((unit.metadata as CardMetaData).cardId !== selectSliceMeta.cardId) {
            return;
        }
        visitUnitTree(unit, (item) => {
            const tempMeta = item.metadata as SliceMeta;
            if (tempMeta.label !== 'NPU') {
                return;
            }
            const key = getTimeOffsetKey(session, tempMeta);
            if (key === selectOffsetKey || key === benchKey) {
                return;
            }
            if (isNaN(Number(tempMeta.processId))) {
                return;
            }
            const tempDeviceId = extractDeviceId(parseInt(tempMeta.processId));
            if (tempDeviceId !== deviceId) {
                return;
            }
            const currentOffset = session.unitsConfig.offsetConfig.timestampOffset[key] ?? 0;
            session.unitsConfig.offsetConfig.timestampOffset[key] = currentOffset + offsetDiff;
        });
    });
    session.updateEndTimeAll();
};

const processOffsetEvent = (session: Session, isLeft: boolean): void => {
    if (session.benchMarkData === undefined || session.selectedData === undefined) {
        return;
    }
    const selectSliceMeta = session.selectedData as unknown as SliceMeta;
    const benchSliceMeta = session.benchMarkData as SliceMeta;
    const selectOffsetKey = getTimeOffsetKey(session, selectSliceMeta);
    const benchSliceOffsetKey = getTimeOffsetKey(session, benchSliceMeta);
    if (selectOffsetKey === benchSliceOffsetKey) {
        return;
    }
    let offsetDiff = 0;
    if (isLeft) {
        offsetDiff = selectSliceMeta.startTime - benchSliceMeta.startTime;
    } else {
        offsetDiff = selectSliceMeta.startTime + selectSliceMeta.duration - benchSliceMeta.startTime - benchSliceMeta.duration;
    }
    if (!isNaN(Number(selectSliceMeta.processId))) {
        updateSameDeviceOffset(selectSliceMeta, session, selectOffsetKey, offsetDiff);
    }
    const before = session.unitsConfig.offsetConfig.timestampOffset[selectOffsetKey];
    session.setTimestampOffset(selectOffsetKey, before + offsetDiff);
    runInAction(() => {
        if (session.selectedData === undefined) {
            return;
        }
        const temp = session.selectedData as unknown as SliceData;
        temp.startTime -= offsetDiff;
        const newAlignSliceData: SliceData[] = [];
        newAlignSliceData.push(temp);
        session.alignSliceData.forEach((item) => {
            const itemTemp = item as unknown as SliceMeta;
            if (itemTemp.cardId === selectSliceMeta.cardId && itemTemp.processId === selectSliceMeta.processId) {
                return;
            }
            newAlignSliceData.push(item);
        });
        session.alignSliceData = newAlignSliceData;

        session.alignRender = !session.alignRender;
    });
};

// 设置基准算子（用于其他算子与其对齐）
export const actionSetBenchmarkSlice = register({
    name: 'setBaseSlice',
    label: 'timeline:contextMenu.Set base slice',
    visible: (session): boolean => {
        return isSetBaseSliceMenuVisible(session);
    },
    perform: (session): void => {
        setBenchmarkSlice(session);
    },
});

export const actionClearBenchmarkSlice = register({
    name: 'clearBaseSlice',
    label: 'timeline:contextMenu.Clear base slice',
    visible: (session) => session.benchMarkData !== undefined,
    perform: (session): void => {
        clearBenchmarkSlice(session);
    },
});

// 与基准算子左（开始时间）对齐
export const actionAlignToBenchmarkLeft = register({
    name: 'alignToBenchmarkLeft',
    label: '',
    perform: (session): void => {
        processOffsetEvent(session, true);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.L,
});

// 与基准算子右（结束时间）对齐
export const actionAlignToBenchmarkRight = register({
    name: 'alignToBenchmarkRight',
    label: '',
    perform: (session): void => {
        processOffsetEvent(session, false);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.R,
});

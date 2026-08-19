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
import { KEYS } from '@insight/lib/utils';
import { register } from './register';
import type { Session } from '../entity/session';
import type { SliceData, SliceMeta, ThreadTrace } from '../entity/data';
import { getCardSideOffset, getOffsetSide } from '../insight/units/offset';

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
    return selectedData.id !== benchMarkData.id || selectedData.threadId !== benchMarkData.threadId;
};

export function canAlignToBenchmark(session: Session): boolean {
    if (session.benchMarkData === undefined || session.selectedData === undefined) {
        return false;
    }
    const selected = session.selectedData as unknown as SliceMeta;
    const benchmark = session.benchMarkData as SliceMeta;
    return selected.cardId !== benchmark.cardId || getOffsetSide(selected.metaType as string) !== getOffsetSide(benchmark.metaType as string);
}

export function alignToBenchmark(session: Session, isLeft: boolean): void {
    if (!canAlignToBenchmark(session)) {
        message.warning(i18n.t('timeline:contextMenu.Same Card Category Offset'));
        return;
    }
    const selected = session.selectedData as unknown as SliceMeta;
    const benchmark = session.benchMarkData as SliceMeta;
    const selectedSide = getOffsetSide(selected.metaType as string);
    const offsetDiff = isLeft
        ? selected.startTime - benchmark.startTime
        : selected.startTime + selected.duration - benchmark.startTime - benchmark.duration;
    const currentOffset = getCardSideOffset(session, selected.cardId, selectedSide);
    session.setCardSideOffset(selected.cardId, selectedSide, currentOffset + offsetDiff);

    runInAction(() => {
        if (session.selectedData === undefined) {
            return;
        }
        const aligned = session.selectedData as unknown as SliceData;
        aligned.startTime -= offsetDiff;
        session.alignSliceData = [aligned, ...session.alignSliceData.filter((item) => {
            const itemMeta = item as unknown as SliceMeta;
            return itemMeta.cardId !== selected.cardId || getOffsetSide(itemMeta.metaType as string) !== selectedSide;
        })];
        session.alignRender = !session.alignRender;
    });
}

export const actionSetBenchmarkSlice = register({
    name: 'setBaseSlice',
    label: 'timeline:contextMenu.Set base slice',
    visible: isSetBaseSliceMenuVisible,
    perform: setBenchmarkSlice,
});

export const actionClearBenchmarkSlice = register({
    name: 'clearBaseSlice',
    label: 'timeline:contextMenu.Clear base slice',
    visible: (session) => session.benchMarkData !== undefined,
    perform: clearBenchmarkSlice,
});

export const actionAlignToBenchmarkLeft = register({
    name: 'alignToBenchmarkLeft',
    label: '',
    perform: (session): void => { alignToBenchmark(session, true); },
    keyTest: (event) => event.key.toLowerCase() === KEYS.L,
});

export const actionAlignToBenchmarkRight = register({
    name: 'alignToBenchmarkRight',
    label: '',
    perform: (session): void => { alignToBenchmark(session, false); },
    keyTest: (event) => event.key.toLowerCase() === KEYS.R,
});

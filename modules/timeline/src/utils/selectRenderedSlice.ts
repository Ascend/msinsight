/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import type { ChartReaction, StackStatusData } from '../entity/chart';
import type { ThreadMetaData } from '../entity/data';
import type { Session } from '../entity/session';
import type { InsightUnit } from '../entity/insight';
import { getSelectedThreadId } from './selectionContext';
import { getOffsetSide } from '../insight/units/offset';

export const selectRenderedSlice = (
    session: Session, metadata: ThreadMetaData, slice: StackStatusData | undefined,
    onClick: ChartReaction<'stackStatus'> | undefined, unit: InsightUnit,
): void => {
    if (session.selectedRangeIsLock) return;
    if (slice) slice.showSelectedData = true;
    runInAction(() => {
        session.selectedData = slice
            ? {
                ...slice,
                threadId: getSelectedThreadId(metadata, slice.threadId),
                processId: metadata.processId ?? '',
                timestamp: slice.originalStartTime as number,
                metaType: metadata.metaType ?? '',
                offsetSide: getOffsetSide((unit.metadata as ThreadMetaData).metaType, (unit.metadata as ThreadMetaData).offsetSide),
            }
            : undefined;
        session.selectedDataUnit = slice === undefined ? undefined : unit;
        if (!session.selectedData) session.drawLineMode = 'all';
        onClick?.(slice, session, metadata);
        session.selectedRangeData = undefined;
    });
};

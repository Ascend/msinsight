/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import { runInAction } from 'mobx';
import type { NotificationHandler } from './defs';
import type { ThreadMetaData } from '../entity/data';
import { CardUnit } from '../insight/units/AscendUnit';
import { getCardSideOffset, getOffsetSide } from '../insight/units/offset';
import { store } from '../store';

/** Memory uses milliseconds relative to the selected rank; Timeline uses aligned nanoseconds. */
export const locateTimeRangeHandler: NotificationHandler = (data): void => {
    const session = store.sessionStore.activeSession;
    const { cardId, startMs, endMs, rankOffsetNs } = data;
    if (session === undefined || typeof cardId !== 'string' || cardId === '' ||
        typeof startMs !== 'number' || typeof endMs !== 'number' || typeof rankOffsetNs !== 'number' ||
        !Number.isFinite(startMs) || !Number.isFinite(endMs) || !Number.isFinite(rankOffsetNs) || endMs <= startMs) {
        return;
    }
    const offset = rankOffsetNs - getCardSideOffset(session, cardId, 'device');
    const start = startMs * 1e6 + offset;
    const end = endMs * 1e6 + offset;
    if (!Number.isFinite(start) || !Number.isFinite(end) || end <= start) { return; }
    runInAction(() => {
        session.selectedRangeIsLock = false;
        session.selectedData = undefined;
        session.selectedDataUnit = undefined;
        // Do not associate this selection with an editable Timeline range marker.
        session.timelineMaker.oldMarkedRange = undefined;
        session.selectedRange = [start, end];
        session.domainRange = { domainStart: start, domainEnd: end };
        session.locateUnit = {
            target: (unit): boolean => {
                const metadata = unit.metadata as ThreadMetaData;
                return metadata.cardId === cardId &&
                    (unit instanceof CardUnit || getOffsetSide(metadata.metaType, metadata.offsetSide) === 'device');
            },
            onSuccess: (): void => {},
            showDetail: false,
            tuneToSelectedSlice: false,
        };
    });
};

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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { runInAction } from 'mobx';
import { message } from 'antd';
import i18n from '@insight/lib/i18n';
import { register } from './register';
import type { Session } from '../entity/session';
import type { ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';
import { getTimeOffsetKey } from '../insight/units/utils';
import { queryTimelineOffset } from '../api/request';
import type {
    QueryTimelineOffsetParams,
    TimelineAlignmentType,
    TimelineOffsetItem,
} from '../api/interface';

const ALIGN_TYPE = {
    LEFT: 'LEFT',
    RIGHT: 'RIGHT',
} as const;

function getSelectedUnit(session: Session): InsightUnit | undefined {
    if (session.selectedUnits.length === 1) {
        return session.selectedUnits[0];
    }
    const selectedData = session.selectedData;
    if (!selectedData?.cardId) {
        return undefined;
    }
    if (!selectedData.processId) {
        return undefined;
    }
    return session.units.find((unit) => {
        const metadata = unit.metadata as Partial<ThreadMetaData>;
        return metadata.cardId === selectedData.cardId && metadata.processId === selectedData.processId;
    });
}

function applyOffsetItems(session: Session, items: TimelineOffsetItem[], offsetDelta: number): void {
    const nextOffset = { ...session.unitsConfig.offsetConfig.timestampOffset };
    items.forEach(({ rankId, offset, processId }) => {
        if (!rankId) {
            return;
        }
        if (!Number.isFinite(offset)) {
            return;
        }
        if (!Array.isArray(processId)) {
            return;
        }
        if (processId.length === 0) {
            return;
        }
        processId.forEach((pid) => {
            const key = getTimeOffsetKey(session, { cardId: rankId, processId: pid });
            nextOffset[key] = offset + offsetDelta;
        });
    });
    session.setTimestampOffsetAll(nextOffset);
}

function getSelectedOffsetDelta(session: Session, params: QueryTimelineOffsetParams, baseOffset: number): number {
    if (!Number.isFinite(baseOffset)) {
        return 0;
    }
    const offsetKey = getTimeOffsetKey(session, { cardId: params.rankId, processId: params.pid });
    const currentOffset = session.unitsConfig.offsetConfig.timestampOffset[offsetKey] ?? 0;
    return currentOffset - baseOffset;
}

function buildRequestParams(session: Session, alignType: TimelineAlignmentType): QueryTimelineOffsetParams | undefined {
    const selectedData = session.selectedData;
    const selectedUnit = getSelectedUnit(session);
    const metadata = selectedUnit?.metadata as Partial<ThreadMetaData> | undefined;
    const sliceName = selectedData?.name;
    const rankId = metadata?.cardId;
    const fileId = metadata?.dbPath;
    const pid = selectedData?.processId ?? metadata?.processId;
    const metaType = selectedData?.metaType ?? metadata?.metaType;
    const startTime = selectedData?.rawStartTime;
    const duration = selectedData?.duration;
    if (!sliceName) {
        return undefined;
    }
    if (!rankId) {
        return undefined;
    }
    if (!fileId) {
        return undefined;
    }
    if (!pid) {
        return undefined;
    }
    if (!metaType) {
        return undefined;
    }
    if (!startTime) {
        return undefined;
    }
    if (duration === undefined || !Number.isFinite(duration)) {
        return undefined;
    }
    return { sliceName, rankId, fileId, pid, metaType, startTime, duration, alignType };
}

async function alignByOperator(session: Session, alignType: TimelineAlignmentType): Promise<void> {
    const params = buildRequestParams(session, alignType);
    if (!params) {
        message.warning(i18n.t('timeline:contextMenu.AlignOperatorRawStartNotReady'));
        return;
    }
    const hide = message.loading(i18n.t('timeline:contextMenu.Calculating Offset'), 0);
    try {
        const res = await queryTimelineOffset(params);
        runInAction(() => {
            const offsetDelta = getSelectedOffsetDelta(session, params, res.baseOffset);
            applyOffsetItems(session, res.result, offsetDelta);
        });
    } catch {
    } finally {
        hide();
    }
}

const isOperatorAlignVisible = (session: Session): boolean => {
    if (session.isSimulation || session.isIE) {
        return false;
    }
    return Boolean(session.selectedData?.name) && getSelectedUnit(session) !== undefined;
};

export const actionAlignByOperatorLeft = register({
    name: 'alignByOperatorLeft',
    label: 'timeline:contextMenu.Align Left',
    parentMenuKey: 'alignByOperator',
    visible: isOperatorAlignVisible,
    perform: (session): void => {
        void alignByOperator(session, ALIGN_TYPE.LEFT);
    },
});

export const actionAlignByOperatorRight = register({
    name: 'alignByOperatorRight',
    label: 'timeline:contextMenu.Align Right',
    parentMenuKey: 'alignByOperator',
    visible: isOperatorAlignVisible,
    perform: (session): void => {
        void alignByOperator(session, ALIGN_TYPE.RIGHT);
    },
});

export const actionAlignByOperator = register({
    name: 'alignByOperator',
    label: 'timeline:contextMenu.Time Alignment',
    visible: isOperatorAlignVisible,
    perform: (): void => {},
    subMode: true,
    subMenus: () => [actionAlignByOperatorLeft, actionAlignByOperatorRight],
});

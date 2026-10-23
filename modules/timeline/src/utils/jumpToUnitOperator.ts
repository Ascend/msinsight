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
import { colorPalette, getTimeOffset } from '../insight/units/utils';
import type { ThreadMetaData } from '../entity/data';
import { calculateDomainRange } from '../components/CategorySearch';
import { hashToNumber } from './colorUtils';
import { ThreadUnit } from '../insight/units/AscendUnit';
import type { OpDetail } from '../api/interface';
import { store } from '../store';
import { InsightUnit } from '../entity/insight';
import { isOperatorMetadata } from './operatorUnit';
import { getSelectedThreadId } from './selectionContext';
import { getOffsetSide } from '../insight/units/offset';

interface NavigationOptions {
    guard?: (unit: InsightUnit) => boolean;
    onSuccess?: (unit: InsightUnit) => void;
    onFailure?: () => void;
}

/**
 * 在泳道中选中特定算子
 * @param {OpDetail} opDetail 算子详情信息
 */
const jumpToUnitOperator = (opDetail: OpDetail, options: NavigationOptions = {}): void => {
    const {
        id,
        cardId: cid,
        dbPath,
        tid,
        pid,
        depth,
        duration,
        name,
        timestamp,
        metaType,
    } = opDetail;
    const session = store.sessionStore.activeSession;
    if (session === undefined) { options.onFailure?.(); return; }

    runInAction(() => {
        session.locateUnit = {
            target: (unit: InsightUnit): boolean => unit instanceof ThreadUnit &&
                (!dbPath || unit.metadata.dbPath === dbPath) &&
                isOperatorMetadata(unit.metadata, { cardId: cid, pid, tid, metaType }) && (options.guard?.(unit) ?? true),
            onFailure: options.onFailure,
            onSuccess: (unit): void => {
                // 匹配和实际定位可能异步分离；副作用前重新检查调用方的导航条件。
                if (options.guard?.(unit) === false) { options.onFailure?.(); return; }
                const unitMetaData = unit.metadata as ThreadMetaData;
                const normalizedThreadId = getSelectedThreadId(unitMetaData, tid);
                const normalizedProcessId = unitMetaData.processId ?? pid;
                const normalizedCardId = unitMetaData.cardId ?? cid;
                const normalizedDbPath = unitMetaData.dbPath ?? dbPath ?? '';
                const normalizedMetaType = unitMetaData.metaType ?? metaType;
                const startTime = timestamp - getTimeOffset(session, unitMetaData);
                const [rangeStart, rangeEnd] = calculateDomainRange(session, startTime, duration);
                session.domainRange = { domainStart: rangeStart, domainEnd: rangeEnd };
                session.selectedData = {
                    id,
                    startTime,
                    name,
                    color: colorPalette[hashToNumber(name, colorPalette.length)],
                    duration,
                    depth,
                    threadId: normalizedThreadId,
                    processId: normalizedProcessId,
                    cardId: normalizedCardId,
                    dbPath: normalizedDbPath,
                    startRecordTime: session.startRecordTime,
                    showSelectedData: true,
                    metaType: normalizedMetaType,
                    offsetSide: getOffsetSide(normalizedMetaType, unitMetaData.offsetSide),
                };
                session.selectedDataUnit = unit;
                session.foregroundTarget = {
                    rankId: normalizedCardId,
                    dbPath: normalizedDbPath,
                    pid: normalizedProcessId,
                    tid: normalizedThreadId,
                    id,
                    name,
                    startTime: timestamp,
                    duration,
                    depth,
                    metaType: normalizedMetaType,
                };
                options.onSuccess?.(unit);
            },
            showDetail: false,
        };
    });
};

export default jumpToUnitOperator;

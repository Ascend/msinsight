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

const PYTHON_STACK_THREAD_ID_PREFIX = 'python_stack:';
const PYTHON_STACK_THREAD_NAME_PREFIX = 'Python Stack ';

const getPythonStackThreadId = (threadId?: string): string | undefined => {
    if (threadId === undefined) {
        return undefined;
    }
    if (threadId.startsWith(PYTHON_STACK_THREAD_ID_PREFIX)) {
        return threadId.slice(PYTHON_STACK_THREAD_ID_PREFIX.length);
    }
    if (threadId.startsWith(PYTHON_STACK_THREAD_NAME_PREFIX)) {
        return threadId.slice(PYTHON_STACK_THREAD_NAME_PREFIX.length);
    }
    return undefined;
};

const isSameThread = (unitMetaData: ThreadMetaData, tid?: string): boolean => {
    if (tid === undefined) {
        return false;
    }
    const { threadId, threadIdList, threadName } = unitMetaData;
    if (threadId === tid || threadName === tid || threadIdList?.includes(tid)) {
        return true;
    }
    const unitPythonStackThreadId = getPythonStackThreadId(threadId);
    const targetPythonStackThreadId = getPythonStackThreadId(tid) ?? tid;
    return unitPythonStackThreadId !== undefined && unitPythonStackThreadId === targetPythonStackThreadId;
};

/**
 * 在泳道中选中特定算子
 * @param {OpDetail} opDetail 算子详情信息
 */
const jumpToUnitOperator = (opDetail: OpDetail): void => {
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
    const targetMetaType = metaType === '' ? undefined : metaType;
    const session = store.sessionStore.activeSession;
    if (session === undefined) { return; }

    runInAction(() => {
        session.locateUnit = {
            target: (unit: InsightUnit): boolean => {
                if (!(unit instanceof ThreadUnit)) { return false; }

                const { cardId, processId } = unit.metadata;
                const isSameMetaType = targetMetaType === undefined || targetMetaType === unit.metadata.metaType;
                const isSameProcess = processId === pid;
                const isSameThreadResult = isSameThread(unit.metadata, tid);
                const isSameUnit = Boolean(isSameProcess && isSameThreadResult && isSameMetaType);
                if (cid && cardId) {
                    return cid === cardId && isSameUnit;
                }
                return isSameUnit;
            },
            onSuccess: (unit): void => {
                const unitMetaData = unit.metadata as ThreadMetaData;
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
                    threadId: unitMetaData.threadId,
                    processId: pid,
                    cardId: cid,
                    dbPath,
                    startRecordTime: session.startRecordTime,
                    showSelectedData: true,
                    metaType: targetMetaType ?? unitMetaData.metaType,
                };
            },
            showDetail: false,
        };
    });
};

export default jumpToUnitOperator;

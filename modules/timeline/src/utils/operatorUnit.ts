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

import type { OpDetail } from '../api/interface';
import type { ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';

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

export type OperatorUnitIdentity = Pick<OpDetail, 'cardId' | 'pid' | 'tid' | 'metaType'>;

export const isOperatorMetadata = (metadata: ThreadMetaData, opDetail: OperatorUnitIdentity): boolean => {
    const targetMetaType = opDetail.metaType === '' ? undefined : opDetail.metaType;
    const isSameMetaType = targetMetaType === undefined || targetMetaType === metadata.metaType;
    const { threadId, threadIdList, threadName } = metadata;
    const isSameThread = threadId === opDetail.tid || threadName === opDetail.tid || threadIdList?.includes(opDetail.tid) === true ||
        (getPythonStackThreadId(threadId) !== undefined &&
            getPythonStackThreadId(threadId) === (getPythonStackThreadId(opDetail.tid) ?? opDetail.tid));
    const isSameUnit = metadata.processId === opDetail.pid && isSameThread && isSameMetaType;
    if (opDetail.cardId && metadata.cardId) {
        return opDetail.cardId === metadata.cardId && isSameUnit;
    }
    return Boolean(isSameUnit);
};

export const findOperatorUnit = (units: InsightUnit[], opDetail: OperatorUnitIdentity): InsightUnit | undefined => {
    return units.find(unit => unit.name === 'Thread' && isOperatorMetadata(unit.metadata as ThreadMetaData, opDetail));
};

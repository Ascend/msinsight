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
import type { MemScopeContext } from '../state/MemScopeContext';
import { runInAction } from 'mobx';
import { workerDestroy } from '../common/leaksWorker/blockWorker/worker';
import { restoreMemoryCommon } from '../common/shared/state/reset';
const funcRestore = (session: MemScopeContext): void => {
    session.funcData = { traces: [], maxTimestamp: 0, minTimestamp: 0 };
    session.threadId = '';
    session.threadOps = [];
    session.searchFunc = [];
    session.funcOptions = [];
    session.maxDepth = 0;
    session.allowTrim = true;
};

const sliceRestore = (session: MemScopeContext): void => {
    session.memoryData = { size: 0, name: '', subNodes: [] };
    session.memoryStamp = 0;
};
export const restoreMemScope = (session: MemScopeContext): void => {
    funcRestore(session);
    sliceRestore(session);
    session.threadFlag = false;
    session.lazyUsedThreshold = { perT: null, valueT: null };
    session.delayedFreeThreshold = { perT: null, valueT: null };
    session.longIdleThreshold = { perT: null, valueT: null };
    session.onlyInefficient = false;
};
export const applyMemScopeParseCompleted = (session: MemScopeContext, data: any): void => {
    workerDestroy(session);
    runInAction(() => {
        session.deviceIds = data.deviceIds;
        session.threadIds = data.threadIds;
        session.module = data.module;
        session.dbPath = typeof data.dbPath === 'string' ? data.dbPath : '';
        session.fileHash = typeof data.fileHash === 'string' ? data.fileHash.trim() : '';
        restoreMemoryCommon(session);
        restoreMemScope(session);
    });
};

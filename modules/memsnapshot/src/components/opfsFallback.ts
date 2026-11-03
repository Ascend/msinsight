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

import { workerCheckOpfsAvailability } from '../common/leaksWorker/blockWorker/worker';
import { createOpfsFallbackGuard } from '../common/components/opfsFallbackGuard';
import { createOpfsFallbackPrompt } from '../common/components/opfsFallbackPrompt';
import type { MemSnapshotContext } from '../state/MemSnapshotContext';

export const opfsFallbackPrompt = createOpfsFallbackPrompt();

const guards = new WeakMap<MemSnapshotContext, ReturnType<typeof createOpfsFallbackGuard>>();
const getGuard = (session: MemSnapshotContext): ReturnType<typeof createOpfsFallbackGuard> => {
    let guard = guards.get(session);
    if (!guard) {
        guard = createOpfsFallbackGuard(() => workerCheckOpfsAvailability(session), opfsFallbackPrompt.requestApproval);
        guards.set(session, guard);
    }
    return guard;
};
export const ensureOpfsOrWaitForFallbackApproval = (session: MemSnapshotContext): Promise<void> => getGuard(session).ensureAvailability();
export const ensureOpfsFallbackApproval = (session: MemSnapshotContext): Promise<void> => getGuard(session).ensureFallbackApproval();

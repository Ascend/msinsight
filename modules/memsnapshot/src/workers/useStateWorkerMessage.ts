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
import { useEffect, useLayoutEffect } from 'react';
import { runInAction } from 'mobx';
import type { MemSnapshotContext as Session } from '../state/MemSnapshotContext';
import { bindStateWorkerSession, isCurrentStateWorkerGeneration, subscribeStateWorker } from '../rendering/stateWorker/worker';
// The mounted feature owns this subscription; replacing a session releases the old listener.
const useStateWorkerMessage = (session: Session | undefined): void => {
    useLayoutEffect(() => bindStateWorkerSession(session), [session]);
    useEffect(() => {
        const onMessage = (ev: MessageEvent<any>): void => {
            if (session === undefined) {
                return;
            }
            if (!isCurrentStateWorkerGeneration(ev.data.generation, session)) return;
            switch (ev.data.type) {
                case 'hoverItemResult':
                    runInAction(() => {
                        session.stateWorkerInfo.hoverItem = ev.data.result;
                    });
                    break;
                case 'clickItemResult':
                    if (ev.data.selectionVersion !== undefined && ev.data.selectionVersion !== session.selectionVersion) {
                        return;
                    }
                    runInAction(() => {
                        session.stateWorkerInfo.clickItem = ev.data.result;
                    });
                    break;
                default:
                    break;
            }
        };
        if (session) return subscribeStateWorker(session, onMessage);
    }, [session]);
};
export default useStateWorkerMessage;

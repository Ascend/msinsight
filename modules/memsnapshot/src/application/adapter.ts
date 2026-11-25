/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { runInAction } from 'mobx';
import { type MemoryApplicationAdapter } from '../common/application/notifications';
import { workerDestroy } from '../common/leaksWorker/blockWorker/worker';
import { resetMemoryApplication } from '../common/shared/state/reset';
import { createSession, type MemSnapshotSession } from './session';
import * as snapshot from '../notifications/controller';
import { workerDestroy as stateWorkerDestroy } from '../rendering/stateWorker/worker';

export const createAdapter = (): MemoryApplicationAdapter<MemSnapshotSession> => {
    const session = createSession();
    const dispose = (): void => { workerDestroy(session); stateWorkerDestroy(session); };
    return {
        session,
        requests: ['language', 'theme', 'directory', 'memSnapshotSession'],
        dispose,
        reset: () => {
            dispose();
            runInAction(() => {
                resetMemoryApplication(session);
                snapshot.removeMemSnapshot(session);
            });
        },
        reconcile: data => { snapshot.reconcileMemSnapshotSession(session, data); },
        isParsing: () => snapshot.isMemSnapshotParsing(session),
        events: {
            'parse/leaksMemoryCompleted': data => {
                if (data.module === 'memsnapshot') snapshot.parseCompletedHandler(session, data);
            },
            'frame/loaded': data => { snapshot.frameLoadedHandler(session, data); },
            switchDirectory: data => { snapshot.switchDirectoryHandler(session, data); },
            'remote/import': data => { snapshot.importRemoteHandler(session, data); },
            'parse/memSnapshotProgress': data => { snapshot.parseProgressHandler(session, data); },
            'parse/memSnapshotSliceReady': data => { snapshot.parseSliceReadyHandler(session, data); },
        },
    };
};

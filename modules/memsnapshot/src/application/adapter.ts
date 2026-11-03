/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
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

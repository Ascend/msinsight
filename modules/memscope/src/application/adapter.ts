/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { runInAction } from 'mobx';
import { type MemoryApplicationAdapter } from '../common/application/notifications';
import { workerDestroy } from '../common/leaksWorker/blockWorker/worker';
import { resetMemoryApplication } from '../common/shared/state/reset';
import { createSession, type MemScopeSession } from './session';
import { applyMemScopeParseCompleted, restoreMemScope } from '../notifications/controller';

export const createAdapter = (): MemoryApplicationAdapter<MemScopeSession> => {
    const session = createSession();
    const dispose = (): void => { workerDestroy(session); };
    return {
        session,
        requests: ['language', 'theme', 'fileHash', 'module', 'deviceIds', 'dbPath', 'directory', 'threadIds'],
        dispose,
        reset: () => {
            dispose();
            runInAction(() => {
                resetMemoryApplication(session);
                session.threadIds = [];
                session.loadingFunc = false;
                restoreMemScope(session);
            });
        },

        events: {
            'parse/leaksMemoryCompleted': data => {
                if (data.module !== 'memsnapshot') applyMemScopeParseCompleted(session, data);
            },
        },
    };
};
